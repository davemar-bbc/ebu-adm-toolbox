#pragma once

#include "eat/framework/process.hpp"
#include "eat/framework/value_ptr.hpp"
#include "eat/process/block.hpp"

namespace eat::process {
using namespace eat::framework;

struct PaConfig {
  uint32_t device_num;
  std::size_t channel_count;
  std::size_t frames_per_buffer;
  uint32_t sample_rate;
};


/// read samples from an audio device
///
/// ports:
/// - out_samples (StreamPort<InterleavedBlockPtr>) : output samples
///
/// @param device_num_  Input device number 
framework::ProcessPtr make_capture_audio(const std::string &name, const uint32_t device_num_,
                                         const int frames_per_buffer_, const uint32_t sample_rate_);

}  // namespace eat::process
