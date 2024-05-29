#include "eat/process/sadmbuilder.hpp"

//#include <fmt/format.h>
#include <adm/document.hpp>
#include <adm/elements.hpp>
#include <adm/serial.hpp>
#include <adm/parse.hpp>
#include <adm/utilities/id_assignment.hpp>
#include <adm/utilities/copy.hpp>
#include <adm/write.hpp>
#include <adm/serial/frame_header.hpp>
#include <adm/serial/transport_track_format.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>


using namespace adm;

SadmBuilder::SadmBuilder(int64_t frame_size_ns, std::string const track_map_fname) {
  frame_size_ns_ = frame_size_ns;

  loadTrackMap(track_map_fname);

  for (uint32_t ch = 0; ch < num_ch_; ch++) {
    std::vector<BlockParams> blocks;
    BlockParams block = {std::chrono::nanoseconds(0), std::chrono::nanoseconds(0), false, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0};
    blocks.push_back(block);
    block_vec_.push_back(blocks);
  }

  func_co = 0;
}

void SadmBuilder::loadTrackMap(std::string map_fname) {
  std::string line, word;
  num_ch_ = 0;

  std::fstream csv_file(map_fname, std::ios::in);
  if (csv_file.is_open()) {
    while (getline(csv_file, line)) {
      std::stringstream ss(line);
      TrackMap track_map;
      if (getline(ss, word, ',')) {
        track_map.track_num = static_cast<uint32_t>(std::stoi(word));
      }
      if (getline(ss, word, ',')) {
        track_map.object_name = word;
      }
      if (getline(ss, word, ',')) {
        track_map.chan_num = static_cast<uint32_t>(std::stoi(word));
        track_maps_.push_back(track_map);
        num_ch_++;
      }
    }
  } else {
    std::cerr << "Can't open file: " << map_fname << std::endl;
    exit(-1);
  }

  //for (auto t : track_maps_) {
  //  std::cout << t.track_num << " " << t.object_name << " " << t.chan_num << std::endl;
  //}
}

void SadmBuilder::buildSadm(uint32_t frame_num, int64_t frame_pos_ns) {
  FrameIndex fr_index = FrameIndex(frame_num + 1);
  auto uuid = "1f399874-dfa9-4a9b-82cd-fedc483d1223";
  auto frameFormatId = FrameFormatId(fr_index); 
  frame_header_ = std::make_shared<FrameHeader>(FrameFormat{
      frameFormatId, Start(std::chrono::nanoseconds(frame_pos_ns)),
      Duration(std::chrono::nanoseconds(frame_size_ns_)), FrameType::FULL, FlowId{uuid}});
  frame_ = Document::create();

  copyBaseToFrame();

  // Update blocks by keeping the last blocks from the previous frames
  for (uint32_t ch = 0; ch < num_ch_; ch++) {
    BlockParams prev_block = block_vec_[ch].back();
    prev_block.lstart = std::chrono::nanoseconds(0);
    prev_block.lduration = std::chrono::nanoseconds(0);
    block_vec_[ch].clear();
    block_vec_[ch].push_back(prev_block);
  }
}


void SadmBuilder::buildChannel(std::shared_ptr<AudioChannelFormat> channel_format, uint32_t track_num) {
  if(track_num - 1 < block_vec_.size()) {
    for (uint32_t i = 0; i < block_vec_[track_num - 1].size(); i++) {
      auto block = block_vec_[track_num - 1][i];
      // Set the position for the new block
      CartesianPosition pos_c(X(block.x), Y(block.y), Z(block.z));
      SphericalPosition pos_s(Azimuth(block.azim), Elevation(block.elev), Distance(block.dist));
      AudioBlockFormatObjects block_format(pos_s);
      if (block.cartesian) {
        block_format.set(Cartesian(true));
        block_format.set(pos_c);
      }

      // Set the timing parameters
      auto lstart = block.lstart;
      auto lduration = block.lduration;
      if (i == 0) {
        block_format.set(InitializeBlock(true));
      } else {
        block_format.set(Rtime(lstart));
        block_format.set(Duration(lduration));
      }

      // Add the block to the channel
      channel_format->add(block_format);
    }

    // Renumber the block counters as we need first block to be zero (doesn't work in above loop)
    uint32_t id_co = 0;
    for (auto &block_format : channel_format->getElements<AudioBlockFormatObjects>()) {
      auto block_format_id = block_format.get<AudioBlockFormatId>();
      auto block_format_id_co = AudioBlockFormatIdCounter(id_co++);
      block_format_id.set(block_format_id_co);
      block_format.set(block_format_id);
    }
  }
}


AudioChannelFormatId SadmBuilder::setChannelFormatId(uint32_t num) {
  AudioChannelFormatId channel_format_id;
  auto channel_format_id_value = AudioChannelFormatIdValue(0x1000u + num);
  channel_format_id.set(channel_format_id_value);
  channel_format_id.set(TypeDefinition::OBJECTS);
  return channel_format_id;
}

uint64_t SadmBuilder::getFrameNumber() {
  // Get the frame number from the frame ID.
  auto frame_format = frame_header_->get<FrameFormat>();
  auto frame_id = frame_format.get<FrameFormatId>();
  auto frame_num = frame_id.get<FrameIndex>().get();
  return static_cast<uint64_t>(frame_num);
}

void SadmBuilder::outputFrame() {
  // generate SADM xml
  auto xml_str = frameToString();

  // write XML data to a file
  char fname[100];
  snprintf(fname, sizeof(fname), "/tmp/tmp_sadm_%05llu.xml", getFrameNumber());
  std::ofstream opfile;
  opfile.open(fname);
  opfile << xml_str;
  opfile.close();
}

std::string SadmBuilder::frameToString() {
  // generate SADM xml
  std::stringstream xml_stream;
  writeXml(xml_stream, frame_, *frame_header_);
  return xml_stream.str();
}

void SadmBuilder::getBaseAdm(std::string adm_fname) {
  adm_base_ = adm::parseXml(adm_fname.c_str(), adm::xml::ParserOptions::recursive_node_search);

  // Clear out audioTrackFormats that aren't needed
  std::vector<std::shared_ptr<AudioTrackFormat>> tf_vec;
  for (const auto& track_format : adm_base_->getElements<AudioTrackFormat>()) {
    tf_vec.push_back(track_format);
  }
  for (auto tf : tf_vec) {
    if (!adm_base_->remove(tf)) std::cout << "FAIL";
  }

  // Clear out audioStreamFormats that aren't needed
  std::vector<std::shared_ptr<AudioStreamFormat>> sf_vec;
  for (const auto& stream_format : adm_base_->getElements<AudioStreamFormat>()) {
    sf_vec.push_back(stream_format);
  }
  for (auto sf : sf_vec) {
    if (!adm_base_->remove(sf)) std::cout << "FAIL";
  }
}

std::shared_ptr<TransportTrackFormat> SadmBuilder::buildTracks() {
  auto track_format = std::make_shared<TransportTrackFormat>(TransportId(TransportIdValue(1)));

  for (auto track_map : track_maps_) {
    for (auto &object : adm_base_->getElements<AudioObject>()) {
      if (track_map.object_name == object->get<AudioObjectName>().get()) {
        auto pack_formats = object->getReferences<AudioPackFormat>();
        if (pack_formats.size() == 1) {
          auto channel_formats = pack_formats[0]->getReferences<AudioChannelFormat>();
          if (!isCommonDefinitionsId(pack_formats[0]->get<AudioPackFormatId>())) {
            for (auto channel_format : channel_formats) {
              std::string channel_name = track_map.object_name + "_" + std::to_string(track_map.chan_num);
              if (channel_format->get<AudioChannelFormatName>().get() == channel_name) {
                buildChannel(channel_format, track_map.track_num);
                buildTrackUid(channel_format, object, pack_formats[0], track_format, track_map);
              }
            }
          } else {
            // Do common definitions
            auto channel_format = channel_formats[track_map.chan_num - 1];
            buildTrackUid(channel_format, object, pack_formats[0], track_format, track_map);
          }
        }
      }
    }
  }
  return track_format;
}

void SadmBuilder::buildTrackUid(std::shared_ptr<AudioChannelFormat> channel_format,
                                std::shared_ptr<AudioObject> object,
                                std::shared_ptr<AudioPackFormat> pack_format, 
                                std::shared_ptr<TransportTrackFormat> track_format,
                                TrackMap track_map) {
  std::shared_ptr<AudioTrackUid> track_uid = AudioTrackUid::create(AudioTrackUidIdValue(track_map.track_num));
  track_uid->setReference(channel_format);
  track_uid->setReference(pack_format);
  object->addReference(track_uid);

  AudioTrack audio_track{TrackId(track_map.track_num)};
  audio_track.add(track_uid->get<AudioTrackUidId>());
  track_format->add(audio_track);
}

void SadmBuilder::copyBaseToFrame() {
  func_co++;
  frame_ = deepCopy(adm_base_);
  //std::cout << "func_co: " << func_co << std::endl;
}

std::vector<double> SadmBuilder::gainValues() const {
  std::vector<double> gains;
  gains.reserve(block_vec_.size());
  std::transform(block_vec_.begin(), block_vec_.end(), std::back_inserter(gains),
                 [](auto const& blocks) {
                   return blocks.back().s;
  });
  return gains;
}