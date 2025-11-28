
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


using namespace eat::framework;
using namespace eat::process;
using namespace adm;
using namespace eat::render;


int main(int argc, char **argv) {
  if (argc != 4) {
    std::cout << "usage: " << (argc ? argv[0] : "level_select") << " in.wav out.wav level\n";
    return 1;
  }

  std::string in_path = argv[1];
  std::string out_path = argv[2];
  uint16_t level = atoi(argv[3]);

  size_t block_size = 1024;

  Graph g;

  auto reader = g.register_process(make_read_adm_bw64("reader", in_path, block_size));
  auto writer = g.register_process(make_write_adm_bw64("writer", out_path));

  auto squeezer_prod_prof = g.register_process(make_squeezer_prod_prof("squeezer pp", block_size, level));

  g.connect(reader->get_out_port("out_axml"), squeezer_prod_prof->get_in_port("in_axml"));
  g.connect(reader->get_out_port("out_samples"), squeezer_prod_prof->get_in_port("in_samples"));

  g.connect(squeezer_prod_prof->get_out_port("out_axml"), writer->get_in_port("in_axml"));
  g.connect(squeezer_prod_prof->get_out_port("out_samples"), writer->get_in_port("in_samples"));


  Plan p = plan(g);
  p.run();
}
