#include "eat/process/sub_document.hpp"

#include <adm/adm.hpp>
#include <bw64/bw64.hpp>
#include <adm/utilities/copy.hpp>
#include <adm/write.hpp>
#include <adm/common_definitions.hpp>
#include <adm/utilities/id_assignment.hpp>
#include <ear/bs2051.hpp>

#include <iostream>

using namespace bw64;
using namespace adm;

namespace eat::process {

ear::Layout SubDocument::create(std::shared_ptr<Document> doc, channel_map_t channel_map, 
                                std::shared_ptr<AudioObject> object) {
  sub_doc_ = Document::create();
  adm::addCommonDefinitionsTo(sub_doc_); 
  channel_map_ = channel_map;

  size_t max_chans = recurseObjects(object, 0);

  for (auto &[child, parent] : obj_parent) {
    if (child->getReferences<AudioObject>().size() == 0) {
      auto obj_param_v_par = obj_params[parent];
      auto obj_param_v_ch = obj_params[child];
      obj_param_v_par.params.push_back(obj_param_v_ch.params[0]);
      obj_params[parent] = obj_param_v_par;

      recurseParents(child, parent);
    }
  }

  if (max_chans > 8) max_chans = 8;
  ear::Layout new_layout = ear::getLayout("0+2+0");
  for (const auto& layout : ear::loadLayouts()) {
    if (layout.channels().size() == max_chans) {
      new_layout = layout;
    }
  }

  for (auto content : doc->getElements<AudioContent>()) {
    for (auto object_ref : content->getReferences<AudioObject>()) {
      if (object_ref == object) {
        sub_doc_->add(content->copy());
      }
    }
  }

  resolveNewReferences(doc);

  return new_layout;
}

size_t SubDocument::recurseObjects(std::shared_ptr<AudioObject> object, int n) {
  size_t max_chans = 0;
  auto object_refs = object->getReferences<AudioObject>();
  if (object_refs) {
    for (auto object_ref : object_refs) {
      obj_parent[object_ref] = object;
      size_t num_chans = recurseObjects(object_ref, n + 1);
      if (num_chans > max_chans) max_chans = num_chans; 
    }
  } else {
    obj_params[object] = fillParams(object);
    if (object->getReferences<AudioTrackUid>()) {
      size_t num_chans = addPackFormats(object);
      if (num_chans > max_chans) max_chans = num_chans; 
      addTrackUids(object);
    }
  }
  sub_doc_->add(object->copy());
  return max_chans;
}

ObjParams SubDocument::fillParams(std::shared_ptr<AudioObject> object) {
  bool interact = false;
  if (object->has<Interact>()) {
    if (object->get<Interact>() == true) {
      interact = true;
    } 
  }
  uint16_t importance = 0;
  if (object->has<Importance>()) {
    importance = object->get<Importance>().get();
  }
  ObjParams obj_param_v;
  ObjParam obj_param{importance, interact};
  obj_param_v.params.push_back(obj_param);
  return obj_param_v;
}

void SubDocument::recurseParents(std::shared_ptr<AudioObject> baby, std::shared_ptr<AudioObject> &child) {
  if (obj_parent.count(child)) {
    auto parent = obj_parent[child];

    auto obj_param_v_par = obj_params[parent];
    auto obj_param_v_ch = obj_params[child];
    auto obj_param = getObjParam(obj_param_v_ch);
    obj_param_v_par.params.push_back(obj_param);
    obj_params[parent] = obj_param_v_par;
    recurseParents(baby, parent);
  } else {
    auto obj_param_v_ch = obj_params[child];
    auto obj_param = getObjParam(obj_param_v_ch);

    auto sub_child = sub_doc_->lookup(child->get<AudioObjectId>());
    //sub_child->set(Importance(obj_param.importance));
    sub_child->set(Interact(obj_param.interact));
  }
}

ObjParam SubDocument::getObjParam(ObjParams obj_param_v) {
  uint16_t max_importance = 0;
  bool or_interact = false;
  for (auto param : obj_param_v.params) {
    if (max_importance < param.importance) max_importance = param.importance;
    or_interact |= param.interact;
  }
  return {max_importance, or_interact};
}

void SubDocument::addTrackUids(std::shared_ptr<AudioObject> object) {
  for (auto track_uid : object->getReferences<AudioTrackUid>()) {
    size_t track_num = channel_map_[track_uid->get<AudioTrackUidId>()];
    track_num_list_.push_back(track_num);
    sub_channel_map_.emplace(track_uid->get<AudioTrackUidId>(), track_num);
    sub_doc_->add(track_uid->copy());
  }
}

size_t SubDocument::addPackFormats(std::shared_ptr<AudioObject> object) {
  size_t max_chans = 0;
  for (auto pack_format : object->getReferences<AudioPackFormat>()) {
    size_t num_chans = addChannelFormats(pack_format);
    if (num_chans > max_chans) max_chans = num_chans;
    if (!isCommonDefinitionsId(pack_format->get<AudioPackFormatId>())) {
      sub_doc_->add(pack_format->copy());
    }
  }
  return max_chans;
}

size_t SubDocument::addChannelFormats(std::shared_ptr<AudioPackFormat> pack_format) {
  auto channel_formats = pack_format->getReferences<AudioChannelFormat>();
  for (auto channel_format : channel_formats) {
    if (!isCommonDefinitionsId(channel_format->get<AudioChannelFormatId>())) {
      sub_doc_->add(channel_format->copy());
    }
  }
  return channel_formats.size(); 
}

void SubDocument::resolveNewReferences(std::shared_ptr<Document> doc) {
  // AudioContents
  for (auto local_content : sub_doc_->getElements<AudioContent>()) {
    for (auto main_content : doc->getElements<AudioContent>()) {
      if (main_content->get<AudioContentId>() == local_content->get<AudioContentId>()) {
        resolveRefs<AudioContent, AudioObject, AudioObjectId>(local_content, main_content);
      }
    }
  }
  // AudioObjects
  for (auto local_object : sub_doc_->getElements<AudioObject>()) {
    for (auto main_object : doc->getElements<AudioObject>()) {
      if (main_object->get<AudioObjectId>() == local_object->get<AudioObjectId>()) {
        resolveRefs<AudioObject, AudioObject, AudioObjectId>(local_object, main_object);
        resolveRefs<AudioObject, AudioTrackUid, AudioTrackUidId>(local_object, main_object);
        resolveRefs<AudioObject, AudioPackFormat, AudioPackFormatId>(local_object, main_object);
      }
    }
  }
  // AudioPackFormats
  for (auto local_pack : sub_doc_->getElements<AudioPackFormat>()) {
    for (auto main_pack : doc->getElements<AudioPackFormat>()) {  
      if (main_pack->get<AudioPackFormatId>() == local_pack->get<AudioPackFormatId>()) {
        resolveRefs<AudioPackFormat, AudioChannelFormat, AudioChannelFormatId>(local_pack, main_pack);
      }
    }
  }
  // AudioTrackUids
  for (auto local_trackuid : sub_doc_->getElements<AudioTrackUid>()) {
    for (auto main_trackuid : doc->getElements<AudioTrackUid>()) {  
      if (main_trackuid->get<AudioTrackUidId>() == local_trackuid->get<AudioTrackUidId>()) {
        resolveTrackUidRefs<AudioChannelFormat, AudioChannelFormatId>(local_trackuid, main_trackuid);
        resolveTrackUidRefs<AudioPackFormat, AudioPackFormatId>(local_trackuid, main_trackuid);
      }
    }
  }
}

template <typename ElementSrc, typename Element, typename ElementId>
void SubDocument::resolveRefs(std::shared_ptr<ElementSrc> local_object, 
                              std::shared_ptr<ElementSrc> main_object) {
  for (const auto& main_ref : main_object->template getReferences<Element>()) {
    for (auto local_ref : sub_doc_->template getElements<Element>()) {
      if (main_ref->template get<ElementId>() == local_ref->template get<ElementId>()) {
        // Ensures reference isn't added that's already there
        auto refs = local_object->template getReferences<Element>();
        bool exist = false;
        for (auto ref : refs) {
          if (ref->template get<ElementId>() == local_ref->template get<ElementId>()) exist = true;
        }
        if (!exist) {
          local_object->addReference(local_ref);
        }
      }
    }
  }
}

template <typename Element, typename ElementId>
void SubDocument::resolveTrackUidRefs(std::shared_ptr<AudioTrackUid> local_object, 
                              std::shared_ptr<AudioTrackUid> main_object) {
  auto main_ref = main_object->template getReference<Element>();
  for (auto local_ref : sub_doc_->template getElements<Element>()) {
    if (main_ref->template get<ElementId>() == local_ref->template get<ElementId>()) {
      local_object->setReference(local_ref);
    }
  }
}

void SubDocument::print() {
  std::stringstream xmlStream;
  writeXml(xmlStream, sub_doc_);
  std::cout << xmlStream.str();
}

}  // namespace eat::process
