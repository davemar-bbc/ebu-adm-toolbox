#pragma once
#include <vector>
#include <cstdint>
#include <vector>
#include <bw64/bw64.hpp>

#include "eat/framework/process.hpp"


class TrigThreader {
 public:
  TrigThreader();
  void start();
  void trigger();
  void run();
 
 public:
  bool ready;

 private:
  std::condition_variable cond_var;
  std::mutex mut;
  std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> beginning;
  int64_t frame_count_ns;
};

namespace eat::process {

/// 
/// ports:
/// - out_trigger (StreamPort<bool>) : output trigger
framework::ProcessPtr make_trigger_send(const std::string &name);

/// 
/// ports:
/// - in_trigger (StreamPort<bool>) : output trigger
framework::ProcessPtr make_trigger_read(const std::string &name);

}
