#include "../data-analyser/fpga_logger.h"
#include "asic_sender.h"
#include <cstring>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

// Static member definitions
const size_t AsicSender::BUF_LEN = 16384; // Must be multiple of 16 for USB 3.0

AsicSender::AsicSender() : device_(nullptr), running_(false), initialized_(false), data_analyzer_(nullptr) {
    device_ = new okCFrontPanel();
}

AsicSender::~AsicSender() {
    stopSending();
    if (device_) {
        delete device_;
        device_ = nullptr;
    }
}

bool AsicSender::initialize(const std::string& deviceSerial, const std::string& bitfilePath) {
    std::cout << "Initializing ASIC Sender..." << std::endl;
    
    // Open device by serial number
    int error = device_->OpenBySerial(deviceSerial.c_str());
    std::cout << "OpenBySerial ret value: " << error << std::endl;
    
    if (error != okCFrontPanel::NoError) {
        std::cerr << "Failed to open ASIC device with serial: " << deviceSerial << std::endl;
        return false;
    }
    
    // Configure FPGA with bitfile
    if (!configureFpga(bitfilePath)) {
        std::cerr << "Failed to configure ASIC FPGA" << std::endl;
        return false;
    }
    
    // Reset FIFO
    resetFifo();
    
    initialized_ = true;
    std::cout << "ASIC Sender initialized successfully" << std::endl;
    return true;
}

bool AsicSender::configureFpga(const std::string& bitfilePath) {
    std::cout << "Configuring ASIC FPGA with bitfile: " << bitfilePath << std::endl;
    
    int error = device_->ConfigureFPGA(bitfilePath.c_str());
    std::cout << "ConfigureFPGA ret value: " << error << std::endl;
    
    return (error == okCFrontPanel::NoError);
}

void AsicSender::resetFifo() {
    std::cout << "Resetting ASIC FIFO..." << std::endl;
    
    // Send reset signal to FIFO
    device_->SetWireInValue(0x10, 0xff, 0x01);
    device_->UpdateWireIns();
    
    device_->SetWireInValue(0x10, 0x00, 0x01);
    device_->UpdateWireIns();
    
    std::cout << "ASIC FIFO reset complete" << std::endl;
}

bool AsicSender::writeToFpga(const std::vector<uint8_t>& data) {
    int writeRet = device_->WriteToPipeIn(0x80, static_cast<long>(data.size()), const_cast<unsigned char*>(data.data()));
    
    // Return value is the number of bytes written, not an error code
    return (writeRet > 0);
}

bool AsicSender::readFromFpga(std::vector<uint8_t>& data) {
    data.resize(BUF_LEN);
    
    int readRet = device_->ReadFromPipeOut(0xA0, static_cast<long>(data.size()), data.data());
    
    // Return value is the number of bytes read, not an error code
    if (readRet > 0) {
        data.resize(readRet); // Resize to actual bytes read
        return true;
    }
    
    return false;
}

void AsicSender::startSending() {
    if (!initialized_) {
        std::cerr << "ASIC Sender not initialized" << std::endl;
        return;
    }
    
    std::cout << "Starting ASIC data sending..." << std::endl;
    running_ = true;
}

void AsicSender::stopSending() {
    if (running_) {
        std::cout << "Stopping ASIC data sending..." << std::endl;
        running_ = false;
    }
}

void AsicSender::setDataAnalyzer(FpgaLogger* analyzer) {
    data_analyzer_ = analyzer;
}

bool AsicSender::configurePipeline(int pipelineId) {
    if (!initialized_) {
        std::cerr << "ASIC Sender not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Configuring FPGA pipeline: " << pipelineId << std::endl;
    
    // Set pipeline configuration via WireIn
    // Address 0x01: Pipeline selection (0-9)
    device_->SetWireInValue(0x01, pipelineId, 0x0F); // Use lower 4 bits for pipeline ID
    device_->UpdateWireIns();
    
    // Trigger configuration update
    device_->ActivateTriggerIn(0x40, 0); // Trigger bit 0 for pipeline config
    device_->UpdateWireIns();
    
    std::cout << "Pipeline " << pipelineId << " configured successfully" << std::endl;
    return true;
}

bool AsicSender::enableAnalysisMode() {
    if (!initialized_) {
        std::cerr << "ASIC Sender not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Enabling FPGA analysis mode..." << std::endl;
    
    // Set analysis mode via WireIn
    // Address 0x02: Mode control (bit 0: analysis mode, bit 1: test mode)
    device_->SetWireInValue(0x02, 0x01, 0x03); // Enable analysis mode, disable test mode
    device_->UpdateWireIns();
    
    // Trigger mode change
    device_->ActivateTriggerIn(0x40, 1); // Trigger bit 1 for mode change
    device_->UpdateWireIns();
    
    std::cout << "Analysis mode enabled successfully" << std::endl;
    return true;
}

bool AsicSender::disableTestPattern() {
    if (!initialized_) {
        std::cerr << "ASIC Sender not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Disabling FPGA test pattern mode..." << std::endl;
    
    // Disable test pattern generation
    // Address 0x03: Test pattern control (bit 0: enable/disable test pattern)
    device_->SetWireInValue(0x03, 0x00, 0x01); // Disable test pattern
    device_->UpdateWireIns();
    
    // Trigger test pattern disable
    device_->ActivateTriggerIn(0x40, 2); // Trigger bit 2 for test pattern control
    device_->UpdateWireIns();
    
    std::cout << "Test pattern mode disabled successfully" << std::endl;
    return true;
}

bool AsicSender::setThresholds(double lowThreshold, double highThreshold) {
    if (!initialized_) {
        std::cerr << "ASIC Sender not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Setting FPGA thresholds - Low: " << lowThreshold << ", High: " << highThreshold << std::endl;
    
    // Convert thresholds to 16-bit values (0-65535 range)
    uint16_t lowThresh = static_cast<uint16_t>(lowThreshold * 65535);
    uint16_t highThresh = static_cast<uint16_t>(highThreshold * 65535);
    
    // Set low threshold via WireIn
    // Address 0x04-0x05: Low threshold (16-bit)
    device_->SetWireInValue(0x04, lowThresh, 0xFFFF);
    device_->UpdateWireIns();
    
    // Set high threshold via WireIn  
    // Address 0x06-0x07: High threshold (16-bit)
    device_->SetWireInValue(0x06, highThresh, 0xFFFF);
    device_->UpdateWireIns();
    
    // Trigger threshold update
    device_->ActivateTriggerIn(0x40, 3); // Trigger bit 3 for threshold update
    device_->UpdateWireIns();
    
    std::cout << "Thresholds set successfully" << std::endl;
    return true;
}

void AsicSender::sendWaveformData(const std::vector<uint8_t>& waveformData) {
    if (!running_ || !initialized_) {
        return;
    }
    
    // Ensure data length is multiple of 16 for USB 3.0
    std::vector<uint8_t> paddedData = waveformData;
    while (paddedData.size() % 16 != 0) {
        paddedData.push_back(0);
    }
    
    // Limit to BUF_LEN
    if (paddedData.size() > BUF_LEN) {
        paddedData.resize(BUF_LEN);
    }
    
    std::cout << "Sending waveform data to the FPGA..." << std::endl;
    
    // Send data to FPGA
    if (!writeToFpga(paddedData)) {
        std::cerr << "Failed to write waveform data to ASIC FPGA" << std::endl;
        return;
    }
    
    // Read processed data from FPGA
    std::vector<uint8_t> processedData;
    if (!readFromFpga(processedData)) {
        std::cerr << "Failed to read processed data from ASIC FPGA" << std::endl;
        return;
    } else {
        std::cout << "Data successfully read from ASIC FPGA!" << std::endl;
    }
    
    // Analyze FPGA response data
    if (data_analyzer_) {
        data_analyzer_->analyzeFpgaData(processedData);
    }
}
