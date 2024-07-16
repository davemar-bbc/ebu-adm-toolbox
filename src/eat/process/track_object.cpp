#include <iostream>
#include <adm/adm.hpp>
#include <bw64/bw64.hpp>
#include <adm/utilities/copy.hpp>
#include "eat/process/track_object.hpp"
#include "eat/process/chna.hpp"

using namespace adm;
using namespace bw64;

namespace eat::process {


TrackObject::TrackObject() {
  remove_ = false; 
  prod_prof_limits_.obj_min_gap_ns = 1000e6;
  prod_prof_limits_.obj_pre_gap_ns = 20e6;
  prod_prof_limits_.obj_post_gap_ns =  20e6;
  prod_prof_limits_.obj_min_dur_ns = 100e6;
  prod_prof_limits_.obj_max_gap_ns = 5000e6;
}


void TrackObject::getAdmInfo(std::shared_ptr<const AudioObject> ao,
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
  for (uint32_t i = 0; i < starts_.size(); i++) {
    std::cout << starts_[i].count() / 1e6 << "/";
    std::cout << ends_[i].count() / 1e6 << " ";
  }
  std::cout << "| ";
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
    for (auto ti : track_info_) {
      if (block_active.active[i][ti.track_index]) {
        start_tot++;
      }
    }
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


void TrackObject::getStartsAndEnds(BlockActive block_active) {
  std::vector<int64_t> new_starts;
  std::vector<int64_t> new_ends;

  // Find the first block_time from object start
  uint32_t i = 0;
  do {
    i++;
  } while (start(0).count() >= block_active.block_times[i] &&
          i < block_active.block_times.size() - 1);

  do {
    int64_t new_start = 0;
    int64_t new_end = 0;
    // Find start
    bool start_found = findStart(i, new_start, block_active);

    // Move the start back by 20ms
    new_start -= prod_prof_limits_.obj_pre_gap_ns;
    if (new_start < 0) new_start = 0;

    // Find end
    bool end_found = false;
    if (start_found) {
      end_found = findEnd(i, new_end, block_active);
    }

    // Move the end forward by 20ms
    new_end += prod_prof_limits_.obj_post_gap_ns;

    // Make the duration at least 100ms
    if (new_end - new_start < prod_prof_limits_.obj_min_dur_ns) {
      new_end = new_start + prod_prof_limits_.obj_min_dur_ns;
    }

    // If start and ends found add the times to the vectors
    if (start_found) {
      new_starts.push_back(new_start);
    }
    if (end_found) {
      new_ends.push_back(new_end);
    }

    i++;
    // Finish loop when reached original end point for object
  } while (i < block_active.block_times.size() - 1 &&
          end(0).count() >= block_active.block_times[i - 1]);

  // If the end of blocks are reached without the last new_ends set, then set a new new_ends
  if (new_ends.size() < new_starts.size()) {
    new_ends.push_back(end(0).count());
  }
  assert(new_ends.size() == new_starts.size());  // Just to make sure they are the same!

  mergeCloseObjects(new_starts, new_ends, prod_prof_limits_.obj_min_gap_ns);

  for (auto new_start : new_starts) {
    addNewStart(new_start);
  }
  for (auto new_end : new_ends) {
    addNewEnd(new_end);
  }

  // Ensure starts and ends aren't beyond the orignal audioObject
  if (starts_[1] < starts_[0]) starts_[1] = starts_[0];
  if (ends_.back() > ends_[0]) ends_.back() = ends_[0];
}


void TrackObject::mergeCloseObjects(std::vector<int64_t> &new_starts, 
                                    std::vector<int64_t> &new_ends, int64_t min_gap_ns) {
  uint32_t j = 0;
  // Double loop as more than 2 objects might need merging into 1.
  while (j < new_starts.size() - 1) {
    while (j < new_starts.size() - 1) {
      // If objects within min gap, then merge start and ends
      if (new_ends[j] + min_gap_ns > new_starts[j + 1]) {
        new_ends[j] = new_ends[j + 1];
        new_ends.erase(new_ends.begin() + j + 1);
        new_starts.erase(new_starts.begin() + j + 1);
      } else break;
    }
    j++;
  }
}

}  // namespace eat::process
