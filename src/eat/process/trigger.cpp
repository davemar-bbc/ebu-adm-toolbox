#include <adm/write.hpp>
#include <bw64/bw64.hpp>
#include <string>
#include <sstream>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "eat/process/trigger.hpp"

using namespace eat::framework;

namespace eat::process {

class TriggerSend : public StreamingAtomicProcess {
 public:
  TriggerSend(const std::string &name)
      : StreamingAtomicProcess(name),
        out_trigger(add_out_port<StreamPort<bool>>("out_trigger")) {}
    
  void initialise() override { 
    std::cout << "TriggerSend::initialise" << std::endl;
    trig_threader = std::make_shared<TrigThreader>();
    trig_threader->start();
    beginning = std::chrono::system_clock::now();
    frame_count_ns = 0;
  }
  
  void process() override {
    if (trig_threader->ready) {
      out_trigger->push(true);
      trig_threader->ready = false;
      auto now = std::chrono::system_clock::now() - beginning;
      std::cout << "TriggerSend::process ready: " << now.count() / 1e6 << std::endl;
    }
  }
  
  void finalise() override {
  }  

 private:
  StreamPortPtr<bool> out_trigger;
  std::shared_ptr<TrigThreader> trig_threader;
  std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> beginning;
  int64_t frame_count_ns;
};

ProcessPtr make_trigger_send(const std::string &name) {
  return std::make_shared<TriggerSend>(name); 
}

} // namespace eat::process



TrigThreader::TrigThreader() {
  ready = false;
}

void TrigThreader::start() {
  std::thread this_thread(&TrigThreader::run, this);
  beginning = std::chrono::system_clock::now();
  frame_count_ns = 0;
  this_thread.detach();
}

void TrigThreader::trigger() {
  { // Scope for mutex lock
    std::lock_guard<std::mutex> lg{mut};
    ready = true;
  }
  cond_var.notify_one();
}

void TrigThreader::run() {
  std::cout << "TrigThreader::run: start" << std::endl;
  while (true) {
    frame_count_ns += 1024000000 / 48;
    std::unique_lock<std::mutex> lock{mut};
    ready = true;
    lock.unlock();
    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> timeout = 
              std::chrono::nanoseconds(frame_count_ns) + beginning;
    std::this_thread::sleep_until(timeout);
  }
}


// ----------------------------------------------------------------------------


namespace eat::process {

class TriggerRead : public StreamingAtomicProcess {
 public:
  TriggerRead(const std::string &name)
      : StreamingAtomicProcess(name),
        in_trigger(add_in_port<StreamPort<bool>>("in_trigger")) {}
    
  void initialise() override { 
    std::cout << "TriggerRead::initialise" << std::endl;
    beginning = std::chrono::system_clock::now();
  }
  
  void process() override {
    if (in_trigger->available()) {
      (void)in_trigger->pop();
      auto now = std::chrono::system_clock::now() - beginning;
      std::cout << "TriggerRead::process trigger: " << now.count() / 1e6 << std::endl;
    }
  }
  
  void finalise() override {
  }  

 private:
  StreamPortPtr<bool> in_trigger;
  std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> beginning;
};

ProcessPtr make_trigger_read(const std::string &name) {
  return std::make_shared<TriggerRead>(name); 
}

}
