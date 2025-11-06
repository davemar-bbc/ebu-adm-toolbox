#pragma once
#include <vector>
#include <cstdint>
#include <vector>
#include <bw64/bw64.hpp>

#include "eat/framework/process.hpp"

namespace eat::process {

/// 
/// ports:
/// - in_sadm (StreamPort<std::string>) : input S-ADM frame
framework::ProcessPtr make_sadm_output(const std::string &name);

/// 
/// ports:
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input audio frame
framework::ProcessPtr make_audio_frame_output(const std::string &name);

}  // namespace eat::process
