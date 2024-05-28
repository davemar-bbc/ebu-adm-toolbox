#include <iostream>
#include <unistd.h>
#include <vector>
#include <span>

#include "eat/process/pa_init.hpp"
#include <portaudio.h>

#include "eat/process/audio_capture.hpp"

#include "eat/framework/exceptions.hpp"
#include "eat/process/block.hpp"


using namespace eat::framework;
using namespace eat::process;

namespace {

class AudioCapture : public StreamingAtomicProcess {
 public:
  AudioCapture(const std::string &name, const uint32_t device_num_,
               const int frames_per_buffer_, const uint32_t sample_rate_)
      : StreamingAtomicProcess(name),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples")),
        in_stop(add_in_port<StreamPort<bool>>("in_stop")) {
    pa_config.device_num = device_num_;
    pa_config.frames_per_buffer = static_cast<std::size_t>(frames_per_buffer_);
    pa_config.sample_rate = sample_rate_;
  }

  template <typename T>
  static int pa_callback_interleaved(const void *inputBuffer, void *outputBuffer,
                                   unsigned long framesPerBuffer,
                                   const PaStreamCallbackTimeInfo*,
                                   PaStreamCallbackFlags,
                                   void* userData) {
    (void)outputBuffer;
    (void)framesPerBuffer;
    auto processor = static_cast<T *>(userData);
    assert(processor);
    processor->process_interleaved(static_cast<int32_t const*>(inputBuffer));
    return 0;
  }

  void list_devices() {
    int numDevices;
    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) throw std::runtime_error("No devices");
    const PaDeviceInfo *deviceInfo;
    std::cout << "Capture devices" << std::endl;
    std::cout << "Num\tIPs\tName" << std::endl;
    for( auto i = 0; i < numDevices; ++i) {
      deviceInfo = Pa_GetDeviceInfo(i);
      std::cout << i << ":\t" << deviceInfo->maxInputChannels << "\t" << deviceInfo->name << std::endl;
    }
    std::cout << std::endl;
  }

  PaStreamParameters get_stream_parameters() {
    PaStreamParameters inputParameters{};
    auto info = Pa_GetDeviceInfo(static_cast<PaDeviceIndex>(pa_config.device_num));
    inputParameters.device = static_cast<PaDeviceIndex>(pa_config.device_num);
    inputParameters.sampleFormat = paInt32;
    inputParameters.channelCount = pa_config.channel_count;
    inputParameters.hostApiSpecificStreamInfo = nullptr;
    inputParameters.suggestedLatency = info->defaultLowInputLatency;
    return inputParameters;
  }

  void initialise() override { 
    stream = nullptr;

    list_devices();

    auto device_info = Pa_GetDeviceInfo(static_cast<PaDeviceIndex>(pa_config.device_num));
    pa_config.channel_count = static_cast<std::size_t>(device_info->maxInputChannels);

    auto inputParameters = get_stream_parameters();

    auto err = Pa_OpenStream(&stream, &inputParameters, nullptr, pa_config.sample_rate, 
                            pa_config.frames_per_buffer, 
                            paDitherOff, pa_callback_interleaved<AudioCapture>, this);
    if (err != paNoError) {
      std::cerr << "Error: " << Pa_GetErrorText(err) << std::endl;
      throw std::runtime_error("error opening audio stream");
    }
    fr_count = 0;
    sample_count = 1024; //frame_info.sample_count;
    in_samps.resize(sample_count * pa_config.channel_count);
    ready = true;
    err = Pa_StartStream(stream);
    if (err != paNoError) {
      throw std::runtime_error("error starting audio stream");
    }
  }

  void process() override {
    auto input_size = pa_config.channel_count * sample_count;
    std::vector<int32_t> read_buffer(input_size);

    if (ready) {
      auto samples = std::make_shared<InterleavedSampleBlock>(
            std::move(in_samps), 
            BlockDescription{sample_count, pa_config.channel_count, pa_config.sample_rate});
      out_samples->push(std::move(samples));
      in_samps.resize(sample_count * pa_config.channel_count);
      ready = false;

      if (in_stop->eof()) {
        out_samples->close();
      }
    }
  }

  void process_interleaved(const int32_t *input) {
    auto input_size = pa_config.channel_count * pa_config.frames_per_buffer;
    std::vector<float> fsamples(input_size);
    auto offset = fr_count * pa_config.channel_count;
    for (uint32_t i = 0; i < input_size; i++) {
      in_samps[i + offset] = static_cast<double>(input[i]) / 2147483648.0;
    }
    fr_count += pa_config.frames_per_buffer;
    if (fr_count >= sample_count) {
      fr_count = 0;
      ready = true;
    }
  }

  void finalise() override {
    Pa_StopStream(&stream);
    Pa_CloseStream(&stream);
  }

  std::optional<float> get_progress() override {
    return std::nullopt;
  }

 private:
  PAInit pa;
  PaStream *stream;
  StreamPortPtr<InterleavedBlockPtr> out_samples;
  StreamPortPtr<bool> in_stop;
  PaConfig pa_config;
  uint64_t fr_count;
  std::size_t sample_count;
  std::vector<float> in_samps;
  bool ready;
};

}

namespace eat::process {

ProcessPtr make_capture_audio(const std::string &name, const uint32_t device_num_,
                              const int frames_per_buffer_, const uint32_t sample_rate_) {
  return std::make_shared<AudioCapture>(name, device_num_, frames_per_buffer_, sample_rate_);
}

}  // namespace eat::process
