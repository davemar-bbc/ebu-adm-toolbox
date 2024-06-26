#include <iostream>
#include <adm/adm.hpp>
#include <bw64/bw64.hpp>
#include <adm/utilities/copy.hpp>
#include "eat/process/track_object.hpp"
#include "eat/process/chna.hpp"

using namespace adm;
using namespace bw64;

namespace eat::process {

void TrackObject::getAdmInfo(std::shared_ptr<const AudioObject> ao, /*std::shared_ptr<ChnaChunk> chna_chunk,*/
                             channel_map_t channel_map,
                             BlockActive block_active) {
  // Set the audioObject ID, start and end times in track_object
  audio_object_id_ = formatId(ao->get<AudioObjectId>());
  starts_.push_back(std::chrono::nanoseconds(0));
  if (ao->has<Start>()) {
    starts_[0] = ao->get<Start>().get().asNanoseconds();
  }
  ends_.push_back(std::chrono::nanoseconds(block_active.file_length_ns));
  if (ao->has<Duration>()) {
    ends_[0] = ao->get<Duration>().get().asNanoseconds() + starts_[0];
  } 

  auto ao_aturefs = ao->getReferences<AudioTrackUid>();
  // Set the audioTrackUIDs and track indices in track_object
  for (auto ao_aturef : ao_aturefs) {
    TrackInfo ti;

    ti.audio_track_uid = formatId(ao_aturef->get<AudioTrackUidId>());
    ti.track_index = channel_map[ao_aturef->get<AudioTrackUidId>()];
    track_info_.push_back(ti);
  }
}


void TrackObject::print() {
  std::cout << "trackObject: ";
  std::cout << audio_object_id_ << " ";
  std::cout << starts_[0].count() / 1e9 << " ";
  std::cout << ends_[0].count() / 1e9 << " ";
  for (auto ti : track_info_) {
    std::cout << ti.track_index << " ";
    std::cout << ti.audio_track_uid << " ";
  }
  std::cout << (remove_ ? "Rem " : "Ok ");
  std::cout << std::endl;
}


bool TrackObject::findStart(uint32_t &i, int64_t &new_start, BlockActive block_active) {
  bool start_found = false;
  do {
    uint32_t start_tot = 0;
    //std::cout << "start: " << i << ": ";
    for (auto ti : track_info_) {
      //std::cout << block_active.active[i][ti.track_index];
      if (block_active.active[i][ti.track_index]) {
        start_tot++;
      }
    }
    //std::cout << " = " << start_tot << std::endl;
    i++;
    if (start_tot > 0) {
      new_start = block_active.block_times[i - 1];
      start_found = true;
      break;
    }
  } while (i < block_active.block_times.size() - 1 && 
           ends_[0].count() >= block_active.block_times[i - 1]);
  return start_found;
}


bool TrackObject::findEnd(uint32_t &i, int64_t &new_end, BlockActive block_active) {
  bool end_found = false;
  do {
    uint32_t end_tot = 0;
    for (auto ti : track_info_) {
      if (!block_active.active[i][ti.track_index]) {
        end_tot++;
      }
    }
    i++;
    if (end_tot == track_info_.size()) {
      new_end = block_active.block_times[i - 1];
      end_found = true;
      break;
    }
  } while (i < block_active.block_times.size() - 1 && 
           ends_[0].count() >= block_active.block_times[i - 1]);
           
  if (ends_[0].count() < block_active.block_times[i - 1] && !end_found) {
    new_end = ends_[0].count();
    end_found = true;       
  }
  return end_found;
}


uint32_t TrackObject::findMaxTrack() {
  uint32_t max_track_index = 0;
  for (auto ti : track_info_) {
    if (ti.track_index > max_track_index) {
      max_track_index = ti.track_index;
    }
  }
  return max_track_index;
}


TrackInfo TrackObject::getTrackInfo(const uint32_t tind) {
  TrackInfo ti_ret = {"", 0};
  for (auto ti : track_info_) {
    if (ti.track_index == tind) {
      ti_ret = ti;
    }
  }
  return ti_ret;
}

}  // namespace eat::process
