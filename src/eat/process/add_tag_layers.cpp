#include "eat/process/add_tag_layers.hpp"

#include <adm/document.hpp>
#include <adm/adm.hpp>

#include "eat/framework/exceptions.hpp"
#include "eat/process/adm_bw64.hpp"

using namespace eat::framework;

namespace eat::process {

class AddTagLayers : public FunctionalAtomicProcess {
 public:
  AddTagLayers(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    auto tagList = adm::TagList{};
    uint32_t layer = 1;
    adm::TagGroup tagGroup;
    adm::Tag tag{adm::TagClass("urn:profile:production:Layer"), adm::TagValue(std::to_string(layer))};
    tagGroup.add(tag);

    // Add the main element references for the tag layer
    for (auto &apr : doc->getElements<adm::AudioProgramme>()) {
      tagGroup.addReference(apr);
      for (auto &aco : apr->getReferences<adm::AudioContent>()) {
        tagGroup.addReference(aco);
        // No need to recurse to child audioObjects as only the top-level are used in the layer tagging
        for (auto &obj : aco->getReferences<adm::AudioObject>()) {
          tagGroup.addReference(obj);
        }
      }
    }
    tagList.add(tagGroup);
    doc->set(tagList);

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_add_tag_layers(const std::string &name) { return std::make_shared<AddTagLayers>(name); }

class AddImportanceDefaults : public FunctionalAtomicProcess {
 public:
  AddImportanceDefaults(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    for (auto &aco : doc->getElements<adm::AudioContent>()) {
      auto importance = adm::Importance(5);
      if (aco->has<adm::DialogueId>()) {
        if (aco->get<adm::DialogueId>() == adm::Dialogue::DIALOGUE) {
          if (aco->get<adm::DialogueContentKind>() == adm::DialogueContent::UNDEFINED) {
            importance = adm::Importance(6);
          } else if (aco->get<adm::DialogueContentKind>() == adm::DialogueContent::DIALOGUE) {
            importance = adm::Importance(6);
          } else if (aco->get<adm::DialogueContentKind>() == adm::DialogueContent::VOICEOVER) {
            importance = adm::Importance(6);
          } else if (aco->get<adm::DialogueContentKind>() == adm::DialogueContent::SPOKEN_SUBTITLE) {
            importance = adm::Importance(7);
          } else if (aco->get<adm::DialogueContentKind>() == adm::DialogueContent::AUDIO_DESCRIPTION) {
            importance = adm::Importance(7);
          } else if (aco->get<adm::DialogueContentKind>() == adm::DialogueContent::COMMENTARY) {
            importance = adm::Importance(6);
          } else if (aco->get<adm::DialogueContentKind>() == adm::DialogueContent::EMERGENCY) {
            importance = adm::Importance(8);
          } else {
            importance = adm::Importance(6);
          }
        } else if (aco->get<adm::DialogueId>() == adm::Dialogue::NON_DIALOGUE) {
          if (aco->get<adm::NonDialogueContentKind>() == adm::NonDialogueContent::UNDEFINED) {
            importance = adm::Importance(3);
          } else if (aco->get<adm::NonDialogueContentKind>() == adm::NonDialogueContent::MUSIC) {
            importance = adm::Importance(3);
          } else if (aco->get<adm::NonDialogueContentKind>() == adm::NonDialogueContent::EFFECT) {
            importance = adm::Importance(4);
          } else {
            importance = adm::Importance(3);
          }
        } else if (aco->get<adm::DialogueId>() == adm::Dialogue::MIXED) {
          if (aco->get<adm::MixedContentKind>() == adm::MixedContent::UNDEFINED) {
            importance = adm::Importance(5);
          } else if (aco->get<adm::MixedContentKind>() == adm::MixedContent::COMPLETE_MAIN) {
            importance = adm::Importance(7);
          } else if (aco->get<adm::MixedContentKind>() == adm::MixedContent::MIXED) {
            importance = adm::Importance(5);
          } else if (aco->get<adm::MixedContentKind>() == adm::MixedContent::HEARING_IMPAIRED) {
            importance = adm::Importance(5);
          } else {
            importance = adm::Importance(7);
          }
        }
      }

      for (auto &obj : aco->getReferences<adm::AudioObject>()) {
        obj->set(importance);
      }
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_add_importance_defaults(const std::string &name) { return std::make_shared<AddImportanceDefaults>(name); }

}  // namespace eat::process
