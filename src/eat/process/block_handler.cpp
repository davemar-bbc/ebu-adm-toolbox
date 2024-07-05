#include <iostream>
#include <type_traits>
#include <adm/adm.hpp>
#include <adm/utilities/id_assignment.hpp>
//#include <adm/utilities/block_duration_assignment.hpp>
#include "eat/process/block_handler.hpp"

#include <adm/route_tracer.hpp>
#include <adm/utilities/time_conversion.hpp>
#include <adm/helper/get_optional_property.hpp>

using namespace adm;

namespace eat::process {

void BlockModify::modifyBlocks(uint64_t file_length_ns) {
  // TODO:
  // * Need to look at how some parameters (e.g. position) might need interpolation after the block's size has changed.

  // Get the maximum ID values for audioPackFormat and audioCHannelFormat
  getMaxIds();

  // Duplicate any audioPackFormat and audioChannelFormats if an audioObject has been split.
  duplicatePacks();

  // Go through each audioObject
  for (auto ao : adm_->getElements<AudioObject>()) {
    auto apfs = ao->getReferences<AudioPackFormat>();

    if (apfs.size() > 0) {
      block_object_.getNewTimes(ao, track_objects_);

      for (auto apf : apfs) {  // Usually expect only one audioPackFormat here
        bool apf_remove = true;
        // Go through all the audioChannelFormats
        for (auto acf : apf->getReferences<AudioChannelFormat>()) {

          // Check if this channel is referenced from other audioObjects if it's getting removed
          bool shared_acf = findSharedChannelFormat(ao, acf);

          if (!block_object_.remove_ || shared_acf) {
            BlockHandler block_handler(block_object_);

            // Each block_handler needs a different call for each typeDefinition
            if (acf->get<AudioChannelFormatId>().get<TypeDescriptor>() == TypeDefinition::DIRECT_SPEAKERS) {
              block_handler.handleChannel<AudioBlockFormatDirectSpeakers>(acf);
            }
            if (acf->get<AudioChannelFormatId>().get<TypeDescriptor>() == TypeDefinition::MATRIX) {
              block_handler.handleChannel<AudioBlockFormatMatrix>(acf);
            }
            if (acf->get<AudioChannelFormatId>().get<TypeDescriptor>() == TypeDefinition::OBJECTS) {
              block_handler.handleChannel<AudioBlockFormatObjects>(acf);
              block_handler.fixInterpolationLengths(acf);
            }
            if (acf->get<AudioChannelFormatId>().get<TypeDescriptor>() == TypeDefinition::HOA) {
              block_handler.handleChannel<AudioBlockFormatHoa>(acf);
            }
            if (acf->get<AudioChannelFormatId>().get<TypeDescriptor>() == TypeDefinition::BINAURAL) {
              block_handler.handleChannel<AudioBlockFormatBinaural>(acf);
            }
            apf_remove = false;   // We have to keep this audioPackFormat
          } else {  // Remove the channel and blocks as they aren't used
            removeChannelStreamTrack(acf);
          }
        }
        if (apf_remove) {  // Remove unused audioPackFormat
          auto apf_id = apf->template get<AudioPackFormatId>();
          if (!isCommonDefinitionsId(apf_id)) {
            adm_->remove(apf);
          }
        }
      }
    }
  }
}


bool BlockModify::findSharedChannelFormat(std::shared_ptr<AudioObject> ao,
                                          std::shared_ptr<AudioChannelFormat> &acf) {
  bool shared_acf = false;
  if (block_object_.remove_) {
    for (auto ao_other : adm_->getElements<AudioObject>()) {
      if (ao_other->get<AudioObjectId>() != ao->get<AudioObjectId>()) {
        auto apfs_other = ao_other->getReferences<AudioPackFormat>();
        if (apfs_other.size() > 0) {
          auto acfs_other = apfs_other[0]->getReferences<AudioChannelFormat>();
          for (auto acf_other : acfs_other) {
            if (acf->get<AudioChannelFormatId>() == acf_other->get<AudioChannelFormatId>()) {
              shared_acf = true;
            }
          }
        }
      }
    }
  }
  return shared_acf;
}


void BlockModify::removeChannelStreamTrack(std::shared_ptr<AudioChannelFormat> &acf) {
  auto acf_id = acf->template get<AudioChannelFormatId>();
  std::shared_ptr<AudioStreamFormat> asf_rem = nullptr;
  std::shared_ptr<AudioTrackFormat> atf_rem = nullptr;
  if (!isCommonDefinitionsId(acf_id)) {
    for (auto asf : adm_->getElements<AudioStreamFormat>()) {
      auto acf_ref = asf->getReference<AudioChannelFormat>();
      if (acf_ref) {
        if (acf_ref->get<AudioChannelFormatId>() == acf->get<AudioChannelFormatId>()) {
          asf_rem = asf;
          for (auto atf : adm_->getElements<AudioTrackFormat>()) {
            auto asf_ref = atf->getReference<AudioStreamFormat>();
            if (asf_ref) {
              if (asf_ref->get<AudioStreamFormatId>() == asf->get<AudioStreamFormatId>()) {
                atf_rem = atf;
              }
            }
          }
        }
      }
    }
    if (atf_rem != nullptr) adm_->remove(atf_rem);
    if (asf_rem != nullptr) adm_->remove(asf_rem);
    adm_->remove(acf);
  }
}


void BlockModify::getMaxIds() {
  // Get the maximum audioPackFormat ID value
  max_apf_idv_ = AudioPackFormatIdValue(0);
  auto apfs = adm_->template getElements<AudioPackFormat>();
  for (auto apf : apfs) {
    auto apf_id = apf->template get<AudioPackFormatId>();
    if (!isCommonDefinitionsId(apf_id)) {
      auto apf_idv = apf_id.template get<AudioPackFormatIdValue>();
      if (apf_idv > max_apf_idv_) {
        max_apf_idv_ = apf_idv;
      }
    }
  }

  // Get the maximum audioChannelFormat ID value
  max_acf_idv_ = AudioChannelFormatIdValue(0);
  auto acfs = adm_->template getElements<AudioChannelFormat>();
  for (auto acf : acfs) {
    auto acf_id = acf->template get<AudioChannelFormatId>();
    if (!isCommonDefinitionsId(acf_id)) {
      auto acf_idv = acf_id.template get<AudioChannelFormatIdValue>();
      if (acf_idv > max_acf_idv_) {
        max_acf_idv_ = acf_idv;
      }
    }
  }
}


void BlockModify::duplicatePacks() {
  // Go through each track_object to find which audioObjects have been split into multiple ones.
  for (auto track_object : track_objects_) {
    // Generate a list of audioObject IDs that would now reference packs.
    std::vector<std::string> ao_ids;
    if (track_object.newAudioObjectIdsSize() > 0) {
      for (size_t i = 0; i < track_object.newAudioObjectIdsSize(); i++) {
        ao_ids.push_back(track_object.newAudioObjectId(i));
      }
    } else {
      ao_ids.push_back(track_object.getAudioObjectId());
    }

    // Get the audioPackFormat reference for the first audioObject, and make this the pack we'll copy from.
    std::shared_ptr<AudioPackFormat> apf_ref = nullptr;
    auto ao_id1 = ao_ids[0];
    for (auto ao : adm_->getElements<AudioObject>()) {
      if (ao_id1 == formatId(ao->get<AudioObjectId>())) {
        auto apfs = ao->getReferences<AudioPackFormat>();
        apf_ref = apfs[0]; // Assuming only one audioPackFormat is used
      }
    }

    // Go through the other audioObject IDs (if they exist) for duplicating packs and channels.
    uint32_t apf_ind = 2;  // audioPackFormatName index
    for (uint32_t i = 1; i < ao_ids.size(); i++) {
      auto ao_id = ao_ids[i];
      for (auto ao : adm_->getElements<AudioObject>()) {
        if (ao_id == formatId(ao->get<AudioObjectId>())) {
          if (!isCommonDefinitionsId(apf_ref->get<AudioPackFormatId>())) {
            // Duplicate apr_ref to new audioPackFormat
            auto apf_new = copyPack(apf_ref, apf_ind);
            apf_ind++;

            // Remove the old pack reference from the audioObject
            auto apfs = ao->getReferences<AudioPackFormat>();
            for (auto apf : apfs) {
              ao->removeReference(apf);
            }
            // Add the new pack reference to the audioObject
            ao->addReference(apf_new);

            modifyTrackUIDs(ao, apf_new);
          }
        }
      }
    }
  }
}


void BlockModify::modifyTrackUIDs(std::shared_ptr<AudioObject> &ao, std::shared_ptr<AudioPackFormat> apf_new) {
  auto ao_aturefs = ao->getReferences<AudioTrackUid>();
  auto acf_news = apf_new->getReferences<AudioChannelFormat>();
  assert(ao_aturefs.size() == acf_news.size());  // These should always be the same

  for (int32_t i = 0; i < static_cast<int32_t>(acf_news.size()); i++) {
    auto acf_new = acf_news[i];

    // Generate the audioTrackFormat from the new audioChannelFormats
    std::shared_ptr<AudioTrackFormat> atf_new;

    // Add new references to audioTrackUID
    ao_aturefs[i]->setReference(acf_new);
    ao_aturefs[i]->setReference(apf_new);
  }
}


void BlockModify::generateStreamTrackFormats(std::shared_ptr<AudioChannelFormat> acf_new,
                                             std::shared_ptr<AudioTrackFormat> &atf_new) {
  auto acf_new_name = acf_new->template get<AudioChannelFormatName>().get();
  auto acf_new_id_val = acf_new->get<AudioChannelFormatId>().get<AudioChannelFormatIdValue>().get();
  auto acf_type = acf_new->get<TypeDescriptor>();

  // Make audioStreamFormat
  auto asf_new_name = std::string("PCM_") + acf_new_name;
  auto asf_new_id_val = AudioStreamFormatIdValue(acf_new_id_val);

  auto asf_new = AudioStreamFormat::create(
      AudioStreamFormatName(asf_new_name), FormatDefinition::PCM);
  auto asf_new_id = AudioStreamFormatId(acf_type, asf_new_id_val);
  asf_new->set(asf_new_id);
  asf_new->setReference(acf_new);

  // Make audioTrackFormat
  auto atf_new_name = std::string("PCM_") + acf_new_name;
  auto atf_new_id_val = AudioTrackFormatIdValue(acf_new_id_val);
  AudioTrackFormatIdCounter atf_new_count(1);

  atf_new = AudioTrackFormat::create(
      AudioTrackFormatName(atf_new_name), FormatDefinition::PCM);
  auto atf_new_id = AudioTrackFormatId(acf_type, atf_new_id_val, atf_new_count);
  atf_new->set(atf_new_id);
  atf_new->setReference(asf_new);
}


std::shared_ptr<AudioPackFormat> BlockModify::copyPack(std::shared_ptr<AudioPackFormat> apf_ref, uint32_t apf_ind) {
  auto apf_new = apf_ref->copy();
  auto apf_new_id = apf_new->template get<AudioPackFormatId>();
  max_apf_idv_++;   // Increment the maximum audioPackFormat ID
  // Change the ID to the new maximum value
  apf_new_id.set(max_apf_idv_);
  apf_new->set(apf_new_id);

  // Change the name to add an index
  auto apf_new_name = apf_new->template get<AudioPackFormatName>().get();
  apf_new_name += "_" + std::to_string(apf_ind);
  apf_new->set(AudioPackFormatName(apf_new_name));

  // Copy the audioChannelFormats
  for (auto acf_ref : apf_ref->getReferences<AudioChannelFormat>()) {
    auto acf_new = acf_ref->copy();
    auto acf_new_id = acf_new->template get<AudioChannelFormatId>();
    max_acf_idv_++;  // Increment the maximum audioChannelFormat ID
    // Change the ID to the new maximum value
    acf_new_id.set(max_acf_idv_);
    acf_new->set(acf_new_id);

    // Change the name to add an index
    auto acf_new_name = acf_new->template get<AudioChannelFormatName>().get();
    acf_new_name += "_" + std::to_string(apf_ind);
    acf_new->set(AudioChannelFormatName(acf_new_name));

    apf_new->addReference(acf_new);  // Add the audioChannelFormat to the new audioPackFormat
  }

  return apf_new;
}



void BlockObject::getNewTimes(std::shared_ptr<AudioObject> ao, std::vector<TrackObject> track_objects) {
  // Find the track_object entry that matches the audioObject (ao)
  TrackObject track_object_m;
  for (auto track_object : track_objects) {
    if (track_object.checkObjectMatch(ao)) {
      track_object_m = track_object;
    }

    if (track_object.newAudioObjectIdsSize() > 0) {
      for (size_t i = 0; i < static_cast<size_t>(track_object.newAudioObjectIdsSize()); i++) {
        auto naoi = track_object.newAudioObjectId(i);
        if (naoi == formatId(ao->get<AudioObjectId>())) {
          track_object_m = track_object;
        }
      }
    }
  }

  // Initialise the start and end times
  old_start_ = track_object_m.start(0);
  old_end_ = track_object_m.end(0);
  new_start_ = std::chrono::nanoseconds(0);
  new_end_ = std::chrono::nanoseconds(0);
  if (ao->has<Start>()) {
    new_start_ = ao->get<Start>().get().asNanoseconds();
  }
  if (ao->has<Duration>()) {
    new_end_ = ao->get<Duration>().get().asNanoseconds() + new_start_;
  }

  track_object_m.print();

  remove_ = track_object_m.getRemove();
}


template<typename T>
void BlockHandler::handleChannel(std::shared_ptr<AudioChannelFormat> &acf) {
  std::vector<T> new_abfs;
  auto abfs = acf->getElements<T>();
  if (abfs.size() == 0) {
    return;
  }

  handleBlocks(abfs, new_abfs);
  acf->clearAudioBlockFormats();
  for (auto new_abf : new_abfs) {
    acf->add(new_abf);
  }
}


template<typename T1, typename T2>
void BlockHandler::handleBlocks(T1 &abfs, std::vector<T2> &new_abfs) {
  new_off_start_ = block_object_.getNewOffStart();
  new_off_end_ = block_object_.getNewOffEnd();

  buildBlockInfo(abfs);
  adjustBlockInfo();
  setNewBlocks(abfs, new_abfs);
}


template<typename T>
void BlockHandler::buildBlockInfo(T &abfs) {
  for (auto abf : abfs) {
    BlockInfo block_info;

    block_info.block_start = std::chrono::nanoseconds(0);
    if (abf.template has<Rtime>()) {
      block_info.block_start = abf.template get<Rtime>().get().asNanoseconds();
    }

    block_info.block_end = block_object_.getBlockEnd();
    block_info.inf_end = true;
    if (abf.template has<Duration>()) {
      block_info.block_end = abf.template get<Duration>().get().asNanoseconds() + block_info.block_start;
      block_info.inf_end = false;
    }
    block_info.keep = true;

    block_infos_.push_back(block_info);
  }
}


void BlockHandler::adjustBlockInfo() {
  for (uint32_t i = 0; i < block_infos_.size(); i++) {
    if (block_infos_[i].block_end <= new_off_start_ && !block_infos_[i].inf_end) {
      // Discard the block
      block_infos_[i].keep = false;
    } else if (block_infos_[i].block_start > new_off_end_) {
      // Discard the block
      block_infos_[i].keep = false;
    } else {
      // Trim the start of the block
      if (block_infos_[i].block_start < new_off_start_) {
        block_infos_[i].block_start = new_off_start_;
      }
      // Trim the end of the block
      if (block_infos_[i].block_end > new_off_end_ && !block_infos_[i].inf_end) {
        block_infos_[i].block_end = new_off_end_;
      }
    }
  }
}


template<typename T1, typename T2>
void BlockHandler::setNewBlocks(T1 &abfs, std::vector<T2> &new_abfs) {
  uint32_t first_block = 0;
  while (first_block < abfs.size() && !block_infos_[first_block].keep) {
    first_block++;
  }

  uint32_t last_block = abfs.size();
  do {
    last_block--;
  } while (last_block > 0 && !block_infos_[last_block].keep);

  for (uint32_t i = first_block; i <= last_block; i++) {
    std::chrono::nanoseconds new_rtime = std::chrono::nanoseconds(0);
    std::chrono::nanoseconds new_duration = std::chrono::nanoseconds(0);

    new_rtime = block_infos_[i].block_start - block_infos_[first_block].block_start;
    new_duration = block_infos_[i].block_end - block_infos_[i].block_start;

    if (first_block > 0) {
      // Generate a new zero-duration first block.
      if (i == first_block) {
        T2 new_abf = abfs[i];
        new_abf = abfs[i];
        auto abf_counter = AudioBlockFormatIdCounter(1);
        auto abf_id = new_abf.template get<AudioBlockFormatId>();
        abf_id.set(abf_counter);
        new_abf.set(abf_id);
        new_abf.set(Rtime(std::chrono::nanoseconds(0)));
        new_abf.set(Duration(std::chrono::nanoseconds(0)));
        new_abfs.push_back(new_abf);
      }
      { // keeping the scope of new_abf
        T2 new_abf = abfs[i];
        new_abf = abfs[i];
        auto abf_counter = AudioBlockFormatIdCounter(i - first_block + 2);
        auto abf_id = new_abf.template get<AudioBlockFormatId>();
        abf_id.set(abf_counter);
        new_abf.set(abf_id);
        new_abf.set(Rtime(new_rtime));
        if (!block_infos_[i].inf_end) {
          new_abf.set(Duration(new_duration));
        } else {
          new_abf.template unset<Duration>();
          new_abf.template unset<Rtime>();
        }
        new_abfs.push_back(new_abf);
      }
    } else {  // first_block == 0
      T2 new_abf = abfs[i];
      auto abf_counter = AudioBlockFormatIdCounter(1);
      auto abf_id = new_abf.template get<AudioBlockFormatId>();
      abf_id.set(abf_counter);
      new_abf.set(Rtime(new_rtime));
      if (!block_infos_[i].inf_end) {
        new_abf.set(Duration(new_duration));
      } else {
        new_abf.template unset<Duration>();
        new_abf.template unset<Rtime>();
      }
      new_abfs.push_back(new_abf);
    }
  }

  // Prevents a audioChannelFormat without any audioBlockFormats appearing
  if (first_block > last_block) { 
    T2 new_abf = abfs[0];
    new_abf = abfs[0];
    auto abf_counter = AudioBlockFormatIdCounter(1);
    auto abf_id = new_abf.template get<AudioBlockFormatId>();
    abf_id.set(abf_counter);
    new_abf.set(abf_id);
    // Make it unbounded
    new_abf.template unset<Duration>();
    new_abf.template unset<Rtime>();
    new_abfs.push_back(new_abf);
  }
}


void BlockHandler::fixInterpolationLengths(std::shared_ptr<AudioChannelFormat> &acf) {
  auto abfs = acf->getElements<AudioBlockFormatObjects>();
  for (auto &abf : abfs) {
    auto jump_position = abf.get<JumpPosition>();
    if (jump_position.get<JumpPositionFlag>() == true) {
      auto int_len = jump_position.get<InterpolationLength>().get();
      auto duration = abf.template get<Duration>().get().asNanoseconds();
      if (duration == std::chrono::nanoseconds(0)) {
        jump_position.unset<JumpPositionFlag>();
        jump_position.unset<InterpolationLength>();
      } else {
        if (duration < int_len) {
          jump_position.set(InterpolationLength(duration));
        } 
        // If below 1ms, then get rid of interpolation length
        if (int_len < std::chrono::nanoseconds(10000000)) {
          jump_position.unset<InterpolationLength>();
        }
      }
      abf.set(jump_position);
    }
  }
}

} // namespace: eat::process
