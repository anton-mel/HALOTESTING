#include "hdf5_reader.h"
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>

Hdf5Reader::Hdf5Reader() : file_(nullptr), dset_codes_(nullptr), dset_uv_(nullptr) {}

Hdf5Reader::~Hdf5Reader() { close(); }

bool Hdf5Reader::open(const std::string& path) {
    close();
    
    if (!std::filesystem::exists(path)) {
        std::cerr << "[ERROR] HDF5 file does not exist: " << path << std::endl;
        return false;
    }
    
    hid_t file = H5Fopen(path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0) {
        std::cerr << "[ERROR] Failed to open HDF5 file: " << path << std::endl;
        return false;
    }
    
    // Open datasets
    hid_t dsetCodes = H5Dopen2(file, "/samples_codes", H5P_DEFAULT);
    if (dsetCodes < 0) {
        std::cerr << "[ERROR] Failed to open samples_codes dataset" << std::endl;
        H5Fclose(file);
        return false;
    }
    
    hid_t dsetUv = H5Dopen2(file, "/samples_uV", H5P_DEFAULT);
    if (dsetUv < 0) {
        std::cerr << "[ERROR] Failed to open samples_uV dataset" << std::endl;
        H5Dclose(dsetCodes);
        H5Fclose(file);
        return false;
    }
    
    file_ = reinterpret_cast<void*>(static_cast<intptr_t>(file));
    dset_codes_ = reinterpret_cast<void*>(static_cast<intptr_t>(dsetCodes));
    dset_uv_ = reinterpret_cast<void*>(static_cast<intptr_t>(dsetUv));
    
    return true;
}

void Hdf5Reader::close() {
    if (dset_codes_) { 
        H5Dclose(static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_codes_))); 
        dset_codes_ = nullptr; 
    }
    if (dset_uv_) { 
        H5Dclose(static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_uv_))); 
        dset_uv_ = nullptr; 
    }
    if (file_) { 
        H5Fclose(static_cast<hid_t>(reinterpret_cast<intptr_t>(file_))); 
        file_ = nullptr; 
    }
}

bool Hdf5Reader::readHeader(IntanHeaderInfo& info) {
    if (!file_ || !dset_codes_) return false;
    
    hid_t dsetC = static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_codes_));
    
    // Read attributes
    auto read_attr_u32 = [&](const char* name, uint32_t& value) -> bool {
        hid_t attr = H5Aopen(dsetC, name, H5P_DEFAULT);
        if (attr < 0) return false;
        
        hid_t atype = H5Aget_type(attr);
        herr_t status = H5Aread(attr, atype, &value);
        
        H5Tclose(atype);
        H5Aclose(attr);
        return status >= 0;
    };
    
    bool success = true;
    success &= read_attr_u32("streamCount", info.streamCount);
    success &= read_attr_u32("channelCount", info.channelCount);
    success &= read_attr_u32("sampleRate", info.sampleRate);
    
    // Set magic number based on file type
    info.magic = 0x464741; // "FGA" magic number
    
    return success;
}

std::vector<SeizureDetectionData> Hdf5Reader::readSeizureDetections() {
    std::vector<SeizureDetectionData> detections;
    
    if (!file_ || !dset_codes_ || !dset_uv_) return detections;
    
    hid_t dsetC = static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_codes_));
    hid_t dsetU = static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_uv_));
    
    // Get dataset dimensions
    hid_t spaceC = H5Dget_space(dsetC);
    int ndims = H5Sget_simple_extent_ndims(spaceC);
    if (ndims != 2) {
        std::cerr << "[ERROR] Expected 2D dataset, got " << ndims << "D" << std::endl;
        H5Sclose(spaceC);
        return detections;
    }
    
    hsize_t dims[2];
    H5Sget_simple_extent_dims(spaceC, dims, nullptr);
    H5Sclose(spaceC);
    
    hsize_t numFrames = dims[0];
    hsize_t numSignals = dims[1];
    
    if (numFrames == 0) return detections;
    
    // Read all data at once
    std::vector<uint16_t> codes(numFrames * numSignals);
    std::vector<float> microvolts(numFrames * numSignals);
    
    herr_t status1 = H5Dread(dsetC, H5T_NATIVE_UINT16, H5S_ALL, H5S_ALL, H5P_DEFAULT, codes.data());
    herr_t status2 = H5Dread(dsetU, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, microvolts.data());
    
    if (status1 < 0 || status2 < 0) {
        std::cerr << "[ERROR] Failed to read HDF5 data" << std::endl;
        return detections;
    }
    
    // Process each frame
    for (hsize_t frame = 0; frame < numFrames; ++frame) {
        SeizureDetectionData detection;
        
        // Extract data from this frame
        size_t offset = frame * numSignals;
        
        if (numSignals >= 36) {
            // New format: 32 neural channels (0-31) + 4 metadata channels (32-35)
            // Channel 32: Raw data from FPGA response
            // Channel 33: Seizure type and timestamp fraction
            // Channel 34: Seizure detection confidence
            // Channel 35: Activity level
            
            detection.rawData = static_cast<uint8_t>(codes[offset + 32] & 0xFF);
            detection.confidence = microvolts[offset + 34];
            detection.activityLevel = microvolts[offset + 35];
            detection.secondaryMetric = microvolts[offset + 32];
            
            // Find the channel with highest activity (channels 0-31)
            detection.channelIndex = 0;
            double maxActivity = 0.0;
            for (int ch = 0; ch < 32; ++ch) {
                double channelActivity = std::abs(microvolts[offset + ch]);
                if (channelActivity > maxActivity) {
                    maxActivity = channelActivity;
                    detection.channelIndex = ch;
                }
            }
            
            // Extract seizure type from channel 33
            int seizureTypeInt = static_cast<int>(codes[offset + 33]);
            if (seizureTypeInt >= 0 && seizureTypeInt <= 5) {
                detection.responseType = responseTypeToString(detection.confidence, detection.activityLevel);
            }
        } else if (numSignals >= 32) {
            // Legacy format: metadata in channels 28-31
            detection.rawData = static_cast<uint8_t>(codes[offset + 28] & 0xFF);
            detection.confidence = microvolts[offset + 30];
            detection.activityLevel = microvolts[offset + 31];
            detection.secondaryMetric = microvolts[offset + 28];
            
            // Find the channel with highest activity (channels 0-27)
            detection.channelIndex = 0;
            double maxActivity = 0.0;
            for (int ch = 0; ch < 28; ++ch) {
                double channelActivity = std::abs(microvolts[offset + ch]);
                if (channelActivity > maxActivity) {
                    maxActivity = channelActivity;
                    detection.channelIndex = ch;
                }
            }
            
            // Extract seizure type from channel 29
            int seizureTypeInt = static_cast<int>(codes[offset + 29]);
            if (seizureTypeInt >= 0 && seizureTypeInt <= 5) {
                detection.responseType = responseTypeToString(detection.confidence, detection.activityLevel);
            }
        } else if (numSignals >= 3) {
            // Legacy format for backward compatibility
            detection.rawData = static_cast<uint8_t>(codes[offset] & 0xFF);
            detection.confidence = microvolts[offset];
            detection.activityLevel = microvolts[offset + 1];
            detection.secondaryMetric = microvolts[offset + 2];
            detection.channelIndex = 0; // Default to channel 0 for legacy format
        } else {
            // Fallback for files with fewer signals
            detection.rawData = static_cast<uint8_t>(codes[offset] & 0xFF);
            detection.confidence = microvolts[offset];
            detection.activityLevel = detection.confidence;
            detection.secondaryMetric = 0.0;
            detection.channelIndex = 0; // Default to channel 0 for fallback
        }
        
        // Generate timestamp based on file creation time and frame index
        detection.timestamp = extractTimestampFromFrame(frame);
        
        // Determine response type based on confidence and activity level
        detection.responseType = responseTypeToString(detection.confidence, detection.activityLevel);
        
        // Generate description
        std::ostringstream desc;
        desc << "Confidence: " << std::fixed << std::setprecision(3) << detection.confidence
             << ", Activity: " << detection.activityLevel;
        detection.description = desc.str();
        
        detections.push_back(detection);
    }
    
    return detections;
}

std::chrono::system_clock::time_point Hdf5Reader::extractTimestampFromFrame(int frameIndex) const {
    // For now, we'll generate timestamps based on the file modification time
    // In a real implementation, you might store actual timestamps in the HDF5 file
    
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::milliseconds(frameIndex * 1000); // Assume 1 second per frame
    return now - duration;
}

std::string Hdf5Reader::responseTypeToString(double confidence, double activityLevel) const {
    // Use similar thresholds as in halo_response_decoder
    const double highThreshold = 0.7;
    const double lowThreshold = 0.3;
    
    if (confidence > highThreshold || activityLevel > highThreshold) {
        return "SEIZURE_DETECTED";
    } else if (confidence > lowThreshold || activityLevel > lowThreshold) {
        return "THRESHOLD_EXCEEDED";
    } else {
        return "NORMAL_ACTIVITY";
    }
}

std::vector<float> Hdf5Reader::readChannelData(int channelIndex) {
    std::vector<float> channelData;
    
    if (!file_ || !dset_codes_ || !dset_uv_ || channelIndex < 0 || channelIndex >= 32) {
        return channelData;
    }
    
    hid_t dsetC = static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_codes_));
    hid_t dsetU = static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_uv_));
    
    // Get dataset dimensions
    hid_t spaceC = H5Dget_space(dsetC);
    hsize_t dims[2];
    H5Sget_simple_extent_dims(spaceC, dims, nullptr);
    hsize_t numFrames = dims[0];
    hsize_t numSignals = dims[1];
    H5Sclose(spaceC);
    
    if (numSignals < static_cast<hsize_t>(channelIndex + 1)) {
        std::cerr << "[ERROR] Channel " << channelIndex << " not available (only " << numSignals << " channels)" << std::endl;
        return channelData;
    }
    
    // Read all data
    std::vector<float> microvolts(numFrames * numSignals);
    herr_t status = H5Dread(dsetU, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, microvolts.data());
    
    if (status < 0) {
        std::cerr << "[ERROR] Failed to read channel data from HDF5" << std::endl;
        return channelData;
    }
    
    // Extract data for the specific channel
    channelData.reserve(numFrames);
    for (hsize_t frame = 0; frame < numFrames; ++frame) {
        size_t offset = frame * numSignals;
        channelData.push_back(microvolts[offset + channelIndex]);
    }
    
    return channelData;
}
