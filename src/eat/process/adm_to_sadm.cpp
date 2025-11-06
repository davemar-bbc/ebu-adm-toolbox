#include <adm/write.hpp>
#include <bw64/bw64.hpp>
#include <adm/document.hpp>
#include <adm/segmenter.hpp>
#include <string>
#include <sstream>
#include <unistd.h>

#include "eat/process/adm_bw64.hpp"
#include "eat/process/adm_to_sadm.hpp"

using namespace eat::framework;

namespace eat::process {

class AdmToSadm : public StreamingAtomicProcess {
 public:
  AdmToSadm(const std::string &name, std::chrono::nanoseconds frame_size_)
      : StreamingAtomicProcess(name),
        frame_size(frame_size_),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        in_length(add_in_port<DataPort<uint64_t>>("in_length")),
        out_sadm(add_out_port<StreamPort<std::string>>("out_sadm")) {
        }

  void initialise() override { 
    segment_start = std::make_shared<adm::SegmentStart>(std::chrono::milliseconds(0));
    segment_size = std::make_shared<adm::SegmentDuration>(frame_size);

    // Import and parse ADM document
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();
    filelength = std::chrono::nanoseconds(in_length->get_value());

    // Generate track list
    trackUidList_p = buildTrackList(doc);

    // The audioProgramme start and end overrides the the file length
    for (auto programme : doc->getElements<adm::AudioProgramme>()) {
      if (programme->has<adm::Start>() && programme->has<adm::End>()) {
        filelength = programme->get<adm::End>().get().asNanoseconds() - programme->get<adm::Start>().get().asNanoseconds();
        break;
      } else if (programme->has<adm::End>()) {
        filelength = programme->get<adm::End>().get().asNanoseconds();
        break;
      }
    }

    // Set up the segmenter
    segmenter = std::make_shared<adm::Segmenter>(doc, adm::Time(filelength));
    fr = 0;
  }

  void process() override {
    // If the file hasn't finished
    if (*segment_start < filelength) {
      segmenter->buildFrame(*segment_start, *segment_size, fr);
      auto transportTrackFormat = segmenter->generateTransportTrackFormat(trackUidList_p, 
                                                              *segment_start, *segment_size);
      auto frame = segmenter->getFrame();
      auto frameHeader = segmenter->getFrameHeader();

      std::stringstream xmlStream;
      adm::writeXml(xmlStream, frame, *frameHeader);
    
      std::string sadm_xml = xmlStream.str();
      out_sadm->push(std::move(sadm_xml));

      // Get ready for next frame
      segment_start = std::make_shared<adm::SegmentStart>(segment_start->get() + segment_size->get());
      fr++;
    } else {
      out_sadm->close();
    }
  }

  void finalise() override {
  }

  adm::TrackUidList buildTrackList(std::shared_ptr<adm::Document> document) {
    adm::TrackUidList trackUidList;

    auto atus = document->getElements<adm::AudioTrackUid>();
    uint16_t track_idx = 1;
    for (auto atu : atus) {
      adm::TrackUid track_uid;
      track_uid.uid = formatId(atu->get<adm::AudioTrackUidId>());
      track_uid.trackIndex = track_idx;
      trackUidList.trackUid.push_back(track_uid);
      track_idx++;
    }
    return trackUidList;
  }

 private:
  std::chrono::nanoseconds frame_size;
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<uint64_t> in_length;
  StreamPortPtr<std::string> out_sadm;

  std::chrono::nanoseconds filelength;
  std::shared_ptr<adm::Segmenter> segmenter;
  std::shared_ptr<adm::SegmentStart> segment_start;
  std::shared_ptr<adm::SegmentDuration> segment_size;
  adm::TrackUidList trackUidList_p;
  unsigned int fr;
};

ProcessPtr make_adm_to_sadm(const std::string &name, std::chrono::nanoseconds frame_size_) {
  return std::make_shared<AdmToSadm>(name, frame_size_); 
}

}  // namespace eat::process

