#include "eat/process/track_analyser.hpp"

#include <iostream>
#include <math.h>
#include <bw64/bw64.hpp>

#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"

using namespace bw64;
using namespace eat::framework;

namespace eat::process {

class TrackAnalyser : public StreamingAtomicProcess {
 public:
  TrackAnalyser(const std::string &name)
      : StreamingAtomicProcess(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_block_active(add_out_port<DataPort<BlockActive>>("out_blocks")) {
  }

  void initialise() override {
    num_ch = 0;
    samples = 0;
  }

  void process() override {
    const float thresh = 5.0e-7; // Not zero to account for dither noise

    while (in_samples->available()) {
      auto in_block = in_samples->pop().read();
      auto &info = in_block->info();
      num_ch = info.channel_count;

      std::vector<bool> en_bool;
      for (uint32_t j = 0; j < num_ch; j++) {
        float en = 0.0;
        for (uint32_t i = 0; i < info.sample_count * num_ch; i += num_ch) {
          en += in_block->data()[i + j] * in_block->data()[i + j];
        }
        en = sqrt(en / static_cast<float>(info.sample_count));
        en_bool.push_back((en) > thresh ? true : false);
        /*if (j == 11) {
          std::cout << en << " ";
        }*/
      }
      //std::cout << en_bool[11] << std::endl;
      block_active.active.push_back(en_bool);
      block_active.block_times.push_back(static_cast<int64_t>(1.0e9 * static_cast<double>(samples) / 48000.0));
      samples += static_cast<uint64_t>(info.sample_count);
    }
    //std::cout << "TrackAnalyser:process: " << block_active.active.size() << "," << block_active.active[0].size() << std::endl;
  }

  void finalise() override {
    std::vector<bool> en_bool;
    for (uint32_t j = 0; j < num_ch; j++) {
      en_bool.push_back(false);
    }
    block_active.active.push_back(en_bool);
    block_active.block_times.push_back(static_cast<int64_t>(1.0e9 * static_cast<double>(samples) / 48000.0));
    block_active.file_length_ns = static_cast<uint64_t>(1.0e9 * static_cast<double>(samples) / 48000.0);

    out_block_active->set_value(std::move(block_active));
  }

 private:
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  DataPortPtr<BlockActive> out_block_active;
  BlockActive block_active;
  uint32_t num_ch;
  uint64_t samples;
};

framework::ProcessPtr make_track_analyser(const std::string &name) {
  return std::make_shared<TrackAnalyser>(name);
}

//-----------------------------------------------------------------------------

class BlockPrint : public StreamingAtomicProcess {
 public:
  BlockPrint(const std::string &name)
      : StreamingAtomicProcess(name),
        in_block_active(add_in_port<DataPort<BlockActive>>("in_blocks")) {
  }

  void process() override {
    auto block_active = std::move(in_block_active->get_value());

    for (uint32_t i = 0; i < block_active.active.size(); i++) {
      for (uint32_t j = 0; j < block_active.active[i].size(); j++) {
        std::cout << block_active.active[i][j] << " ";
      }
      std::cout << std::endl;
    }
  }

  void finalise() override {
  }

 private:
  DataPortPtr<BlockActive> in_block_active;
};

framework::ProcessPtr make_block_print(const std::string &name) {
  return std::make_shared<BlockPrint>(name);
}

}  // namespace eat::process

