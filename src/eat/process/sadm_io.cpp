#include <adm/write.hpp>
#include <bw64/bw64.hpp>
#include <string>
#include <sstream>
#include <unistd.h>

#include "eat/process/sadm_io.hpp"

#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"

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

class AudioFrameOutput : public StreamingAtomicProcess {
 public:
  AudioFrameOutput(const std::string &name)
      : StreamingAtomicProcess(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")) {}
    
  void initialise() override { 
    frame_number = 0;
  }
  
  void process() override {
    if (in_samples->available()) {
      auto samples = in_samples->pop().read();
      auto &frame_info = samples->info();

      std::shared_ptr<bw64::Bw64Writer> file;
      char fname[100];
      snprintf(fname, sizeof(fname), "/tmp/tmp_audio_%05llu.wav", frame_number);
      file = bw64::writeFile(fname, frame_info.channel_count, frame_info.sample_rate, 24);

      file->write(samples->data(), frame_info.sample_count);
      frame_number++;
    }
  }
  
  void finalise() override {
  }  

 private:
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  uint64_t frame_number;
};

ProcessPtr make_audio_frame_output(const std::string &name) {
  return std::make_shared<AudioFrameOutput>(name); 
}

} // namespace eat::process
