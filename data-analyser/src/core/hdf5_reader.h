#ifndef HDF5_READER_H
#define HDF5_READER_H

#include <hdf5.h>
#include <vector>
#include <string>
#include <chrono>

struct IntanHeaderInfo {
    uint32_t magic;
    uint32_t streamCount;
    uint32_t channelCount;
    uint32_t sampleRate;
};

struct SeizureDetectionData {
    std::chrono::system_clock::time_point timestamp;
    uint8_t rawData;
    double confidence;
    double activityLevel;
    double secondaryMetric;
    std::string responseType;
    std::string description;
    int channelIndex; // Channel where detection occurred (0-31)
};

class Hdf5Reader {
public:
    Hdf5Reader();
    ~Hdf5Reader();
    
    // Open HDF5 file for reading
    bool open(const std::string& path);
    
    // Close the file
    void close();
    
    // Read header information
    bool readHeader(IntanHeaderInfo& info);
    
    // Read all seizure detection data from the file
    std::vector<SeizureDetectionData> readSeizureDetections();
    
    // Read data for a specific channel (0-31)
    std::vector<float> readChannelData(int channelIndex);
    
    // Check if file is open
    bool isOpen() const { return file_ != nullptr; }
    
private:
    void* file_;  // hid_t file handle
    void* dset_codes_;  // hid_t dataset handle for codes
    void* dset_uv_;     // hid_t dataset handle for microvolts
    
    // Helper functions
    std::chrono::system_clock::time_point extractTimestampFromFrame(int frameIndex) const;
    std::string responseTypeToString(double confidence, double activityLevel) const;
};

#endif // HDF5_READER_H
