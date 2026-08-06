#pragma once
#include "eat/framework/process.hpp"

namespace eat::process {
/// add layers in tagList
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_add_tag_layers(const std::string &name);

/// add some default importance values to audioObjects for Production Profile
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_add_importance_defaults(const std::string &name);

}  // namespace eat::process
