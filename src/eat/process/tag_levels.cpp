#include "eat/process/tag_levels.hpp"
#include "eat/process/sub_document.hpp"

#include <adm/adm.hpp>
#include <bw64/bw64.hpp>
#include <adm/utilities/copy.hpp>
#include <adm/write.hpp>
#include <adm/common_definitions.hpp>

#include "eat/process/adm_bw64.hpp"
#include "eat/process/block_handler.hpp"
#include "eat/process/block.hpp"
#include "eat/render/render_basic.hpp"

#include <iostream>

using namespace bw64;
using namespace adm;
using namespace eat::framework;
using namespace eat::render;

namespace eat::process {

std::vector<Level> buildLevelList(std::shared_ptr<Document> doc) {
  std::vector<Level> levels;

  auto tag_list = doc->getElement<TagList>();
  if (tag_list->has<TagGroups>()) {
    auto tag_groups = tag_list->get<TagGroups>();
    // Loop through the tagGroups
    for (auto tag_group : tag_groups) {
      auto tags = tag_group.get<TTags>();
      uint16_t lev_val = 0;
      // Find the level values in the tags
      for (auto tag : tags) {
        if (tag.has<TTagClass>()) {
          if (tag.get<TTagClass>() == "urn:profile:production:Layer") {
            if (tag.has<TTagValue>()) {
              lev_val = std::stoi(tag.get<TTagValue>().get());
            }
          }
        }
      }
      // Build up the list of the referenced elements for each level
      if (lev_val > 0) {
        bool exists = false;
        for (auto level : levels) {
          if (level.lev_num == lev_val) {
            exists = true;
          }
        }
        if (!exists) {
          Level level;
          level.lev_num = lev_val;
          // Generate the lists of the top-level elements for this level
          for (auto &programme : tag_group.getReferences<AudioProgramme>()) {
            level.programme_list.push_back(programme);
          }
          for (auto &content : tag_group.getReferences<AudioContent>()) {
            level.content_list.push_back(content);
          }
          for (auto &object : tag_group.getReferences<AudioObject>()) {
            level.object_list.push_back(object);
          }
          levels.push_back(level);
        } else {
          // Should never get here
          std::cerr << "Level already exists " << std::endl;
        }
      }
    }
  }
  return levels;
}

}  // namespace eat::process
