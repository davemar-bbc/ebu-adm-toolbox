#pragma once

#include <adm/adm.hpp>
#include <bw64/bw64.hpp>
#include <ear/layout.hpp>
#include "eat/process/chna.hpp"

using namespace bw64;
using namespace adm;

namespace eat::process {

struct ObjParam {
  uint16_t importance;
  bool interact;
};

struct ObjParams {
  std::vector<ObjParam> params;
};


class SubDocument {
 public:
  SubDocument() {}

  ear::Layout create(std::shared_ptr<Document> doc, channel_map_t channel_map, 
                     std::shared_ptr<AudioObject> object);

  void print();

  std::shared_ptr<Document> getDoc() { return sub_doc_; }

  std::vector<size_t> getTrackNumList() { return track_num_list_; }

  channel_map_t getChannelMap() { return sub_channel_map_; }

 private:
  size_t recurseObjects(std::shared_ptr<AudioObject> object, int n);

  ObjParams fillParams(std::shared_ptr<AudioObject> object);

  void recurseParents(std::shared_ptr<AudioObject> baby, std::shared_ptr<AudioObject> &child);

  ObjParam getObjParam(ObjParams obj_param_v);

  void addTrackUids(std::shared_ptr<AudioObject> object);

  size_t addPackFormats(std::shared_ptr<AudioObject> object);

  size_t addChannelFormats(std::shared_ptr<AudioPackFormat> pack_format);

  void resolveNewReferences(std::shared_ptr<Document> doc);

  template <typename ElementSrc, typename Element, typename ElementId>
  void resolveRefs(std::shared_ptr<ElementSrc> local_object, 
                              std::shared_ptr<ElementSrc> main_object);

  template <typename Element, typename ElementId>
  void resolveTrackUidRefs(std::shared_ptr<AudioTrackUid> local_object, 
                           std::shared_ptr<AudioTrackUid> main_object);
  
  std::shared_ptr<Document> sub_doc_;
  std::vector<size_t> track_num_list_;
  channel_map_t channel_map_;
  channel_map_t sub_channel_map_;

  std::map<std::shared_ptr<AudioObject>, std::shared_ptr<AudioObject>> obj_parent;
  std::map<std::shared_ptr<AudioObject>, ObjParams> obj_params;
};

}  // namespace eat::process

