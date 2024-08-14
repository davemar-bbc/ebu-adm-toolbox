#pragma once

#include <adm/adm.hpp>
#include <bw64/bw64.hpp>
#include "eat/framework/process.hpp"
#include "eat/process/chna.hpp"
#include "eat/process/track_analyser.hpp"

using namespace adm;
using namespace bw64;

namespace eat::process {

struct TrackInfo {
  std::string audio_track_uid;
  uint32_t track_index;
};

struct ProdProfLimits {
  int64_t obj_min_gap_ns;   // Minimum gap between objects that can be combined
  int64_t obj_lead_in_ns;   // Minimum lead in time
  int64_t obj_lead_out_ns;  // Minimum lead out time
  int64_t obj_min_dur_ns;   // Minimum object duration
  int64_t obj_max_gap_ns;   // Maximum duration of silence in object
  bool crop_objs;           // Enforce cropping of objects so they aren't longer than original
};

class TrackObject {
  public:
    /**
     * Constructor
     */
    TrackObject();

    /**
     * Analyses the ADM and chna chunk.
     *
     * @param adm         Input ADM document
     * @param chna_chunk  Input chna chunk
     */
    void getAdmInfo(std::shared_ptr<const AudioObject> ao,
                    channel_map_t channel_map,
                    BlockActive block_active);

    /**
     *  Prints some diagnostic info
     */
    void print();

    /**
     * Finds the object starting point based on the activity from the track analyser
     *
     * @param i               Index to track analyser activity blocks (gets incremented)
     * @param new_start       The new start time for the object
     * @param track_analyser  The track analyser
     * @return                True if successful
     */
    bool findStart(uint32_t &i, int64_t &new_start, BlockActive block_active);

    /**
     * Finds the object end point based on the activity from the track analyser
     *
     * @param i               Index to track analyser activity blocks (gets incremented)
     * @param new_end         The new end time for the object
     * @param track_analyser  The track analyser
     * @return                True if successful
     */
    bool findEnd(uint32_t &i, int64_t &new_end, BlockActive block_active);

    /**
     * Checks an audioObjectID matches the ID for the object track
     *
     * @param ao              audioObject to check
     * @return                True if matched
     */
    bool checkObjectMatch(std::shared_ptr<const AudioObject> ao) { return audio_object_id_ == formatId(ao->get<AudioObjectId>()); };

    /**
     * Gets the start time from the vector of start times
     *
     * @param ind             Index to the vector
     * @return                Start time
     */
    std::chrono::nanoseconds start(const uint32_t ind) const { return starts_[ind]; };

    /**
     * Gets the end time from the vector of end times
     *
     * @param ind             Index to the vector
     * @return                End time
     */
    std::chrono::nanoseconds end(const uint32_t ind) const { return ends_[ind]; };

    /**
     * Gets the size of the start vector
     *
     * @return                Start vector size
     */
    uint32_t startSize() { return starts_.size(); };

    /**
     * Gets the size of the end vector
     *
     * @return                End vector size
     */
    uint32_t endSize() { return ends_.size(); };

    /**
     * Adds a new start time to the start vector
     *
     * @param ns              Start time in nanoseconds
     */
    void addNewStart(int64_t ns) { starts_.push_back(std::chrono::nanoseconds(ns)); };

    /**
     * Adds a new end time to the end vector
     *
     * @param ns              End time in nanoseconds
     */
    void addNewEnd(int64_t ns) { ends_.push_back(std::chrono::nanoseconds(ns)); };

    /**
     * Finds the longest track
     *
     * @returns  Track index              
     */
    uint32_t findMaxTrack();

    /**
     * Get a trackInfo entry
     *
     * @param tind            Index of the required track
     * 
     * @returns  Track index              
     */
    TrackInfo getTrackInfo(const uint32_t tind);

    /**
     * Get the start end ends of each block
     *
     * @param block_active    BlockActive data            
     */
    void getStartsAndEnds(BlockActive block_active);

    /**
     * Merge any objects that are close together, modifying the start and end vectors.
     *
     * @param new_starts      Start times of objects
     * @param new_ends        End times of objects
     * @param min_gap_ns      Minimum gap between object in ns.          
     */
    void mergeCloseObjects(std::vector<int64_t> &new_starts, 
                           std::vector<int64_t> &new_ends, int64_t min_gap_ns);

    /**
     * Get the size of the vector of new_audio_objects
     * 
     * @returns  Size of vector              
     */
    size_t newAudioObjectIdsSize() { return new_audio_object_ids_.size(); };

    /**
     * Get a new_audio_object from vector
     * 
     * @param i               Index to vector
     * 
     * @returns  new_audio_object              
     */
    std::string newAudioObjectId(size_t i) { return new_audio_object_ids_[i]; };

    /**
     * Add a new_audio_object to vector
     * 
     * @param id              audioOBject ID        
     */
    void addNewAudioObjectId(std::string id) { new_audio_object_ids_.push_back(id); };

    /**
     * Get an audio_object
     * 
     * @returns  audio_object              
     */
    std::string getAudioObjectId() { return audio_object_id_; };

    /**
     * Get the remove flag
     * 
     * @returns  remove true/false              
     */
    bool getRemove() { return remove_; };

    /**
     * Set the remove flag to true
    */
    void setRemove() { remove_ = true; };

  private:
    std::string audio_object_id_;                   // audioObject ID for this object
    std::vector<std::chrono::nanoseconds> starts_;  // vector of start times for the object
    std::vector<std::chrono::nanoseconds> ends_;    // vector of end times for the object
    std::vector<TrackInfo> track_info_;             // vector of track information
    std::vector<std::string> new_audio_object_ids_; // The new audioObject IDs that have been generated
    bool remove_;                                   // Set to true to remove audioObject 
    ProdProfLimits prod_prof_limits_;               // Production profile limits
};

}  // namespace eat::framework
