#pragma once
#include <chrono>
#include <string>

class PAInit {
 public:
  PAInit();
  ~PAInit();
};

class DeviceInfo {
  std::string name() const;
  int max_input_channels() const;
  int max_output_channels() const;
  std::chrono::microseconds default_low_input_latency() const;
  std::chrono::microseconds default_low_output_latency() const;
  std::chrono::microseconds default_high_input_latency() const;
  std::chrono::microseconds default_high_output_latency() const;
  double default_sample_rate() const;
};