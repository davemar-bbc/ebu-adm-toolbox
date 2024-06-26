#pragma once

#include <bw64/bw64.hpp>
#include "eat/framework/process.hpp"

using namespace bw64;

struct BlockActive {
  std::vector<std::vector<bool>> active;  // 2D vector of activity flags
  std::vector<int64_t> block_times;      // vector of block times
  uint64_t file_length_ns;
};

namespace eat::process {
/// a process which measures the loudness of input samples
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
/// - out_block_active (DataPort<BlockActive>) : block info
framework::ProcessPtr make_track_analyser(const std::string &name);

/// a process which measures the loudness of input samples
/// - in_block_active (DataPort<BlockActive>) : block info
framework::ProcessPtr make_block_print(const std::string &name);

}  // namespace eat::process

