#include "eat/process/object_modifier.hpp"

#include <iostream>
#include <adm/utilities/copy.hpp>

#include "eat/process/adm_bw64.hpp"
#include "eat/process/track_analyser.hpp"
#include "eat/process/track_object.hpp"
#include "eat/process/block_handler.hpp"
#include "eat/process/validate.hpp"  // Not sure why this is needed for ADMData.

using namespace bw64;
using namespace adm;
using namespace eat::framework;

namespace eat::process {

class ObjectModifier : public FunctionalAtomicProcess {
 public:
  ObjectModifier(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        in_block_active(add_in_port<DataPort<BlockActive>>("in_blocks")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    adm_ = adm::deepCopy(adm.document.read());
    adm_->set(Version("ITU-R_BS.2076-2"));
    channel_map_ = adm.channel_map;
    auto block_active = std::move(in_block_active->get_value());

    analyseAdmChna(block_active);
    shrinkObjects(block_active);
    makeNewObjects();

    BlockModify block_modify(adm_, track_objects_);
    block_modify.modifyBlocks(block_active.file_length_ns);

    removeObjects();

    adm.document = std::move(adm_);
    adm.channel_map = channel_map_;
    out_axml->set_value(std::move(adm));
  }

 private:
  void analyseAdmChna(BlockActive block_active) {
    // Loop through each audioObject
    for (auto ao : adm_->getElements<AudioObject>()) {
      // Get the audioTrackUID references in the audioObject
      auto ao_aturefs = ao->getReferences<AudioTrackUid>();

      if (ao_aturefs.size() > 0) {  // Only include audioObjects with audioTrackUid refs.
        TrackObject track_object;
        track_object.getAdmInfo(ao, channel_map_, block_active);

        track_objects_.push_back(track_object);
      }
    }
  }

  void shrinkObjects(BlockActive block_active) {
    for (auto &track_object : track_objects_) {
      track_object.getStartsAndEnds(block_active);
    }
  }

  void makeNewObjects() {
    // Find the highest audioObject ID number
    uint32_t max_ao_id = getMaxObjectId();

    // Find the highest audioTrackUID number
    uint32_t max_atu_id = getMaxUidId();

    std::cout << "makeNewObjects: " << max_ao_id << " " << max_atu_id << std::endl;

    for (auto track_object : track_objects_) {
      std::cout << "makeNewObjects: startSize=" << track_object.startSize() << " : ";
      track_object.print();
      // Deal with existing object and new start and duration times
      for (auto &ao : adm_->getElements<AudioObject>()) {
        if (track_object.checkObjectMatch(ao)) {
          if (track_object.startSize() == 2) {  // If it's the only object needed, just modify it.
            // Index zero is the old one, so use index 1 for first new one
            Start start(track_object.start(1));
            ao->set(start);

            Duration duration(track_object.end(1) - track_object.start(1));
            ao->set(duration);
          }
        }
      }
    }

    // Generate new audioObjects
    for (auto &track_object : track_objects_) {
      if (track_object.startSize() > 2) {
        // Copy the existing audioObject as we'll need to copy stuff from it.
        std::shared_ptr<AudioObject> existing_ao;
        for (auto &ao : adm_->getElements<AudioObject>()) {
          if (track_object.checkObjectMatch(ao)) {
            existing_ao = ao;
          }
        }
        max_ao_id++;

        // Go though the new objects
        std::vector<std::shared_ptr<AudioObject>> new_aos; // List of new audioObjects for parent audioObject;
        for (uint32_t i = 1; i < track_object.startSize(); i++) {
          auto new_ao = addNewObject(track_object, existing_ao, max_ao_id, max_atu_id, i);
          // Add reference to this new audioObject in the parent audioObject
          adm_->add(new_ao);
          new_aos.push_back(new_ao);
        }
        makeNewParentObject(existing_ao, new_aos);
      }
      // If there's no objects in the track 
      if (track_object.startSize() == 1) {
        for (auto &ao : adm_->getElements<AudioObject>()) {
          if (track_object.checkObjectMatch(ao)) {
            track_object.setRemove();
          }
        }
      }
    }
  }

  uint32_t getMaxObjectId() {
    uint32_t max_ao_id = 0;
    for (auto ao : adm_->getElements<AudioObject>()) {
      auto ao_id_str = formatId(ao->get<AudioObjectId>()).substr(3);
      uint32_t ao_id = std::stoul(ao_id_str, nullptr, 16);
      if (ao_id > max_ao_id) {
        max_ao_id = ao_id;
      }
    }
    return max_ao_id;
  }


  uint32_t getMaxUidId() {
    uint32_t max_atu_id = 0;
    for (auto atu : adm_->getElements<AudioTrackUid>()) {
      auto atu_id_str = formatId(atu->get<AudioTrackUidId>()).substr(4);
      uint32_t atu_id = std::stoul(atu_id_str, nullptr, 16);
      if (atu_id > max_atu_id) {
        max_atu_id = atu_id;
      }
    }
    return max_atu_id;
  }

  void makeNewParentObject(std::shared_ptr<AudioObject> &existing_ao,
                           std::vector<std::shared_ptr<AudioObject>> new_aos) {
    std::shared_ptr<AudioObject> new_parent_ao = existing_ao;

    // Clear some parameters that are not needed in a parent object
    existing_ao->clearReferences<AudioTrackUid>();
    existing_ao->clearReferences<AudioPackFormat>();
    existing_ao->unset<Start>();
    existing_ao->unset<Duration>();

    // Add the new audioObject references for the new children
    for (auto new_ao : new_aos) {
      existing_ao->addReference(new_ao);
    }
  }

  std::shared_ptr<AudioObject> addNewObject(TrackObject &track_object,
                              std::shared_ptr<AudioObject> existing_ao,
                              uint32_t &max_ao_id, uint32_t &max_atu_id, const uint32_t ind) {
    std::shared_ptr<AudioObject> new_ao = existing_ao->copy();

    // Set new ID
    auto new_ao_id_value = AudioObjectIdValue(max_ao_id);
    auto new_ao_id = new_ao->template get<AudioObjectId>();
    new_ao_id.set(new_ao_id_value);
    new_ao->set(new_ao_id);
    //track_object.new_audio_object_ids_.push_back(formatId(new_ao_id));
    track_object.addNewAudioObjectId(formatId(new_ao_id));

    // Set new name
    auto new_ao_name = new_ao->template get<AudioObjectName>().get();
    new_ao_name = new_ao_name + std::string("_") + std::to_string(ind);
    new_ao->set(AudioObjectName(new_ao_name));

    // Set new start and duration
    Start start(track_object.start(ind));
    new_ao->set(start);
    Duration duration(track_object.end(ind) - track_object.start(ind));
    new_ao->set(duration);

    // And audioPackFormat
    auto apfs = existing_ao->getReferences<AudioPackFormat>();
    new_ao->addReference(apfs[0]);

    // Setup new audioTrackUIDRefs
    auto ao_aturefs = existing_ao->getReferences<AudioTrackUid>();
    for (uint32_t atu_num = 0; atu_num < ao_aturefs.size(); atu_num++) {
      auto ao_aturef = ao_aturefs[atu_num];
      addNewTrackUID(ao_aturef, max_atu_id, new_ao, atu_num);
    }
    max_ao_id++;

    return new_ao;
  }


  void addNewTrackUID(const std::shared_ptr<AudioTrackUid> ao_aturef, uint32_t &max_atu_id,
                      std::shared_ptr<AudioObject> &new_ao, uint32_t atu_num) {
    auto new_atu = ao_aturef->copy();
    // Set new ATU id
    max_atu_id++;
    auto new_atu_id_value = AudioTrackUidIdValue(max_atu_id);
    auto new_atu_id = new_atu->template get<AudioTrackUidId>();
    new_atu_id.set(new_atu_id_value);
    new_atu->set(new_atu_id);


    auto apfs = new_ao->getReferences<AudioPackFormat>();
    for (auto apf : apfs) {  // should only be 1 apf
      auto acfs = apf->getReferences<AudioChannelFormat>();
      
      if (acfs[atu_num] != nullptr) new_atu->setReference(acfs[atu_num]);

      new_atu->setReference(apf);
    }

    new_ao->removeReference(ao_aturef);
    new_ao->addReference(new_atu);

    adm_->add(new_atu);

    // Find chna track
    channel_map_[new_atu_id_value] = channel_map_[ao_aturef->get<AudioTrackUidId>()];
    //addChnaTrack(ao_aturef, new_atu_id);
  }


  void removeObjects() {
    for (auto &track_object : track_objects_) {
      std::shared_ptr<AudioObject> ao_rem = nullptr;
      for (auto &ao : adm_->getElements<AudioObject>()) {
        if (track_object.checkObjectMatch(ao)) {
          if (track_object.getRemove()) {
            ao_rem = ao;
          }
        }
      }
      if (ao_rem != nullptr) {
        auto atus = ao_rem->getReferences<AudioTrackUid>();
        for (auto atu : atus) {
          adm_->remove(atu);
        }
        adm_->remove(ao_rem);
      }
    }
  }  

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<BlockActive> in_block_active;
  DataPortPtr<ADMData> out_axml;

  std::vector<TrackObject> track_objects_;   // A vector of the track_objects that will get populated
  std::shared_ptr<adm::Document> adm_;
  channel_map_t channel_map_;
};

framework::ProcessPtr make_object_modifier(const std::string &name) {
  return std::make_shared<ObjectModifier>(name);
}

}  // namespace eat::process
