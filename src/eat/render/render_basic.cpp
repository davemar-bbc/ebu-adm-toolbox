#include "eat/render/render_basic.hpp"
#include "eat/render/render.hpp"

#include <adm/document.hpp>
#include <adm/utilities/time_conversion.hpp>
#include <ear/dsp/dsp.hpp>
#include <ear/ear.hpp>
#include <limits>
#include <string>

#include "eat/framework/exceptions.hpp"
#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"
#include "eat/render/rendering_items.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace eat::render;

namespace eat::render {

void BasicRenderer::initialise() {
  auto adm = in_axml;
  auto doc = adm.document.move_or_copy();

  auto selection_options_ref = selection_options_from_ids(doc, selection_options);

  SelectionResult result = select_items(doc, selection_options_ref);

  renderer.setup_rendering_items(sample_rate, result.items, adm.channel_map);

  n_samples_processed = 0;
  has_input = false;
}

void BasicRenderer::process(std::shared_ptr<const InterleavedSampleBlock> in_block_p, 
                            std::shared_ptr<InterleavedSampleBlock> &out_block) {
  auto in_block = in_block_p;
  auto &info = in_block->info();

  if (info.sample_rate != sample_rate)
    throw std::runtime_error("sample rate must be " + std::to_string(sample_rate));

  if (!has_input) {
    n_input_channels = info.channel_count;
    renderer.setup_input_channels(n_input_channels);
    vbs_adapter = std::make_unique<ear::dsp::VariableBlockSizeAdapter>(
        block_size, n_input_channels, n_channels,
        [this](const float *const *in, float *const *out) { renderer.process(in, out); });

    delay_samples = renderer.delay() + static_cast<size_t>(vbs_adapter->get_delay());
    has_input = true;
  } else {
    always_assert(n_input_channels == info.channel_count, "number of samples changed while rendering");
  }

  inputs.from_interleaved(*in_block);
  outputs.resize(n_channels, info.sample_count);

  vbs_adapter->process(info.sample_count, inputs.ptrs(), outputs.ptrs());

  // only push output if the block extends past the negative delay period
  if (n_samples_processed + info.sample_count > delay_samples) {
    size_t start = n_samples_processed > delay_samples ? 0 : delay_samples - n_samples_processed;
    out_block = std::make_shared<InterleavedSampleBlock>(outputs.to_interleaved(sample_rate, start));
  }

  n_samples_processed += info.sample_count;
}

void BasicRenderer::finalise(std::shared_ptr<InterleavedSampleBlock> &out_block) {
  // feed through silence to make up for the negative delay
  if (has_input && n_samples_processed) {
    inputs.resize(n_input_channels, delay_samples);
    inputs.zero();
    outputs.resize(n_channels, delay_samples);

    vbs_adapter->process(delay_samples, inputs.ptrs(), outputs.ptrs());
    size_t start = n_samples_processed > delay_samples ? 0 : delay_samples - n_samples_processed;
    out_block = std::make_shared<InterleavedSampleBlock>(outputs.to_interleaved(sample_rate, start));
  }
}

}  // namespace eat::render
