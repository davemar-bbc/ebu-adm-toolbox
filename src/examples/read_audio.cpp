#include <cmath>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <chrono>

#include <bw64/bw64.hpp>
#include <eat/framework/evaluate.hpp>

#include <eat/process/adm_bw64.hpp>
#include "eat/process/audio_capture.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace adm;



namespace {

class Stopper : public StreamingAtomicProcess {
 public:
  Stopper(const std::string &name, double duration_)
      : StreamingAtomicProcess(name),
        duration(duration_),
        out_stop(add_out_port<StreamPort<bool>>("out_stop")) {
  }

  void initialise() override { 
    start = std::chrono::system_clock::now();
  }

  void process() override {
    auto now = std::chrono::system_clock::now();
    if (duration > 0.0) {
      if (now - start >= std::chrono::duration<double>(duration)) {
        out_stop->close();
      }
    }
  }

  void finalise() override {
  }

  std::optional<float> get_progress() override {
    return std::nullopt;
  }

 private:
  StreamPortPtr<bool> out_stop;
  std::chrono::time_point<std::chrono::system_clock> start;
  double duration;
};

}

namespace eat::process {

ProcessPtr make_stopper(const std::string &name, double duration_) {
  return std::make_shared<Stopper>(name, duration_);
}
}


int main(int argc, char **argv) {
  if (argc < 3) {
    std::cout << "usage: " << (argc ? argv[0] : "read_audio") << " out.wav duration/s\n";
    return 1;
  }

  std::string out_path = argv[1];
  double duration = atof(argv[2]);

  Graph g;

  auto stopper = g.register_process(make_stopper("stopper", duration));
  auto audio_recorder = g.register_process(make_capture_audio("audio recorder", 11, 128, 48000));
  auto writer = g.register_process(make_write_bw64("writer", out_path));

  g.connect(stopper->get_out_port("out_stop"), audio_recorder->get_in_port("in_stop"));
  g.connect(audio_recorder->get_out_port("out_samples"), writer->get_in_port("in_samples"));

  Plan p = plan(g);
  p.run();
}
