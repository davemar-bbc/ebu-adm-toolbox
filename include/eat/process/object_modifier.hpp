#pragma once

#include <bw64/bw64.hpp>
#include "eat/process/adm_bw64.hpp"
#include "eat/framework/process.hpp"

using namespace bw64;

namespace eat::process {
/// a process which measures the loudness of input samples
/// - in_axml (DataPort<ADMData>) : input axml
framework::ProcessPtr make_object_modifier(const std::string &name);

}  // namespace eat::process

