#include <cmath>
#include <cstdlib>
#include <iostream>

#include <eat/framework/evaluate.hpp>
#include "eat/framework/process.hpp"

#include "eat/process/audio_capture.hpp"
#include "eat/process/audio_playback.hpp"

using namespace eat::framework;
using namespace eat::process;

int main(int argc, char **argv) {
  if (argc < 1) {
    std::cout << "usage: " << (argc ? argv[0] : "read_play_audio") << "\n";
    return 1;
  }

  Graph g;

  auto audio_recorder = g.register_process(make_capture_audio("audio recorder", 10, 512, 48000));
  auto audio_player = g.register_process(make_playback_audio("audio player", 2, 512, 48000));

  g.connect(audio_recorder->get_out_port("out_samples"), audio_player->get_in_port("in_samples"));

  Plan p = plan(g);
  p.run();
}
