#define APLAY

#include <cmath>
#include <cstdlib>
#include <iostream>

#include <bw64/bw64.hpp>
#include <eat/framework/evaluate.hpp>
#include <eat/framework/utility_processes.hpp>

#include <eat/process/adm_bw64.hpp>
#include "eat/process/track_analyser.hpp"
#include "eat/process/object_modifier.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace adm;


int main(int argc, char **argv) {
  if (argc != 3) {
    std::cout << "usage: " << (argc ? argv[0] : "object_chop") << " in.wav out.wav\n";
    return 1;
  }

  std::string in_path = argv[1];
  std::string out_path = argv[2];

  Graph g;

  auto reader = g.register_process(make_read_adm_bw64("reader", in_path, 1024));
  auto track_analyser = g.register_process(make_track_analyser("track analyser"));
  auto object_modifier = g.register_process(make_object_modifier("object modifier"));
  //auto block_print = g.register_process(make_block_print("block print"));
  auto writer = g.register_process(make_write_adm_bw64("writer", out_path));

  g.connect(reader->get_out_port("out_samples"), track_analyser->get_in_port("in_samples"));
  g.connect(reader->get_out_port("out_axml"), object_modifier->get_in_port("in_axml"));
  //g.connect(track_analyser->get_out_port("out_blocks"), block_print->get_in_port("in_blocks"));
  g.connect(track_analyser->get_out_port("out_blocks"), object_modifier->get_in_port("in_blocks"));
  g.connect(object_modifier->get_out_port("out_axml"), writer->get_in_port("in_axml"));
  g.connect(reader->get_out_port("out_samples"), writer->get_in_port("in_samples"));

  //g.connect(reader->get_out_port("out_axml"), writer->get_in_port("in_axml"));

  Plan p = plan(g);
  p.run();
}
