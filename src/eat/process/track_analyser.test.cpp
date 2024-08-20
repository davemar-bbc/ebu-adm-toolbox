#include "eat/process/track_analyser.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <bw64/bw64.hpp>

#include "eat/framework/evaluate.hpp"
#include "eat/framework/utility_processes.hpp"
#include "eat/process/block.hpp"


using namespace eat::framework;
using namespace eat::process;

TEST_CASE("read samples") {
  Graph g;

  const std::size_t duration = 1000;
  const std::size_t block_size = 100;
  const std::size_t channels = 2;

  // Generate a test signal with some silent parts
  std::vector<float> input;
  for (auto i = 0; i != duration; ++i) {
    for (auto ch = 0; ch < channels; ch++) {
      if (ch % 2 == 0) {
        if (i < duration / 4) input.push_back(0.0f);
        else input.push_back(0.5f);
      } else {
        if (i >= duration / 2) input.push_back(0.0f);
        else input.push_back(-0.5f);
      }
    }
  }

  auto source = g.add_process<InterleavedStreamingAudioSource>(
      "source", input, BlockDescription{block_size, channels, 48000});

  auto track_analyser = g.register_process(make_track_analyser("track analyser"));
  auto data_sink = g.add_process<DataSink<BlockActive>>("data_sink");

  g.connect(source->get_out_port("out_samples"), track_analyser->get_in_port("in_samples"));
  g.connect(track_analyser->get_out_port("out_blocks"), data_sink->get_in_port("in"));

  Plan p = plan(g);
  p.run();
  auto b_active = data_sink->get_value();

  // Generate the reference output that should be achieved.
  std::vector<bool> ref_out; 
  const std::size_t blocks = duration / block_size;
  for (auto i = 0; i != blocks; ++i) {
    for (auto ch = 0; ch < channels; ch++) {
      if (ch % 2 == 0) {
        if (i < blocks / 4) ref_out.push_back(false);
        else ref_out.push_back(true);
      } else {
        if (i >= blocks / 2) ref_out.push_back(false);
        else ref_out.push_back(true);
      }
    }
  }

  // Compare the reference output with the test output
  for (uint32_t i = 0; i < ref_out.size(); i+=channels) {
    for (uint32_t ch = 0; ch < channels; ch++) {
      REQUIRE(b_active.active[i / 2][ch] == ref_out[i + ch]);
    }
  }
}
