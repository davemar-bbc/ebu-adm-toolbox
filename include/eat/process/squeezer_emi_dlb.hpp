#pragma once

#include <adm/adm.hpp>
#include <bw64/bw64.hpp>
#include "eat/process/adm_bw64.hpp"
#include "eat/framework/process.hpp"

using namespace bw64;
using namespace adm;

namespace eat::process {

/// a process which measures the loudness of input samples
/// - in_axml (DataPort<ADMData>) : input axml
/// - out_axml (DataPort<ADMData>) : output axml
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
/// - out_samples (StreamPort<InterleavedBlockPtr>) : output samples
framework::ProcessPtr make_squeezer_emi_dlb(const std::string &name, size_t block_size);

}  // namespace eat::process

