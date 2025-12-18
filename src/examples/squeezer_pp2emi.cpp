
#include <cmath>
#include <cstdlib>
#include <iostream>

#include <ear/bs2051.hpp>
#include <bw64/bw64.hpp>
#include <eat/framework/evaluate.hpp>
#include <eat/framework/utility_processes.hpp>

#include <eat/process/adm_bw64.hpp>
#include <eat/process/block.hpp>
#include "eat/process/squeezer_pp.hpp"
#include "eat/render/render.hpp"
#include "eat/process/loudness.hpp"
#include "eat/process/profile_conversion_misc.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace adm;
using namespace eat::render;


int main(int argc, char **argv) {
  if (argc != 4) {
    std::cout << "usage: " << (argc ? argv[0] : "squeezer_pp2emi") << " in.wav out.wav level\n";
    return 1;
  }

  std::string in_path = argv[1];
  std::string out_path = argv[2];
  uint16_t level = atoi(argv[3]);

  size_t block_size = 1024;

  Graph g;

  auto reader = g.register_process(make_read_adm_bw64("reader", in_path, block_size));
  auto writer = g.register_process(make_write_adm_bw64("writer", out_path));
  auto measure_loudness = g.register_process(make_update_all_loudnesses("measure_loudness", true));
  auto squeezer_prod_prof = g.register_process(make_squeezer_prod_prof("squeezer pp", block_size, level));
  auto profile = g.register_process(make_set_profiles("set_profiles", {profiles::ITUEmissionProfile{2}}));

  g.connect(reader->get_out_port("out_axml"), squeezer_prod_prof->get_in_port("in_axml"));
  g.connect(reader->get_out_port("out_samples"), squeezer_prod_prof->get_in_port("in_samples"));
  g.connect(squeezer_prod_prof->get_out_port("out_axml"), measure_loudness->get_in_port("in_axml"));
  g.connect(squeezer_prod_prof->get_out_port("out_samples"), writer->get_in_port("in_samples"));
  g.connect(squeezer_prod_prof->get_out_port("out_samples"), measure_loudness->get_in_port("in_samples"));
  g.connect(measure_loudness->get_out_port("out_axml"), profile->get_in_port("in_axml"));
  g.connect(profile->get_out_port("out_axml"), writer->get_in_port("in_axml"));

  Plan p = plan(g);
  p.run();
}
