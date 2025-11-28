#pragma once

#include <adm/adm.hpp>
#include <bw64/bw64.hpp>
#include "eat/process/adm_bw64.hpp"
#include "eat/framework/process.hpp"

using namespace bw64;
using namespace adm;

namespace eat::process {

struct Level {
  uint16_t lev_num;
  std::vector<std::shared_ptr<AudioProgramme>> programme_list;
  std::vector<std::shared_ptr<AudioContent>> content_list;
  std::vector<std::shared_ptr<AudioObject>> object_list;
};

std::vector<Level> buildLevelList(std::shared_ptr<Document> doc);

}  // namespace eat::process

