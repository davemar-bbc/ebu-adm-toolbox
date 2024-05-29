#include <cmath>
#include <cstdlib>
#include <iostream>

#include <bw64/bw64.hpp>
#include <eat/framework/evaluate.hpp>

#include <eat/process/adm_bw64.hpp>
#include "eat/process/audio_playback.hpp"
#include "eat/process/sadm_generator.hpp"
#include "eat/process/trigger.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace adm;


int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "usage: " << (argc ? argv[0] : "play_audio") << " in.wav\n";
    return 1;
  }

  std::string in_path = argv[1];

  Graph g;

  auto reader = g.register_process(make_read_bw64("reader", in_path, 1024));
  auto audio_player = g.register_process(make_playback_audio("audio player", 2, 128, 48000));
  auto trigger_read = g.register_process(make_trigger_read("trigger read"));

  g.connect(reader->get_out_port("out_samples"), audio_player->get_in_port("in_samples"));
  g.connect(audio_player->get_out_port("out_trigger"), trigger_read->get_in_port("in_trigger"));

  Plan p = plan(g);
  p.run();
}
