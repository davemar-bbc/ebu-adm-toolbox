#pragma once

#include "eat/framework/process.hpp"
#include "eat/framework/value_ptr.hpp"
#include "eat/process/block.hpp"

namespace eat::process {
using namespace eat::framework;

struct PaConfigPlay {
  int device_num;
  std::size_t channel_count;
  std::size_t frames_per_buffer;
  uint32_t sample_rate;
};


/// send samples to an audio device
///
/// ports:
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
///
/// @param device_num_  Output device number 
framework::ProcessPtr make_playback_audio(const std::string &name, const int device_num_,
                                          const int frames_per_buffer_, const uint32_t sample_rate_);

}  // namespace eat::process
