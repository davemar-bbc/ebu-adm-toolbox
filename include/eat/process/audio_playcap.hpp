#pragma once

#include "eat/framework/process.hpp"
#include "eat/framework/value_ptr.hpp"
#include "eat/process/block.hpp"

namespace eat::process {
using namespace eat::framework;

struct PaConfigPC {
  uint32_t device_num;
  std::size_t channel_count;
  std::size_t frames_per_buffer;
  uint32_t sample_rate;
};


/// read and play samples from an audio device
///
/// ports:
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
/// - out_samples (StreamPort<InterleavedBlockPtr>) : output samples
///
/// @param device_num_  Input device number 
framework::ProcessPtr make_playcap_audio(const std::string &name, const uint32_t device_num_,
                                         const int frames_per_buffer_, const uint32_t sample_rate_);


/// audio straight through
///
/// ports:
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
/// - out_samples (StreamPort<InterleavedBlockPtr>) : output samples
///
/// @param device_num_  Input device number 
framework::ProcessPtr make_audio_through(const std::string &name);

}  // namespace eat::process
