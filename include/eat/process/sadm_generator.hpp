#pragma once
#include <vector>
#include <cstdint>
#include <vector>
#include <bw64/bw64.hpp>

#include "eat/process/sadmbuilder.hpp"

#include "eat/framework/process.hpp"

struct Config {
  int device_index;
  int metadata_channel;
  uint32_t frame_size;
  int channel_count;
  std::string template_fname;
  std::string track_map_fname;
  std::string input_file;
};


namespace eat::process {

/// 
/// ports:
/// - in_sadm (StreamPort<std::string>) : input S-ADM frame
framework::ProcessPtr make_sadm_output(const std::string &name);

/// 
/// ports:
/// - out_sadm (StreamPort<std::string>) : output S-ADM frame
framework::ProcessPtr make_sadm_generator(const std::string &name, const Config config_);

}  // namespace eat::process


class Threader {
 public:
  Threader(const Config config_);
  void start_player();
  void trigger();
  std::string getXml();

  class Generator;

 protected:
  std::condition_variable cond_var;
  std::mutex mut;
  bool ready;
 private:
  std::shared_ptr<Generator> generator;
  Config config;
};


class Threader::Generator {
 public:
  explicit Generator(Threader &player_generator_, Config const& config);
  ~Generator() {};
  void run();
  std::string getXml() { return sadm_xml; };

 private:
  std::pair<std::shared_ptr<Document>, std::shared_ptr<FrameHeader>> generate(uint32_t frame_number_, std::chrono::nanoseconds frame_start_) {
    sadm_builder.buildSadm(frame_number_, frame_start_.count());
    sadm_builder.addTransportTrackFormat(track_format);
    return std::make_pair(sadm_builder.frame(), sadm_builder.frameHeader());
  }

  std::string getXML() {
    return sadm_builder.frameToString();
  }

  std::string generate_metadata(std::chrono::nanoseconds frame_start_);

  int output_channels;
  uint32_t frame_length;
  int64_t frame_length_ns{0};
  uint64_t frame_start{0};
  uint64_t frame_number{0};
  std::shared_ptr<TransportTrackFormat> track_format;
  SadmBuilder sadm_builder;
  Threader &threader;
  std::string sadm_xml;
};
