#include <adm/write.hpp>
#include <bw64/bw64.hpp>
#include <string>
#include <sstream>
#include <unistd.h>

#include <condition_variable>
#include <mutex>
#include <thread>

#include "eat/process/sadm_generator.hpp"

#include "eat/process/adm_bw64.hpp"

using namespace eat::framework;

namespace eat::process {

class SadmOutput : public StreamingAtomicProcess {
 public:
  SadmOutput(const std::string &name)
      : StreamingAtomicProcess(name),
        in_sadm(add_in_port<StreamPort<std::string>>("in_sadm")) {}
    
  void initialise() override { 
    frame_number = 0;
  }
  
  void process() override {
    if (in_sadm->available()) {
      auto str_in = in_sadm->pop();
      std::cout << "SadmOutput::process: " << std::endl;
      writeSadmXml(str_in, frame_number);
      frame_number++;
    }
  }
  
  void finalise() override {
  }  

  void writeSadmXml(std::string sadm_xml, uint64_t frame_number_) {
    char fname[100];
    snprintf(fname, sizeof(fname), "/tmp/tmp_sadm_%05llu.xml", frame_number_);
    std::ofstream opfile;
    opfile.open(fname);
    opfile << sadm_xml;
    opfile.close();
  }

 private:
  StreamPortPtr<std::string> in_sadm;
  uint64_t frame_number;
};

ProcessPtr make_sadm_output(const std::string &name) {
  return std::make_shared<SadmOutput>(name); 
}

// ----------------------------------------------------------------------------

class SadmGenerator : public StreamingAtomicProcess {
 public:
  SadmGenerator(const std::string &name, const Config config_)
      : StreamingAtomicProcess(name),
        config(config_),
        in_trigger(add_in_port<StreamPort<bool>>("in_trigger")),
        out_sadm(add_out_port<StreamPort<std::string>>("out_sadm")) {
        }

  void initialise() override { 
    std::cout << "initialise: start_player " << std::endl;
    threader = std::make_shared<Threader>(config);
    threader->start_player();
    std::cout << "initialise: done: " << std::endl;
    beginning = std::chrono::system_clock::now();
  }

  void process() override {
    if (in_trigger->available()) {
      auto trigger = in_trigger->pop();
      auto now1 = std::chrono::system_clock::now() - beginning;
      threader->trigger();
      auto now2 = std::chrono::system_clock::now() - beginning;
      auto sadm_xml = threader->getXml();
      auto now3 = std::chrono::system_clock::now() - beginning;
      out_sadm->push(std::move(sadm_xml));
      auto now4 = std::chrono::system_clock::now() - beginning;
      std::cout << "SadmGenerator::process: " << now1.count() / 1e6 << " " <<
        now2.count() / 1e6 << " " << now3.count() / 1e6 << " " << now4.count() / 1e6 << std::endl;
    }
  }

  void finalise() override {
  }

 private:
  Config config;
  StreamPortPtr<bool> in_trigger;
  StreamPortPtr<std::string> out_sadm;
  std::shared_ptr<Threader> threader;
  std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> beginning;
};

ProcessPtr make_sadm_generator(const std::string &name, const Config config_) {
  return std::make_shared<SadmGenerator>(name, config_); 
}

}  // namespace eat::process


// ----------------------------------------------------------------------------


Threader::Threader(Config config_) {
  config = config_;
  generator = std::make_shared<Generator>(*this, config);
}

void Threader::start_player() {
  std::thread generator_thread(&Generator::run, generator);
  generator_thread.detach();
}

void Threader::trigger() {
  { // Scope for mutex lock
    std::lock_guard<std::mutex> lg{mut};
    ready = true;
  }
  cond_var.notify_one();
}

std::string Threader::getXml() { 
  return generator->getXml(); 
}


namespace
{
  std::chrono::nanoseconds to_nanoseconds_48(uint64_t sample_count) {
    using SampleTime = std::chrono::duration<uint64_t, std::ratio<1, 48000>>;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(SampleTime{sample_count});
  }

  int64_t to_nanoseconds_int64(uint64_t sample_count) {
    using SampleTime = std::chrono::duration<uint64_t, std::ratio<1, 48000>>;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(SampleTime{sample_count}).count();
  }
}

Threader::Generator::Generator(Threader &threader_, const Config &config)
    : output_channels{config.channel_count},
      frame_length{config.frame_size},
      frame_length_ns{to_nanoseconds_int64(frame_length)},
      sadm_builder{frame_length_ns, config.track_map_fname},
      threader{threader_} {
  sadm_builder.getBaseAdm(config.template_fname);
  track_format = sadm_builder.buildTracks();

  auto base_str = sadm_builder.baseToString();
  std::unique_lock<std::mutex> lock{threader.mut};
  threader.ready = false;
  lock.unlock();
  std::cout << "Generator: output_channels: " << output_channels << std::endl;
}

void Threader::Generator::run() {
  std::cout << "Generator::run: start" << std::endl;
  while (true) {
    auto xml_str = generate_metadata(to_nanoseconds_48(frame_start));

    std::unique_lock<std::mutex> lock{threader.mut};
    threader.cond_var.wait(lock, [this]()
                                   { return threader.ready; });
    threader.ready = false;
    sadm_xml = xml_str;  // Copy string here, so it's protect by mutex
    lock.unlock();

    frame_start += frame_length;
  }
}

std::string Threader::Generator::generate_metadata(std::chrono::nanoseconds frame_start_) {
  auto sadm_pair = generate(frame_number, frame_start_);
  auto sadm_frame = sadm_pair.first;
  auto sadm_frame_header = sadm_pair.second;
  ++frame_number;

  return getXML();
}

