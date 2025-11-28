#pragma once
#include <ear/layout.hpp>
#include <ear/dsp/dsp.hpp>
#include <ear/ear.hpp>
#include <adm/adm.hpp>

#include "eat/framework/process.hpp"
#include "rendering_items.hpp"
#include "rendering_items_options_by_id.hpp"
#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"

using namespace eat::framework;
using namespace eat::process;

namespace eat::render {
/// render input audio and samples to channels
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_render(const std::string &name, const ear::Layout &layout, size_t block_size,
                                  const SelectionOptionsId &options = {});

};

class Buffer {
 public:
  Buffer() {}
  Buffer(const Buffer &) = delete;
  Buffer(Buffer &&) = default;
  Buffer(size_t n_channels_, size_t n_samples_) { resize(n_channels_, n_samples_); }

  float *const *ptrs() { return pointers.data(); }

  float *channel_ptr(size_t i) { return pointers.at(i); }

  void zero();

  void add(Buffer &other);

  void resize(size_t n_channels_, size_t n_samples_);

  void from_interleaved(const InterleavedSampleBlock &b);

  InterleavedSampleBlock to_interleaved(unsigned int sample_rate, size_t start = 0);

 private:
  size_t n_channels = 0;
  size_t n_samples = 0;
  std::vector<float> samples;
  std::vector<float *> pointers;
};

namespace eat::render {

// like the rendering items track specs, but specialised for rendering with channel number s rather than track uid
// references
struct RenderDirectTrackSpec {
  size_t track_idx;
};

using RenderTrackSpec = std::variant<RenderDirectTrackSpec, SilentTrackSpec>;

struct ToRenderTrackSpecVisitor {
  RenderTrackSpec operator()(const SilentTrackSpec &spec) noexcept { return spec; }

  RenderTrackSpec operator()(const DirectTrackSpec &spec) noexcept {
    auto id = spec.track->get<adm::AudioTrackUidId>();

    auto it = channel_map.find(id);
    assert(it != channel_map.end());
    return RenderDirectTrackSpec{it->second};
  }

  const channel_map_t &channel_map;
};

struct RenderTrackSpecVisitor {
  void operator()(const RenderDirectTrackSpec &spec) noexcept {
    for (size_t sample_i = 0; sample_i < n_samples; sample_i++) out[sample_i] = in[spec.track_idx][sample_i];
  }

  void operator()(const SilentTrackSpec &) noexcept {
    for (size_t sample_i = 0; sample_i < n_samples; sample_i++) out[sample_i] = 0.0f;
  }

  const float *const *in;
  float *out;
  size_t n_samples;
};

RenderTrackSpec to_render_track_spec(const TrackSpec &spec, const channel_map_t &channel_map);

class ObjectRenderer {
 public:
  ObjectRenderer(const ear::Layout &layout, ear::dsp::block_convolver::Context &convolver_ctx, size_t block_size_)
      : block_size(block_size_),
        n_channels(layout.withoutLfe().channels().size()),
        n_channels_out(layout.channels().size()),
        is_lfe(layout.isLfe()),
        decorrelator_delay(n_channels, static_cast<size_t>(ear::decorrelatorCompensationDelay())),
        gain_calc(layout.withoutLfe()),
        temp_mono(1, block_size),
        temp(n_channels, block_size),
        temp_direct(n_channels, block_size),
        temp_diffuse(n_channels, block_size),
        temp_out(n_channels, block_size) {
    auto decorrelation_filters = ear::designDecorrelators(layout);
    for (size_t i = 0; i < decorrelation_filters.size(); i++) {
      if (!is_lfe.at(i)) {
        auto &filter = decorrelation_filters.at(i);
        ear::dsp::block_convolver::Filter filter_obj(convolver_ctx, filter.size(), filter.data());
        decorrelators.emplace_back(
            std::make_unique<ear::dsp::block_convolver::BlockConvolver>(convolver_ctx, filter_obj));
      }
    }
  }

  void setup_rendering_items(unsigned int fs, const std::vector<std::shared_ptr<ObjectRenderingItem>> &rendering_items,
                             const channel_map_t &channel_map);

  void setup_input_channels(size_t n_in_channels);

  size_t delay() { return static_cast<size_t>(ear::decorrelatorCompensationDelay()); }

  void process(const float *const *in, float *const *out);

 private:
  long int block_start = 0;
  size_t block_size;
  size_t n_channels;  // number of non-LFE channels to be processes internally (size of gains, delays, decorrelators)
  size_t n_channels_out;  // number of channels including LFE
  size_t n_objects;

  std::vector<bool> is_lfe;

  std::vector<RenderTrackSpec> track_specs;

  using InterpType = ear::dsp::LinearInterpVector;
  using GainInterpolator = ear::dsp::GainInterpolator<InterpType>;

  std::vector<GainInterpolator> direct_gain_interpolators;
  std::vector<GainInterpolator> diffuse_gain_interpolators;

  std::vector<std::unique_ptr<ear::dsp::block_convolver::BlockConvolver>> decorrelators;
  ear::dsp::DelayBuffer decorrelator_delay;

  ear::GainCalculatorObjects gain_calc;

  Buffer temp_mono;
  Buffer temp;
  Buffer temp_direct;
  Buffer temp_diffuse;
  Buffer temp_out;
};


class DirectSpeakersRenderer {
 public:
  DirectSpeakersRenderer(const ear::Layout &layout, size_t block_size_)
      : block_size(block_size_),
        n_channels(layout.channels().size()),
        gain_calc(layout),
        temp_mono(1, block_size),
        temp(n_channels, block_size) {}

  void setup_rendering_items(unsigned int fs,
                             const std::vector<std::shared_ptr<DirectSpeakersRenderingItem>> &rendering_items,
                             const channel_map_t &channel_map);

  void setup_input_channels(size_t n_in_channels);

  size_t delay() { return 0; }

  void process(const float *const *in, float *const *out);

 private:
  long int block_start = 0;
  size_t block_size;
  size_t n_channels;
  size_t n_objects;

  std::vector<RenderTrackSpec> track_specs;

  using InterpType = ear::dsp::LinearInterpVector;
  using GainInterpolator = ear::dsp::GainInterpolator<InterpType>;

  std::vector<GainInterpolator> gain_interpolators;

  ear::GainCalculatorDirectSpeakers gain_calc;

  Buffer temp_mono;
  Buffer temp;
};


class HOARenderer {
 public:
  HOARenderer(const ear::Layout &layout, size_t block_size_)
      : block_size(block_size_),
        n_channels(layout.channels().size()),
        gain_calc(layout),
        temp_out(n_channels, block_size) {}

  void setup_rendering_items(unsigned int fs, const std::vector<std::shared_ptr<HOARenderingItem>> &rendering_items,
                             const channel_map_t &channel_map);
 
  void setup_input_channels(size_t n_in_channels);

  size_t delay() { return 0; }

  void process(const float *const *in, float *const *out);

 private:
  long int block_start = 0;
  size_t block_size;
  size_t n_channels;
  size_t n_objects;

  // one vector of track specs per input, containing one track spec per channel
  using RenderTrackSpecs = std::vector<RenderTrackSpec>;
  std::vector<RenderTrackSpecs> track_specs;

  using InterpType = ear::dsp::LinearInterpMatrix;
  using GainInterpolator = ear::dsp::GainInterpolator<InterpType>;

  std::vector<GainInterpolator> gain_interpolators;

  ear::GainCalculatorHOA gain_calc;

  Buffer temp_in;
  Buffer temp_out;
};


class CombinedRenderer {
 public:
  CombinedRenderer(const ear::Layout &layout, ear::dsp::block_convolver::Context &convolver_ctx, size_t block_size_)
      : n_channels(layout.channels().size()),
        block_size(block_size_),
        objects_renderer(layout, convolver_ctx, block_size),
        direct_speakers_renderer(layout, block_size),
        hoa_renderer(layout, block_size),
        objects_comp_delay(n_channels, objects_renderer.delay()),
        temp1(n_channels, block_size),
        temp2(n_channels, block_size) {}

  void setup_input_channels(size_t n_in_channels_);

  void setup_rendering_items(unsigned int fs, const std::vector<std::shared_ptr<RenderingItem>> &rendering_items,
                             const channel_map_t &channel_map);
   
  size_t delay() { return objects_renderer.delay(); }

  void process(const float *const *in, float *const *out);

 private:
  size_t n_channels;
  size_t block_size;
  size_t n_in_channels = std::numeric_limits<size_t>::max();
  ObjectRenderer objects_renderer;
  DirectSpeakersRenderer direct_speakers_renderer;
  HOARenderer hoa_renderer;
  ear::dsp::DelayBuffer objects_comp_delay;  // to align non-objects paths
  Buffer temp1;
  Buffer temp2;
};

};  // namespace eat::render
