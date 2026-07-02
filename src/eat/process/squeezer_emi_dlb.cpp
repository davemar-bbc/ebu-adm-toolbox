#include "eat/process/squeezer_emi_dlb.hpp"
#include "eat/process/sub_document.hpp"

#include <adm/adm.hpp>
#include <bw64/bw64.hpp>
#include <adm/utilities/copy.hpp>
#include <adm/write.hpp>
#include <adm/common_definitions.hpp>

#include "eat/process/adm_bw64.hpp"
#include "eat/process/block_handler.hpp"
#include "eat/process/block.hpp"
#include "eat/render/render_basic.hpp"

#include <iostream>

using namespace bw64;
using namespace adm;
using namespace eat::framework;
using namespace eat::render;

namespace eat::process {

struct RenderMono {
  std::shared_ptr<BasicRenderer> basic_render;
  bool mono;
};

struct ObjectLevel {
  std::shared_ptr<AudioContent> new_content;
  std::shared_ptr<AudioObject> new_object;
  std::vector<std::shared_ptr<AudioObject>> object_refs;
};

class SqueezerEmiDlb : public StreamingAtomicProcess {
 public:
  SqueezerEmiDlb(const std::string &name, size_t block_size)
      : StreamingAtomicProcess(name),
        block_size_(block_size),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples")) {}

  void initialise() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();
    channel_map_ = adm.channel_map;

    // Create the new output ADM document
    createNewDoc(doc->getElements<AudioProgramme>());

    auto orig_objects = doc->getElements<AudioObject>();

    std::vector<ObjectLevel> object_levels;
    auto bed_content = AudioContent::create(AudioContentName("Bed"));
    auto bed_object = AudioObject::create(AudioObjectName("Bed"));
    std::vector<std::shared_ptr<AudioObject>> object_bed_refs;
    for (auto object : orig_objects) {
      bool bed = false;
      for (auto pack_ref : object->getReferences<AudioPackFormat>()) {
        if (pack_ref->get<AudioPackFormatId>().get<TypeDescriptor>() == TypeDefinition::DIRECT_SPEAKERS) {
          auto chan_refs = pack_ref->getReferences<AudioChannelFormat>();
          if (chan_refs.size() > 2) {
            bed = true;
          }
        }
      }
      if (bed) {
        object_bed_refs.push_back(object);
      }
    }
    {
      ObjectLevel object_level{bed_content, bed_object, object_bed_refs};
      object_levels.push_back(object_level);
    }

    uint16_t co_obj = 1;
    for (auto object : orig_objects) {
      bool mono = false;
      std::string mono_name;
      for (auto pack_ref : object->getReferences<AudioPackFormat>()) {
        auto chan_refs = pack_ref->getReferences<AudioChannelFormat>();
        if (chan_refs.size() <= 2) {
          mono = true;
          mono_name = object->get<AudioObjectName>().get();
        }
      }
      if (mono) {
        auto name = std::string("D_") + std::to_string(co_obj);
        co_obj++;
        auto mono_object = AudioObject::create(AudioObjectName(mono_name));
        auto mono_content = AudioContent::create(AudioContentName(mono_name));
        std::vector<std::shared_ptr<AudioObject>> object_mono_refs = {object};
        ObjectLevel object_level{mono_content, mono_object, object_mono_refs};
        object_levels.push_back(object_level);
      }
    }

    for (auto &object_level : object_levels) {
      doc->add(object_level.new_object);
      doc->add(object_level.new_content);
      for (auto &object_ref: object_level.object_refs) {
        object_level.new_object->addReference(object_ref);
      }
      object_level.new_content->addReference(object_level.new_object);
    }

    for (auto &object_level : object_levels) {
      doc->add(object_level.new_object);
      for (auto &object_ref: object_level.object_refs) {
        object_level.new_object->addReference(object_ref);
      }
    }

    std::stringstream xmlStream;
    writeXml(xmlStream, doc);
    std::cout << xmlStream.str();

    // Go through each audioObject at the chosen level
    size_t track_num = 0;
    for (auto object_level : object_levels) {
      for (auto content : doc->getElements<AudioContent>()) {
        std::cout << "initialise: cont: " << formatId(content->get<AudioContentId>()) << std::endl;
      }

      // Create a sub-document from the audioObject downwards from the main document
      SubDocument sub_document;
      auto max_chans = sub_document.create(doc, channel_map_, object_level.new_object); 

      if (max_chans > 8) max_chans = 8;  // Set to 8 for the emission profile for now.
      auto layout = chooseLayout(max_chans);
      auto num_chans = layoutChans(layout);
      if (num_chans > max_chans) num_chans = max_chans;

      // Add the sub-document to an ADMData so it can be rendered
      ADMData local_adm;
      local_adm.document = sub_document.getDoc();
      local_adm.channel_map = sub_document.getChannelMap();

      // Add the new ADM metadata for the rendered version of the audioObject to the new document
      addNewDoc(local_adm.document.read(), doc->getElements<AudioProgramme>(), layout, track_num, num_chans);

      // Prepare the renderer for this object
      RenderMono render_mono;
      std::shared_ptr<BasicRenderer> basic_render;
      render_mono.basic_render = std::make_shared<BasicRenderer>(local_adm, layout, block_size_);
      render_mono.basic_render->initialise();
      render_mono.mono = (max_chans == 1); // Set to mono if num_chans is one (renderer can't do mono)
      render_monos.push_back(render_mono);

      // Increment the track number counter
      track_num += num_chans;
    }

    // Output the new document
    ADMData new_adm;
    new_adm.document = new_doc_;
    new_adm.channel_map = new_channel_map_;
    out_axml->set_value(std::move(new_adm));

    adm.channel_map = channel_map_;
    first_ = true;
  }

  void process() override {
    while (in_samples->available()) {
      auto in_block = in_samples->pop().read();
      auto &info = in_block->info();
      // Store the block info for sample rate and block size
      if (first_) {
        core_info_ = info;
        first_ = false;
      }

      // Set up block to contain the combined output of the renderers
      auto info_tot = info;
      info_tot.channel_count = 0;
      auto tot_block = std::make_shared<InterleavedSampleBlock>(info_tot);

      // Loop through each renderer
      for (auto render_mono : render_monos) {
        auto info_out = info;
        renderMix(render_mono.basic_render, render_mono.mono, in_block, tot_block, info_out, false);
      }
      out_samples->push(std::move(tot_block));
    }
    // Deal with the final frame
    if (in_samples->eof() && !out_samples->eof_triggered()) {
      auto info_tot = core_info_;
      info_tot.channel_count = 0;
      auto tot_block = std::make_shared<InterleavedSampleBlock>(info_tot);
      for (auto render_mono : render_monos) {
        auto info_out = core_info_;
        renderMix(render_mono.basic_render, render_mono.mono, nullptr, tot_block, info_out, true);
      }
      out_samples->push(std::move(tot_block));
      out_samples->close();
    }
  }

 private:

  // Create the new output ADM document
  void createNewDoc(ElementRange<AudioProgramme> programmes) {
    new_doc_ = Document::create();
    addCommonDefinitionsTo(new_doc_); 
    new_doc_->set(adm::Version{"ITU-R_BS.2076-3"});

    for (auto programme : programmes) {
      auto new_programme = programme->copy();
      new_doc_->add(new_programme);
    }
  }

  // Add the new elements for the new document
  void addNewDoc(std::shared_ptr<const Document> sub_doc, ElementRange<AudioProgramme> programmes,
                 const ear::Layout layout, size_t track_num, size_t max_chans) {
    auto pack_format_id = audioPackFormatLookupTable().at(layout.name());
    auto new_pack_format = new_doc_->lookup(pack_format_id);
    if (max_chans == 1) { // As the renderer can't handle mono, the layout will be wrong
      generateMonoObject(new_pack_format);
    }

    for (auto content : sub_doc->getElements<AudioContent>()) {
      auto new_content = content->copy();
      for (auto object_ref : content->getReferences<AudioObject>()) {
        auto new_object = object_ref->copy();
        new_object->addReference(new_pack_format);
        for (auto channel_format : new_pack_format->getReferences<AudioChannelFormat>()) {
          auto track_uid = AudioTrackUid::create();
          track_uid->setReference(channel_format);
          track_uid->setReference(new_pack_format);
          new_channel_map_.emplace(track_uid->get<AudioTrackUidId>(), track_num);
          track_num++;
          new_doc_->add(track_uid);
          new_object->addReference(track_uid);
        }
        new_content->addReference(new_object);
        new_doc_->add(new_object);
      }
      new_doc_->add(new_content);
      std::cout << "addNewDoc: conts: " << formatId(content->get<AudioContentId>()) << " " << formatId(new_content->get<AudioContentId>()) << std::endl;
    }

    for (auto &new_programme : new_doc_->getElements<AudioProgramme>()) {
      for (auto new_content : new_doc_->getElements<AudioContent>()) {
        new_programme->addReference(new_content);
      }
    }
  
    /*std::map<std::shared_ptr<AudioProgramme>, std::shared_ptr<AudioProgramme>> programme_map;
    for (auto programme : programmes) {
      for (auto &new_programme : new_doc_->getElements<AudioProgramme>()) {
        std::cout << "addNewDoc: " << formatId(programme->get<AudioProgrammeId>()) << " " << formatId(new_programme->get<AudioProgrammeId>()) << std::endl;
        if (new_programme->get<AudioProgrammeId>() == programme->get<AudioProgrammeId>()) {
          programme_map[programme] = new_programme;
        }
      }
    }
    for (auto &[programme, new_programme] : programme_map) {
      allocateContent(new_programme, programme);    
    }*/
  }

  // Add references to the audioProgrammes
  void allocateContent(std::shared_ptr<AudioProgramme> new_programme,
                       std::shared_ptr<AudioProgramme> programme) {
    for (auto content_ref : programme->getReferences<AudioContent>()) {
      for (auto new_content : new_doc_->getElements<AudioContent>()) {
        std::cout << "allocateContent: " << formatId(content_ref->get<AudioContentId>()) << " " << formatId(new_content->get<AudioContentId>()) << std::endl;
        if (new_content->get<AudioContentId>() == content_ref->get<AudioContentId>()) {
          new_programme->addReference(new_content);
        }
      }
    }
  }

  void generateMonoObject(std::shared_ptr<AudioPackFormat> &new_pack_format) {
    auto name = std::string("Mono_") + std::to_string(mono_co_);
    mono_co_++;
    auto new_channel_format = AudioChannelFormat::create(
            AudioChannelFormatName(name), TypeDefinition::OBJECTS);
    CartesianPosition pos(X(0.0), Y(1.0), Z(0.0));
    AudioBlockFormatObjects block_format(pos, Cartesian(true));
    new_channel_format->add(block_format);
    new_pack_format = AudioPackFormat::create(
            AudioPackFormatName(name), TypeDefinition::OBJECTS);
    new_pack_format->addReference(new_channel_format);
    new_doc_->add(new_channel_format);
    new_doc_->add(new_pack_format);
  }

  // Selects the layout that matches the number of channels
  ear::Layout chooseLayout(size_t max_chans) {
    // This subset should suit fussy profiles
    std::vector<ear::Layout> layouts{ear::getLayout("0+2+0"),
                                     ear::getLayout("0+5+0"),
                                     ear::getLayout("4+5+0")}; 
    ear::Layout layout = ear::getLayout("0+2+0");
    for (const auto& layoutl : /*ear::loadLayouts()*/ layouts) {
      if (layoutl.channels().size() <= max_chans) {
        layout = layoutl;
      }
    }
    return layout;
  }

  size_t layoutChans(ear::Layout layout) {
    return layout.channels().size();
  }

  void renderMix(std::shared_ptr<BasicRenderer> basic_render, 
                 bool mono, std::shared_ptr<const InterleavedSampleBlock> in_block,
                 std::shared_ptr<InterleavedSampleBlock> tot_block,
                 BlockDescription &info_out, bool final) {
    info_out.channel_count = basic_render->num_channels();
    auto out_block = std::make_shared<InterleavedSampleBlock>(info_out);

    if (final) {
      basic_render->finalise(out_block);
    } else {
      basic_render->process(in_block, out_block);
    }
    if (mono) { // Mix down to mono if only 1 channel need
      auto mono_block = out_block->mono_mix(0.5);
      tot_block->append(mono_block);
    } else {
      // Append the output of the renderer to the combined output
      tot_block->append(*out_block);
    }
  }

 private:
  size_t block_size_;
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  StreamPortPtr<InterleavedBlockPtr> out_samples;

  channel_map_t channel_map_;
  std::vector<std::shared_ptr<BasicRenderer>> basic_renders;
  std::vector<bool> monos;
  std::vector<RenderMono> render_monos;

  std::shared_ptr<Document> new_doc_;
  channel_map_t new_channel_map_;

  BlockDescription core_info_;
  bool first_;
  uint16_t mono_co_;
};

framework::ProcessPtr make_squeezer_emi_dlb(const std::string &name, size_t block_size) {
  return std::make_shared<SqueezerEmiDlb>(name, block_size);
}

}  // namespace eat::process
