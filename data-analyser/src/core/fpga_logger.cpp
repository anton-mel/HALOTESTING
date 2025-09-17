#include "fpga_logger.h"
#include "hdf5_writer.h"
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <iomanip>
#include <chrono>
#include <sstream>

FpgaLogger::FpgaLogger() : responseCount_(0) {
    // Create logs directory for today
    std::string current_date = getDateString();
    std::string date_dir = "data-analyser/logs/" + current_date;
    std::filesystem::create_directories(date_dir);
}

FpgaLogger::~FpgaLogger() {
    // HDF5 writers will be automatically closed when unique_ptr goes out of scope
}

void FpgaLogger::analyzeFpgaData(const std::vector<uint8_t>& fpgaData, const std::vector<uint8_t>& originalData) {
    if (fpgaData.empty()) {
        return;
    }

    // Decode the HALO response
    HaloResponse response = decoder_.decodeResponse(fpgaData);
    responseCount_++;
    
    // Log FPGA response to HDF5 (all responses, not just seizures)
    logFpgaResponseToHdf5(response, fpgaData, originalData);
}

void FpgaLogger::setHaloPipeline(HaloPipeline pipeline) {
    decoder_.setPipeline(pipeline);
}

void FpgaLogger::setThresholds(double lowThreshold, double highThreshold) {
    decoder_.setThresholds(lowThreshold, highThreshold);
}


void FpgaLogger::logFpgaResponseToHdf5(const HaloResponse& response, const std::vector<uint8_t>& processedData, const std::vector<uint8_t>& originalData) {
    // Get current hour
    int currentHour = getCurrentHour();
    
    // Create hourly writer if it doesn't exist
    createHourlyWriter(currentHour);
    
    // Get the writer for this hour
    auto it = hourlyWriters_.find(currentHour);
    if (it == hourlyWriters_.end()) {
        std::cerr << "[ERROR] No HDF5 writer available for hour " << currentHour << std::endl;
        return;
    }
    
    Hdf5Writer* writer = it->second.get();
    
    // Prepare data for all 32 channels + 4 metadata channels = 36 total
    std::vector<uint16_t> codes(36, 0);
    std::vector<float> microvolts(36, 0.0f);
    
    // Store original neural data from shared memory (not processed FPGA response)
    // Store original neural data from all channels (0-31)
    size_t dataSize = std::min(originalData.size(), static_cast<size_t>(32));
    for (size_t i = 0; i < dataSize; ++i) {
        // Convert back from scaled uint8_t to microvolts
        // Original scaling: (value + 1000.0f) / 8.0f = uint8_t
        // Reverse: (uint8_t * 8.0f) - 1000.0f = original microvolts
        float originalMicrovolts = (static_cast<float>(originalData[i]) * 8.0f) - 1000.0f;
        
        codes[i] = static_cast<uint16_t>(originalData[i]) << 8; // Convert to 16-bit
        microvolts[i] = originalMicrovolts; // Store original microvolts
    }
    
    // Store seizure detection metadata in channels 32-35 (after neural data)
    // Channel 32: Raw data from FPGA response
    codes[32] = static_cast<uint16_t>(response.raw_data);
    microvolts[32] = static_cast<float>(response.secondary_metric);
    
    // Channel 33: Seizure type and timestamp fraction
    codes[33] = static_cast<uint16_t>(static_cast<int>(response.type));
    microvolts[33] = static_cast<float>(response.timestamp.time_since_epoch().count() % 1000000) / 1000000.0f;
    
    // Channel 34: Seizure detection confidence (0-1 range)
    codes[34] = static_cast<uint16_t>(response.confidence * 65535);
    microvolts[34] = response.confidence;
    
    // Channel 35: Activity level
    codes[35] = static_cast<uint16_t>(response.activity_level * 1000);
    microvolts[35] = response.activity_level;
    
    // Append to HDF5 file
    writer->appendFrame(codes, microvolts);
}

void FpgaLogger::createHourlyWriter(int hour) {
    // Check if writer already exists for this hour
    if (hourlyWriters_.find(hour) != hourlyWriters_.end()) {
        return; // Already exists
    }
    
    // Create HDF5 file for this hour
    std::string current_date = getDateString();
    std::string date_dir = "data-analyser/logs/" + current_date;
    std::filesystem::create_directories(date_dir);
    
    std::ostringstream filename;
    filename << date_dir << "/hour_" << std::setfill('0') << std::setw(2) << hour << ".h5";
    
    // Create HDF5 writer for this hour
    auto writer = std::make_unique<Hdf5Writer>();
    
    // Set up header info for FPGA response data
    IntanHeaderInfo info;
    info.magic = 0x464741; // "FGA" magic number
    info.streamCount = 1;
    info.channelCount = 36; // 32 neural channels + 4 metadata channels
    info.sampleRate = 1000; // 1 kHz
    
    if (writer->open(filename.str(), info)) {
        hourlyWriters_[hour] = std::move(writer);
        // HDF5 file creation logging removed for long-term stability
    } else {
        std::cerr << "[ERROR] Failed to create HDF5 file for hour " << hour << std::endl;
    }
}

std::string FpgaLogger::getDateString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
    return ss.str();
}

std::string FpgaLogger::getHourString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H");
    return ss.str();
}

int FpgaLogger::getCurrentHour() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    return tm.tm_hour;
}
