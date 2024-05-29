/**
 *  \file sadmbuilder.hpp
 */

#pragma once

//#include <adm/frame.hpp>
#include <adm/document.hpp>
#include <adm/write.hpp>
#include <adm/serial/frame_header.hpp>
#include <adm/serial/transport_track_format.hpp>
//#include <adm/serial.hpp>
#include <vector>

//#include "admosc/admosc_model.hpp"

using namespace adm;

struct BlockParams {
  std::chrono::nanoseconds lstart;
  std::chrono::nanoseconds lduration;
  bool cartesian;
  float azim;
  float elev;
  float dist;
  float x;
  float y;
  float z;
  float s;
};

struct TrackMap {
  uint32_t track_num;
  std::string object_name;
  uint32_t chan_num;
};

/**
 * S-ADM Builder class.
 */
class SadmBuilder {
 public:
  /**
   * Constructor
   *
   * @param frame_size_ns
   * @param track_map_fname
   */
  SadmBuilder(int64_t frame_size_ns, std::string const track_map_fname);

  /**
   * Loads the track map csv file
   *
   * @param map_fname   CSV filename
   */
  void loadTrackMap(std::string map_fname);

  /**
   * Initialises the S-ADM frame object
   *
   * @param frame_num     Frame number
   * @param frame_pos_ns  Frame position in ns
   */
  void buildSadm(uint32_t frame_num, int64_t frame_pos_ns);

  /**
   * Build an audioChannelFormat element with audioBlockFormats populated
   *
   * @param channel_format
   * @param track_num
   */
  void buildChannel(std::shared_ptr<AudioChannelFormat> channel_format, uint32_t track_num);

  /**  
   * Get the frame number
   * 
   * @return frame number 
   */
  uint64_t getFrameNumber();

  /**
   * Turns frame object into XML and outputs to a file.
   */
  void outputFrame();

  /**
   * Turns frame object into XML and returns string.
   *
   * @return  XML string
   */
  std::string frameToString();

  /**
   * Return the frame document
   * 
   * @return std::shared_ptr<Document> 
   */
  std::shared_ptr<Document> frame() const { return frame_; };

  /**
   * Return the frame header
   * 
   * @return std::shared_ptr<FrameHeader> 
   */
  std::shared_ptr<FrameHeader> frameHeader() const { return frame_header_; };

  std::shared_ptr<TransportTrackFormat> buildTracks();

  void buildTrackUid(std::shared_ptr<AudioChannelFormat> channel_format, 
                     std::shared_ptr<AudioObject> object,
                     std::shared_ptr<AudioPackFormat> pack_format, 
                     std::shared_ptr<TransportTrackFormat> track_format,
                     TrackMap track_map);

  void getBaseAdm(std::string adm_fname);

  void copyBaseToFrame();

  std::vector<double> gainValues() const;

  void addTransportTrackFormat(std::shared_ptr<TransportTrackFormat> track_format) {
    frame_header_->add(*track_format);
  }

  std::string baseToString() {
    // generate SADM xml
    std::stringstream xml_stream;
    writeXml(xml_stream, adm_base_);
    return xml_stream.str();
  }

 private:

  /**
   * Sets the audioChannelFormatID to the value set by 'num'. The type definition is set to Objects,
   * so will be AC_00031xxx.
   *
   * @param num  The value for the ID.
   * @return AudioChannelFormatId
   */
  AudioChannelFormatId setChannelFormatId(uint32_t num);

  std::shared_ptr<Document> frame_;                     /// S-ADM frame object
  std::shared_ptr<FrameHeader> frame_header_;
  int64_t frame_size_ns_;                           /// Frame size in ns
  uint32_t num_ch_;                                  /// Number of channels

  std::shared_ptr<Document> adm_base_;
  std::vector<TrackMap> track_maps_;

  int func_co;
 protected:
  std::vector<std::vector<BlockParams>> block_vec_;  /// Table of block values  
};
