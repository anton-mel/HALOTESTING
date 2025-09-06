#include "fpga_raw_logger.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <sstream>

FpgaRawLogger::FpgaRawLogger() {
    openLogFile();
}

FpgaRawLogger::~FpgaRawLogger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void FpgaRawLogger::openLogFile() {
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
    filename << date_dir << "/fpga_raw_data_" 
             << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S")
             << "-" << std::setfill('0') << std::setw(3) << ms.count() << ".txt";
    
    logFilePath_ = filename.str();
    logFile_.open(logFilePath_, std::ios::out);
}

void FpgaRawLogger::writeToLog(const std::string& message) {
    if (logFile_.is_open()) {
        logFile_ << message << std::endl;
        logFile_.flush();
    }
}

void FpgaRawLogger::analyzeFpgaData(const std::vector<uint8_t>& fpgaData) {
    if (fpgaData.empty()) {
        return;
    }
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // Create timestamp string
    std::ostringstream timestamp;
    timestamp << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    // Log timestamp : raw data as comma-separated values
    std::ostringstream row_output;
    row_output << timestamp.str() << " ---- ";
    for (size_t i = 0; i < fpgaData.size(); ++i) {
        row_output << static_cast<int>(fpgaData[i]);
        if (i < fpgaData.size() - 1) {
            row_output << ",";
        }
    }
    
    writeToLog(row_output.str() + "\n\n");
}

std::string FpgaRawLogger::get_date_string() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
    
    return ss.str();
}
