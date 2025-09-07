#include "fpga_logger.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <sstream>

FpgaLogger::FpgaLogger() {
    openLogFile();
}

FpgaLogger::~FpgaLogger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void FpgaLogger::openLogFile() {
    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // Create date-specific directory (same as data_logger)
    std::string current_date = get_date_string();
    std::string date_dir = "data-analyser/logs/" + current_date;
    std::filesystem::create_directories(date_dir);
    
    std::ostringstream filename;
    filename << date_dir << "/fpga_data_" 
             << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S")
             << "-" << std::setfill('0') << std::setw(3) << ms.count() << ".txt";
    
    logFilePath_ = filename.str();
    logFile_.open(logFilePath_, std::ios::out);
}

void FpgaLogger::writeToLog(const std::string& message) {
    if (logFile_.is_open()) {
        logFile_ << message << std::endl;
        logFile_.flush();
    }
}

void FpgaLogger::analyzeFpgaData(const std::vector<uint8_t>& fpgaData) {
    if (fpgaData.empty()) {
        return;
    }
    
    // Decode the HALO response
    HaloResponse response = decoder_.decodeResponse(fpgaData);
    
    // Write both raw data and decoded response
    writeDecodedResponse(response);
}

void FpgaLogger::setHaloPipeline(HaloPipeline pipeline) {
    decoder_.setPipeline(pipeline);
}

void FpgaLogger::setThresholds(double lowThreshold, double highThreshold) {
    decoder_.setThresholds(lowThreshold, highThreshold);
}

void FpgaLogger::writeDecodedResponse(const HaloResponse& response) {
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // Create timestamp string
    std::ostringstream timestamp;
    timestamp << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    // Create clean, readable log entry
    std::ostringstream log_entry;
    
    // Format based on response type
    switch (response.type) {
        case HaloResponseType::SEIZURE_DETECTED:
            log_entry << "[" << timestamp.str() << "] SEIZURE DETECTED "
                     << "(" << std::fixed << std::setprecision(1) 
                     << (response.confidence * 100) << "%)";
            break;
            
        case HaloResponseType::THRESHOLD_EXCEEDED:
            log_entry << "[" << timestamp.str() << "] Elevated Activity "
                     << "(" << std::fixed << std::setprecision(1) 
                     << (response.confidence * 100) << "%)";
            break;
            
        case HaloResponseType::NORMAL_ACTIVITY:
            log_entry << "[" << timestamp.str() << "] Normal Activity "
                     << "(" << std::fixed << std::setprecision(1) 
                     << (response.confidence * 100) << "%)";
            break;
            
        case HaloResponseType::TEST_PATTERN:
            log_entry << "[" << timestamp.str() << "] Test Pattern "
                     << "Data: " << static_cast<int>(response.raw_data);
            break;
            
        case HaloResponseType::PROCESSING_ERROR:
            log_entry << "[" << timestamp.str() << "] Processing Error "
                     << response.description;
            break;
            
        default:
            log_entry << "[" << timestamp.str() << "] Unknown Response "
                     << "Data: " << static_cast<int>(response.raw_data);
            break;
    }
    
    // Add pipeline info for non-test patterns
    if (response.type != HaloResponseType::TEST_PATTERN) {
        log_entry << " P" << static_cast<int>(response.pipeline);
    }
    
    // Add key metrics for seizure/threshold cases
    if ((response.type == HaloResponseType::SEIZURE_DETECTED || 
         response.type == HaloResponseType::THRESHOLD_EXCEEDED)) {
        log_entry << " A:" << std::fixed << std::setprecision(2) 
                 << response.activity_level;
    }
    
    log_entry << "\n";
    
    writeToLog(log_entry.str());
}

std::string FpgaLogger::get_date_string() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
    
    return ss.str();
}
