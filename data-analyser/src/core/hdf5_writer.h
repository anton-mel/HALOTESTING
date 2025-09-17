#ifndef HDF5_WRITER_H
#define HDF5_WRITER_H

#include <hdf5.h>
#include <vector>
#include <string>

struct IntanHeaderInfo {
    uint32_t magic;
    uint32_t streamCount;
    uint32_t channelCount;
    uint32_t sampleRate;
};

class Hdf5Writer {
public:
    Hdf5Writer();
    ~Hdf5Writer();
    
    // Open HDF5 file for writing
    bool open(const std::string& path, const IntanHeaderInfo& info);
    
    // Close the file
    void close();
    
    // Append a frame of data
    bool appendFrame(const std::vector<uint16_t>& codes, const std::vector<float>& microvolts);
    
    // Check if file is open
    bool isOpen() const { return file_ != nullptr; }
    
private:
    void* file_;  // hid_t file handle
    void* dset_codes_;  // hid_t dataset handle for codes
    void* dset_uv_;     // hid_t dataset handle for microvolts
    void* space_codes_; // hid_t dataspace handle for codes
    void* space_uv_;    // hid_t dataspace handle for microvolts
    IntanHeaderInfo info_;
    size_t frameIndex_;
};

#endif // HDF5_WRITER_H
