#define APLAY

#include <cmath>
#include <cstdlib>
#include <iostream>

#include <bw64/bw64.hpp>
#include <eat/framework/evaluate.hpp>
#include <eat/framework/utility_processes.hpp>

#include <eat/process/adm_bw64.hpp>
#include "eat/process/sadm_generator.hpp"
#ifdef APLAY
#include "eat/process/audio_playback.hpp"
#else
#include "eat/process/trigger.hpp"
#endif
//#include "eat/process/trigger.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace adm;


int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "usage: " << (argc ? argv[0] : "test_sadm") << " in.wav [device num]\n";
    return 1;
  }

  std::string in_path = argv[1];

  Config config;

  config.device_index = 2;
  if (argc >= 3) {
    config.device_index = atoi(argv[2]);
  }
  config.metadata_channel = -1;
  config.frame_size = 960;
  config.channel_count = 2;
  config.template_fname = std::string("/Users/davidm/Work/ADM/ProjectOSC/sadm_prod_prof/prod_prof_adm.xml");
  config.track_map_fname = std::string("/Users/davidm/Work/ADM/ProjectOSC/sadm_prod_prof/track_map_prod.csv");
  config.input_file = std::string("/Users/davidm/Work/Standards/EBU/AS-PSE/Profile/ESC_Example/combined_short_prod_adm.wav");

  Graph g;

#ifdef APLAY
  auto reader = g.register_process(make_read_bw64("reader", in_path, 1024));
  auto audio_player = g.register_process(make_playback_audio("audio player", config.device_index, 128, 48000));
#else
  auto trigger_send = g.register_process(make_trigger_send("trigger send"));
#endif
  auto sadm_generator = g.register_process(make_sadm_generator("sadm generator", config));
  auto sadm_output =  g.register_process(make_sadm_output("sadm output"));

  //auto trigger_read = g.register_process(make_trigger_read("trigger read"));

#ifdef APLAY
  g.connect(reader->get_out_port("out_samples"), audio_player->get_in_port("in_samples"));
  g.connect(audio_player->get_out_port("out_trigger"), sadm_generator->get_in_port("in_trigger"));
#else
  g.connect(trigger_send->get_out_port("out_trigger"), sadm_generator->get_in_port("in_trigger"));
#endif
  g.connect(sadm_generator->get_out_port("out_sadm"), sadm_output->get_in_port("in_sadm"));

  //g.connect(trigger_send->get_out_port("out_trigger"), trigger_read->get_in_port("in_trigger"));
  //g.connect(audio_player->get_out_port("out_trigger"), trigger_read->get_in_port("in_trigger"));

  Plan p = plan(g);
  p.run();
}
