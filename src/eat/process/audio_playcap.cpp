#include <iostream>
#include <unistd.h>
#include <vector>
#include <span>

#include "eat/process/pa_init.hpp"
#include <portaudio.h>

#include "eat/process/audio_playcap.hpp"

#include "eat/framework/exceptions.hpp"
#include "eat/process/block.hpp"


using namespace eat::framework;
using namespace eat::process;

namespace {

class AudioPlayCap : public StreamingAtomicProcess {
 public:
  AudioPlayCap(const std::string &name, const uint32_t device_num_,
               const int frames_per_buffer_, const uint32_t sample_rate_)
      : StreamingAtomicProcess(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples")) {
    pa_in_config.device_num = device_num_;
    pa_in_config.frames_per_buffer = static_cast<std::size_t>(frames_per_buffer_);
    pa_in_config.sample_rate = sample_rate_;
    pa_out_config.device_num = device_num_;
    pa_out_config.frames_per_buffer = static_cast<std::size_t>(frames_per_buffer_);
    pa_out_config.sample_rate = sample_rate_;
  }

  void list_devices() {
    int numDevices;
    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) throw std::runtime_error("No devices");
    const PaDeviceInfo *deviceInfo;
    std::cout << "Capture devices" << std::endl;
    std::cout << "Num\tIPs\tOPs\tName" << std::endl;
    for( auto i = 0; i < numDevices; ++i) {
      deviceInfo = Pa_GetDeviceInfo(i);
      std::cout << i << ":\t" << deviceInfo->maxInputChannels << ":\t" << deviceInfo->maxOutputChannels <<"\t" << deviceInfo->name << std::endl;
    }
    std::cout << std::endl;
  }

  std::pair<PaStreamParameters, PaStreamParameters> get_stream_parameters() {
    PaStreamParameters inputParameters{}, outputParameters{};
    auto info = Pa_GetDeviceInfo(static_cast<PaDeviceIndex>(pa_in_config.device_num));
    inputParameters.device = static_cast<PaDeviceIndex>(pa_in_config.device_num);
    inputParameters.sampleFormat = paInt32;
    inputParameters.channelCount = pa_in_config.channel_count;
    inputParameters.hostApiSpecificStreamInfo = nullptr;
    inputParameters.suggestedLatency = info->defaultLowInputLatency;

    outputParameters.device = static_cast<PaDeviceIndex>(pa_out_config.device_num);
    outputParameters.sampleFormat = paInt32;
    outputParameters.channelCount = pa_out_config.channel_count;
    outputParameters.hostApiSpecificStreamInfo = nullptr;
    outputParameters.suggestedLatency = info->defaultLowOutputLatency;
    return {inputParameters, outputParameters};
  }

  void initialise() override { 
    stream = nullptr;

    list_devices();

    auto device_info = Pa_GetDeviceInfo(static_cast<PaDeviceIndex>(pa_in_config.device_num));
    pa_in_config.channel_count = static_cast<std::size_t>(device_info->maxInputChannels);

    auto [inputParameters, outputParameters] = get_stream_parameters();

    auto err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, pa_in_config.sample_rate, 
                            pa_in_config.frames_per_buffer, 
                            paDitherOff, nullptr, nullptr);
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
  }

  void process() override {
    auto input_size = pa_in_config.channel_count * pa_in_config.frames_per_buffer;
    std::vector<int32_t> read_buffer(input_size);
    std::vector<float> fsamples(input_size);

    while (in_samples->available()) {
      // Reading
      auto err = Pa_ReadStream(stream, static_cast<void *>(read_buffer.data()), pa_in_config.frames_per_buffer);
      if (err != paNoError) {
        std::cerr << "Error: " << Pa_GetErrorText(err) << std::endl;
        //throw std::runtime_error("error reading audio stream");
      }
      for (uint32_t i = 0; i < input_size; i++) {
        fsamples[i] = static_cast<double>(read_buffer[i]) / 2147483648.0;
      }
      fr_count++;

      auto samples = std::make_shared<InterleavedSampleBlock>(
            std::move(fsamples), 
            BlockDescription{pa_in_config.frames_per_buffer, pa_in_config.channel_count, pa_in_config.sample_rate});
      out_samples->push(std::move(samples));

      if (fr_count > 10000) {
        out_samples->close();
      }

      // Playout
      auto in_samples_r = in_samples->pop().read();
      auto &frame_info = in_samples_r->info();
      std::size_t op_channel_count = std::min(pa_out_config.channel_count, frame_info.sample_count);

      std::vector<float> out_samps(frame_info.sample_count * pa_out_config.channel_count);
      for (uint32_t i = 0; i < frame_info.sample_count; i++) {
        for (uint32_t ch = 0; ch < op_channel_count; ch++) {
          out_samps[(i * pa_out_config.channel_count) + ch] = in_samples_r->sample(ch, i);
        }
      }

      float *out_samps_p = out_samps.data();
      uint32_t samp_co = 0;
      while (samp_co < frame_info.sample_count * pa_out_config.channel_count) {
        err = Pa_WriteStream(stream, out_samps_p, pa_out_config.frames_per_buffer);
        if (err != paNoError) {
          std::cerr << "Error: " << Pa_GetErrorText(err) << std::endl;
          //throw std::runtime_error("error playing audio stream");
        }
        out_samps_p += pa_out_config.frames_per_buffer * pa_out_config.channel_count;
        samp_co += pa_out_config.frames_per_buffer * pa_out_config.channel_count;
      }
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
  StreamPortPtr<InterleavedBlockPtr> out_samples;
  PaConfigPC pa_in_config, pa_out_config;
  uint64_t fr_count;
};

}

namespace eat::process {

ProcessPtr make_playcap_audio(const std::string &name, const uint32_t device_num_,
                              const int frames_per_buffer_, const uint32_t sample_rate_) {
  return std::make_shared<AudioPlayCap>(name, device_num_, frames_per_buffer_, sample_rate_);
}

}  // namespace eat::process


//----------------------------------------------------------------------------------------

namespace {

class AudioThrough : public StreamingAtomicProcess {
 public:
  AudioThrough(const std::string &name)
      : StreamingAtomicProcess(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples")) {
  }

  void initialise() override { 
  }

  void process() override {
    while (in_samples->available()) {
      auto in_block = in_samples->pop().read();
      auto &in_description = in_block->info();

      auto out_description = in_description;
      auto out_block = std::make_shared<InterleavedSampleBlock>(out_description);

      for (size_t sample_i = 0; sample_i < out_description.sample_count; sample_i++)
        for (size_t channel_i = 0; channel_i < out_description.channel_count; channel_i++) {
          out_block->sample(channel_i, sample_i) = in_block->sample(channel_i, sample_i);
        }

      out_samples->push(std::move(out_block));
    }
    if (in_samples->eof()) out_samples->close();
  }

  void finalise() override {
  }

 private:
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  StreamPortPtr<InterleavedBlockPtr> out_samples;
};
}

namespace eat::process {

ProcessPtr make_audio_through(const std::string &name) {
  return std::make_shared<AudioThrough>(name);
}

}  // namespace eat::process