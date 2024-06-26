#pragma once

#include <adm/adm.hpp>
#include "eat/process/track_object.hpp"

#include <adm/route_tracer.hpp>

using namespace adm;

namespace eat::process {

class BlockObject {
  public:
    /**
     * Constructor
     */
    BlockObject() {}

    /**
     * Gets the old and new start and end times for the audioObject
     *
     * @param ao             audioObject
     * @param track_objects  track objects
     */
    void getNewTimes(std::shared_ptr<AudioObject> ao, std::vector<TrackObject> track_objects);

    /**
     * Get the new start offset
     *
     * @return  new start offset
     */
    std::chrono::nanoseconds getNewOffStart() { return new_start_ - old_start_; }

    /**
     * Get the new end offset
     *
     * @return  new end offset
     */
    std::chrono::nanoseconds getNewOffEnd() { return new_end_ - old_start_; }

    /**
     * Get the block end time
     *
     * @return  block end time
     */
    std::chrono::nanoseconds getBlockEnd() { return old_end_ - old_start_; }

  public:
    std::chrono::nanoseconds old_start_;    // Old audioObject start time
    std::chrono::nanoseconds old_end_;      // Old audioObject end time
    std::chrono::nanoseconds new_start_;    // New audioObject start time
    std::chrono::nanoseconds new_end_;      // New audioObject end time
    bool remove_;                           // True if audioObject is to be removed
};


class BlockModify {
  public:
    /**
     * Constructor
     *
     * @param adm            ADM document
     * @param track_objects  track objects
     */
    BlockModify(std::shared_ptr<Document> adm, std::vector<TrackObject> track_objects)
          : adm_{adm}, track_objects_{track_objects}
      { }

    /**
     * Modify the audioBlockFormats that may need changing due to audioObject shrinkage.
     * 
     * @param file_length_ns   File length in ns
     */
    void modifyBlocks(uint64_t file_length_ns);

    /**
     * Check if this channel is referenced from other audioObjects if it's getting removed
     * 
     * @param ao   audioObject to find shared channel
     * @param acf  audioChannelFormat to be checked
     * 
     * @return  true if channel is shared
     */
    bool findSharedChannelFormat(std::shared_ptr<AudioObject> ao,
                                 std::shared_ptr<AudioChannelFormat> &acf);

    /**
     * Remove the audioChannelFormat, audioStreamFormat and audioTrackFormat
     * 
     * @param acf  audioChannelFormat to remove
     */
    void removeChannelStreamTrack(std::shared_ptr<AudioChannelFormat> &acf);

    /**
     * Get the maximum ID values for audioPackFormat and audioCHannelFormat.
     * This so that when new audioPackFormat and audioCHannelFormat elements are generated they can
     * be given appropriate ID values.
     */
    void getMaxIds();

    /**
     * If an audioObject has been split into more than one audioObject, then the audioPackFormats
     * (and their audioCHannelFormats) will need to be duplicated to allow them to be suitably modified.
     */
    void duplicatePacks();

    /**
     * Modifies the audioTrackUIDs with new audioPackFormats and audioTrackFormats.
     *
     * @param ao             The audioObject that references the audioTrackUIDs
     * @param apf_new        The new audioPackFormat that needs adding to the audioTrackUID.
     */
    void modifyTrackUIDs(std::shared_ptr<AudioObject> &ao, std::shared_ptr<AudioPackFormat> apf_new);

    /**
     * Generates the audioStreamFormat and audioTrackFormat from the audioChannelFormat
     *
     * @param acf_new        The new audioChannelFormat that will be copied from.
     * @param atf_new        The new audioTrackFormat that will be generated.
     */
    void generateStreamTrackFormats(std::shared_ptr<AudioChannelFormat> acf_new,
                                    std::shared_ptr<AudioTrackFormat> &atf_new);

    /**
     * Copy an audioPackFormat and audioCHannelFormats and given them new IDs.
     */
    std::shared_ptr<AudioPackFormat> copyPack(std::shared_ptr<AudioPackFormat> apf_ref, uint32_t apf_ind);

  private:
    std::shared_ptr<Document> adm_;            // New ADM document to be generated
    std::vector<TrackObject> track_objects_;   // A vector of the track_objects that will get populated
    BlockObject block_object_;                 // Block object containing times
    AudioPackFormatIdValue max_apf_idv_;       // Maximum audioPackFormat ID Value
    AudioChannelFormatIdValue max_acf_idv_;    // Maximum audioChannelFormat ID Value
};


class BlockHandler {
  private:
    struct BlockInfo {
      std::chrono::nanoseconds block_start;    // Start time of block
      std::chrono::nanoseconds block_end;      // End time of block
      bool keep;                               // Flag to define whether the block is kept or not
      bool inf_end;                            // Flag to indicate if the block has an infinite end time
    };

  public:
    /**
     * Constructor
     *
     * @param block_object    Block object
     */
    BlockHandler(BlockObject block_object)
              : block_object_{block_object}
      { }

    /**
     * Deal wuth the AudioChannelFormat that contains the audioBlockFormats that need changing
     *
     * @param acf           audioCHannelFormat to be handled
     */
    template<typename T>
    void handleChannel(std::shared_ptr<AudioChannelFormat> &acf);

    /**
     * Deal wuth the AudioChannelFormat that contains the audioBlockFormats that need changing
     *
     * @param abfs          Input audioBlockFormats to be modified
     * @param new_abfs      Output new audioBlockFormats that have been modified
     */
    template<typename T1, typename T2>
    void handleBlocks(T1 &abfs, std::vector<T2> &new_abfs);

    /**
     * Generate the build_info_ vector.
     *
     * @param abfs          Input audioBlockFormats to be modified
     */
    template<typename T>
    void buildBlockInfo(T &abfs);

    /**
     * Adjust the block_info_ entries depending on whether that block is to be discarded or reduced in size.
     */
    void adjustBlockInfo();

    /**
     * Generate a set of new audioBlockFormats based on the new block_info_ and from the old audioBlockFormats.
     *
     * @param abfs          Input audioBlockFormats to be modified
     * @param new_abfs      Output new audioBlockFormats that have been modified
     */
    template<typename T1, typename T2>
    void setNewBlocks(T1 &abfs, std::vector<T2> &new_abfs);

    void fixInterpolationLengths(std::shared_ptr<AudioChannelFormat> &acf);

   private:
     BlockObject block_object_;                 // Block object
     std::vector<BlockInfo> block_infos_;       // Vector of BlockInfo, one for each audioBlockFormat
     std::chrono::nanoseconds new_off_start_;   // New offset start time for audioObject
     std::chrono::nanoseconds new_off_end_;     // New offset end time for audioObject
};

} // namespace: eat::process
