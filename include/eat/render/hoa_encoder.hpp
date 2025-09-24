#pragma once
#include <ear/layout.hpp>

#include "eat/framework/process.hpp"
#include "rendering_items_options_by_id.hpp"

namespace eat::render {
/// render input audio and samples to channels
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_hoa_encoder(const std::string &name, const size_t hoa_order, size_t block_size,
                                  const SelectionOptionsId &options = {});

};  // namespace eat::render
