#ifndef ASIC_SENDER_H
#define ASIC_SENDER_H

#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include "../intan-reader/includes/okFrontPanelDLL.h"

// Forward declaration
class FpgaLogger;

class AsicSender {
public:
    AsicSender();
    ~AsicSender();
    
    // Initialize ASIC FPGA connection
    bool initialize(const std::string& deviceSerial, const std::string& bitfilePath);
    
    // Start sending waveform data to FPGA
    void startSending();
    
    // Stop sending data
    void stopSending();
    
    // Check if running
    bool isRunning() const { return running_; }
    
    // Send waveform data (called from main pipeline)
    void sendWaveformData(const std::vector<uint8_t>& waveformData);
    
    // Set FPGA data analyzer for response analysis
    void setDataAnalyzer(FpgaLogger* analyzer);
    
    // FPGA Configuration methods
    bool configurePipeline(int pipelineId);
    bool enableAnalysisMode();
    bool disableTestPattern();
    bool setThresholds(double lowThreshold, double highThreshold);

private:
    okCFrontPanel* device_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    static const size_t BUF_LEN; // Must be multiple of 16 for USB 3.0
    FpgaLogger* data_analyzer_;
    
    // Helper functions
    bool configureFpga(const std::string& bitfilePath);
    void resetFifo();
    bool writeToFpga(const std::vector<uint8_t>& data);
    bool readFromFpga(std::vector<uint8_t>& data);
    void printDataArray(const std::vector<uint8_t>& data, const std::string& label);
};

#endif // ASIC_SENDER_H
