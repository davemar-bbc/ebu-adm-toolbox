#include <cmath>
#include <cstdlib>
#include <iostream>

#include <bw64/bw64.hpp>
#include <eat/framework/evaluate.hpp>
#include <eat/framework/utility_processes.hpp>

#include <eat/process/adm_bw64.hpp>
#include "eat/render/render.hpp"
#include "eat/process/block.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace eat::render;
using namespace adm;

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cout << "usage: " << (argc ? argv[0] : "bw64_render") << " <in.wav> <out.wav> <layout> [programme_id]\n";
    return 1;
  }

  std::string in_path = argv[1];
  std::string out_path = argv[2];
  auto layout = ear::getLayout(argv[3]);
  std::string prod_id = "APR_1001";
  if (argc >= 5) {
    prod_id = std::string(argv[4]);
  }

  eat::render::SelectionOptionsId options = {eat::render::ProgrammeIdStart{adm::parseAudioProgrammeId(prod_id)}};

  Graph g;

  auto reader = g.register_process(make_read_adm_bw64("reader", in_path, 1024));
  auto writer = g.register_process(make_write_bw64("writer", out_path));
  auto renderer = g.register_process(make_render("renderer", layout, 1024, options));

  g.connect(reader->get_out_port("out_samples"), renderer->get_in_port("in_samples"));
  g.connect(reader->get_out_port("out_axml"), renderer->get_in_port("in_axml"));
  g.connect(renderer->get_out_port("out_samples"), writer->get_in_port("in_samples"));

  Plan p = plan(g);
  p.run();
}
