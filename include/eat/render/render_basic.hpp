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
#include "eat/render/render.hpp"

using namespace eat::framework;
using namespace eat::process;

namespace eat::render {

class BasicRenderer {
 public:
  BasicRenderer(ADMData in_axml_,
                const ear::Layout &layout, size_t block_size_,
                const SelectionOptionsId &options = {})
      : in_axml(in_axml_),
        selection_options(options),
        block_size(block_size_),
        n_channels(layout.channels().size()),
        convolver_ctx(block_size, ear::get_fft_kiss<float>()),
        renderer(layout, convolver_ctx, block_size) {}

  void initialise();

  void process(std::shared_ptr<const InterleavedSampleBlock> in_block_p, std::shared_ptr<InterleavedSampleBlock> &out_block);

  void finalise(std::shared_ptr<InterleavedSampleBlock> &out_block);

  size_t num_channels() { return n_channels; };

 private:
  ADMData in_axml;
  SelectionOptionsId selection_options;

  bool has_input = false;  // have we received any input blocks? the below
                           // variables are only initialised on the first block
  size_t n_input_channels;
  size_t delay_samples;
  unsigned int sample_rate = 48000;

  size_t n_samples_processed = 0;

  size_t block_size;
  size_t n_channels;

  ear::dsp::block_convolver::Context convolver_ctx;
  CombinedRenderer renderer;

  std::unique_ptr<ear::dsp::VariableBlockSizeAdapter> vbs_adapter;

  Buffer inputs;
  Buffer outputs;
};

};  // namespace eat::render
