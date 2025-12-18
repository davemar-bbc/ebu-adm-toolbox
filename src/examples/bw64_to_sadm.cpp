#define APLAY

#include <cmath>
#include <cstdlib>
#include <iostream>

#include <bw64/bw64.hpp>
#include <eat/framework/evaluate.hpp>
#include <eat/framework/utility_processes.hpp>

#include <eat/process/adm_bw64.hpp>
#include "eat/process/sadm_io.hpp"
#include "eat/process/adm_to_sadm.hpp"
#include "eat/process/block.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace adm;

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cout << "usage: " << (argc ? argv[0] : "bw64_to_sadm") << " <in.wav> <frame size/samps>\n";
    return 1;
  }

  std::string in_path = argv[1];
  size_t frame_samples = atoi(argv[2]);

  Graph g;

  auto frame_secs = std::chrono::milliseconds(frame_samples * 1000 / 48000);

  auto reader = g.register_process(make_read_adm_bw64("reader", in_path, frame_samples));
  auto wav_length = g.register_process(make_wav_length("wav length", in_path));
  auto adm_to_sadm = g.register_process(make_adm_to_sadm("adm to sadm", frame_secs));
  auto sadm_output = g.register_process(make_sadm_output("sadm output"));
  auto audio_output = g.register_process(make_audio_frame_output("audio output"));

  g.connect(reader->get_out_port("out_axml"), adm_to_sadm->get_in_port("in_axml"));
  g.connect(wav_length->get_out_port("out_length"), adm_to_sadm->get_in_port("in_length"));
  g.connect(adm_to_sadm->get_out_port("out_sadm"), sadm_output->get_in_port("in_sadm"));
  g.connect(reader->get_out_port("out_samples"), audio_output->get_in_port("in_samples"));

  Plan p = plan(g);
  p.run();
}
