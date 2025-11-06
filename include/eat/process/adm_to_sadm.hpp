#pragma once
#include <vector>
#include <cstdint>
#include <vector>
#include <bw64/bw64.hpp>

#include "eat/framework/process.hpp"

namespace eat::process {

/// Converts an ADM document into a stream of S-ADM frames
/// 
/// ports:
/// - in_axml (DataPort<ADMData>) :  input axml data
/// - in_length (DataPort<uint64_t>) : input file length
/// - out_sadm (StreamPort<std::string>) : output S-ADM frame
framework::ProcessPtr make_adm_to_sadm(const std::string &name, std::chrono::nanoseconds frame_size_);

}  // namespace eat::process
