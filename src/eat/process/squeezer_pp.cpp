#include "eat/process/squeezer_pp.hpp"
#include "eat/process/sub_document.hpp"
#include "eat/process/tag_levels.hpp"

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

class SqueezerProdProf : public StreamingAtomicProcess {
 public:
  SqueezerProdProf(const std::string &name, size_t block_size, uint16_t lev_num_in)
      : StreamingAtomicProcess(name),
        block_size_(block_size),
        lev_num_in_(lev_num_in),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples")) {}

  void initialise() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();
    channel_map_ = adm.channel_map;

    // Read the tagList for the levels
    auto levels = buildLevelList(doc);

    // Select the chosen level
    std::shared_ptr<Level> lev_ptr;
    for (auto level : levels) {
      if (level.lev_num == lev_num_in_) {
        lev_ptr = std::make_shared<Level>(level);
        break;
      }
    }
    if (lev_ptr == nullptr) throw std::runtime_error("level number does not exist");

    // Create the new output ADM document
    createNewDoc(lev_ptr);

    // Go through each audioObject at the chosen level
    size_t track_num = 0;
    for (auto object : lev_ptr->object_list) {
      // Create a sub-document from the audioObject downwards from the main document
      SubDocument sub_document;
      auto layout = sub_document.create(doc, channel_map_, object); 

      // Add the sub-document to an ADMData so it can be rendered
      ADMData local_adm;
      local_adm.document = sub_document.getDoc();
      local_adm.channel_map = sub_document.getChannelMap();

      // Add the new ADM metadata for the rendered version of the audioObject to the new document
      addNewDoc(local_adm.document.read(), lev_ptr->programme_list, layout, track_num);
      auto num_tracks = layout.channels().size();

      // Prepare the renderer for this object
      std::shared_ptr<BasicRenderer> basic_render;
      basic_render = std::make_shared<BasicRenderer>(local_adm, layout, block_size_);
      basic_render->initialise();
      basic_renders.push_back(basic_render);

      // Increment the track number counter
      track_num += num_tracks;
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
      for (auto basic_render : basic_renders) {
        auto info_out = info;

        info_out.channel_count = basic_render->num_channels();
        auto out_block = std::make_shared<InterleavedSampleBlock>(info_out);

        basic_render->process(in_block, out_block);

        // Append the output of the renderer to the combined output
        tot_block->append(*out_block);
      }
      out_samples->push(std::move(tot_block));
    }
    // Deal with the final frame
    if (in_samples->eof() && !out_samples->eof_triggered()) {
      auto info_tot = core_info_;
      info_tot.channel_count = 0;
      auto tot_block = std::make_shared<InterleavedSampleBlock>(info_tot);
      for (auto basic_render : basic_renders) {
        auto info_out = core_info_;
        info_out.channel_count = basic_render->num_channels();
        auto out_block = std::make_shared<InterleavedSampleBlock>(info_out);

        basic_render->finalise(out_block);

        // Append the output of the renderer to the combined output
        tot_block->append(*out_block);
      }
      out_samples->push(std::move(tot_block));
      out_samples->close();
    }
  }

 private:

  // Create the new output ADM document
  void createNewDoc(std::shared_ptr<Level> lev_ptr) {
    new_doc_ = Document::create();
    addCommonDefinitionsTo(new_doc_); 
    new_doc_->set(adm::Version{"ITU-R_BS.2076-3"});
    Profile profile{
      ProfileValue{"ITU-R BS.2168-0"},
      ProfileName{"AdvSS Emission ADM and S-ADM Profile"},
      ProfileVersion{"1.0"},
      ProfileLevel{"2"},
    };
    auto profile_list = std::make_shared<ProfileList>() ;
    profile_list->add(profile);
    new_doc_->add(profile_list);

    for (auto programme : lev_ptr->programme_list) {
      auto new_programme = programme->copy();
      for (auto content_ref : programme->getReferences<AudioContent>()) {
        new_doc_->add(new_programme);
      }
    }
  }

  // Add the new elements for the new document
  void addNewDoc(std::shared_ptr<const Document> sub_doc, std::vector<std::shared_ptr<AudioProgramme>> programme_list,
                 const ear::Layout layout, size_t track_num) {
    auto pack_format_id = audioPackFormatLookupTable().at(layout.name());
    auto new_pack_format = new_doc_->lookup(pack_format_id);

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
    }

    std::map<std::shared_ptr<AudioProgramme>, std::shared_ptr<AudioProgramme>> programme_map;
    for (auto programme : programme_list) {
      for (auto &new_programme : new_doc_->getElements<AudioProgramme>()) {
        if (new_programme->get<AudioProgrammeId>() == programme->get<AudioProgrammeId>()) {
          programme_map[programme] = new_programme;
        }
      }
    }
    for (auto &[programme, new_programme] : programme_map) {
      allocateContent(new_programme, programme);    
    }
  }

  // Add references to the audioProgrammes
  void allocateContent(std::shared_ptr<AudioProgramme> new_programme,
                       std::shared_ptr<AudioProgramme> programme) {
    for (auto content_ref : programme->getReferences<AudioContent>()) {
      for (auto new_content : new_doc_->getElements<AudioContent>()) {
        if (new_content->get<AudioContentId>() == content_ref->get<AudioContentId>()) {
          new_programme->addReference(new_content);
        }
      }
    }
  }
 
 private:
  size_t block_size_;
  uint16_t lev_num_in_;
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  StreamPortPtr<InterleavedBlockPtr> out_samples;

  channel_map_t channel_map_;
  std::vector<std::shared_ptr<BasicRenderer>> basic_renders;

  std::shared_ptr<Document> new_doc_;
  channel_map_t new_channel_map_;

  BlockDescription core_info_;
  bool first_;
};

framework::ProcessPtr make_squeezer_prod_prof(const std::string &name, size_t block_size, uint16_t lev_num_in) {
  return std::make_shared<SqueezerProdProf>(name, block_size, lev_num_in);
}

}  // namespace eat::process
