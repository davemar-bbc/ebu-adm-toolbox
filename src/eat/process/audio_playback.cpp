#include <iostream>
#include <unistd.h>
#include <vector>
#include <span>

#include "eat/process/pa_init.hpp"
#include <portaudio.h>

#include "eat/process/audio_playback.hpp"

#include "eat/framework/exceptions.hpp"
#include "eat/process/block.hpp"


using namespace eat::framework;
using namespace eat::process;

namespace {

class AudioPlayback : public StreamingAtomicProcess {
 public:
  AudioPlayback(const std::string &name, const int device_num_,
                const int frames_per_buffer_, const uint32_t sample_rate_)
      : StreamingAtomicProcess(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_trigger(add_out_port<StreamPort<bool>>("out_trigger")) {
    pa_config.device_num = device_num_;
    pa_config.frames_per_buffer = static_cast<std::size_t>(frames_per_buffer_);
    pa_config.sample_rate = sample_rate_;
  }

  void convert(std::span<float> input, std::span<int32_t> output) {
    std::transform(input.begin(), input.end(), output.begin(), [](float sample) {
      double scaled = sample * 0x7FFFFFFF;
      return static_cast<int32_t>(scaled); });
  }

  template <typename T>
  static int pa_callback_interleaved(const void *inputBuffer, void *outputBuffer,
                                   unsigned long framesPerBuffer,
                                   const PaStreamCallbackTimeInfo*,
                                   PaStreamCallbackFlags,
                                   void* userData) {
    (void)inputBuffer;
    (void)framesPerBuffer;
    auto processor = static_cast<T *>(userData);
    assert(processor);
    processor->process_interleaved(static_cast<uint32_t *>(outputBuffer));
    return 0;
  }

  void list_devices() {
    int numDevices;
    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) throw std::runtime_error("No devices");
    const PaDeviceInfo *deviceInfo;
    std::cout << "Playback devices" << std::endl;
    std::cout << "Num\tOPs\tName" << std::endl;
    for( auto i = 0; i < numDevices; ++i) {
      deviceInfo = Pa_GetDeviceInfo(i);
      std::cout << i << ":\t" << deviceInfo->maxOutputChannels << "\t" << deviceInfo->name << std::endl;
    }
    std::cout << std::endl;
  }

  PaStreamParameters get_stream_parameters() {
    PaStreamParameters outputParameters{};
    auto info = Pa_GetDeviceInfo(pa_config.device_num);
    outputParameters.device = pa_config.device_num;
    outputParameters.sampleFormat = paInt32;
    outputParameters.channelCount = pa_config.channel_count;
    outputParameters.hostApiSpecificStreamInfo = nullptr;
    outputParameters.suggestedLatency = info->defaultLowOutputLatency;
    return outputParameters;
  }

  void initialise() override { 
    stream = nullptr;

    list_devices();

    auto device_info = Pa_GetDeviceInfo(pa_config.device_num);
    pa_config.channel_count = static_cast<std::size_t>(device_info->maxOutputChannels);

    std::cout << "initialise: " << pa_config.channel_count << std::endl;
    auto outputParameters = get_stream_parameters();

    auto err = Pa_OpenStream(&stream, nullptr, &outputParameters, pa_config.sample_rate, 
                            pa_config.frames_per_buffer, 
                            paDitherOff, pa_callback_interleaved<AudioPlayback>, this);
    if (err != paNoError) {
      std::cerr << "Error: " << Pa_GetErrorText(err) << std::endl;
      throw std::runtime_error("error opening audio stream");
    }
    err = Pa_StartStream(stream);
    if (err != paNoError) {
      std::cerr << "Error: " << Pa_GetErrorText(err) << std::endl;
      throw std::runtime_error("error starting audio stream");
    }

    fr_count = 0;
    sample_count = 1024; //frame_info.sample_count;
    out_samps.resize(sample_count * pa_config.channel_count);
    ready = true;
  }

  void process() override {
    if (ready) {
      out_trigger->push(true);
      if (in_samples->available()) {
        auto samples = in_samples->pop().read();
        auto &frame_info = samples->info();
        sample_count = frame_info.sample_count;
        std::size_t op_channel_count = std::min(pa_config.channel_count, sample_count);

        for (uint32_t i = 0; i < sample_count; i++) {
          for (uint32_t ch = 0; ch < op_channel_count; ch++) {
            out_samps[(i * pa_config.channel_count) + ch] = samples->sample(ch, i);
          }
        } 
      }
      ready = false;
    }
  }

  void process_interleaved(uint32_t *output) {
    auto offset = fr_count * pa_config.channel_count;
    auto output_size = pa_config.frames_per_buffer * pa_config.channel_count;
    auto output_as_float = std::span<float>(&(out_samps.data())[offset], output_size);
    convert(output_as_float, {reinterpret_cast<int32_t *>(output), output_size});

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
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  StreamPortPtr<bool> out_trigger;
  PaConfigPlay pa_config;
  uint64_t fr_count;
  std::size_t sample_count;
  std::vector<float> out_samps;
  bool ready;
};

}

namespace eat::process {

ProcessPtr make_playback_audio(const std::string &name, const int device_num_,
                               const int frames_per_buffer_, const uint32_t sample_rate_) {
  return std::make_shared<AudioPlayback>(name, device_num_, frames_per_buffer_, sample_rate_);
}

}  // namespace eat::process
