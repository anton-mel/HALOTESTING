#include "../data-analyser/fpga_raw_logger.h"
#include "asic_sender.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <cstring>

// Static member definitions
const size_t AsicSender::BUF_LEN = 16384; // Must be multiple of 16 for USB 3.0

AsicSender::AsicSender() : device_(nullptr), running_(false), initialized_(false), data_analyzer_(nullptr) {
    device_ = new OpalKellyLegacy::okCFrontPanel();
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
    
    if (error != OpalKellyLegacy::okCFrontPanel::NoError) {
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
    
    return (error == OpalKellyLegacy::okCFrontPanel::NoError);
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
    int writeRet = device_->WriteToPipeIn(0x80, data.size(), data.data());
    
    // Return value is the number of bytes written, not an error code
    return (writeRet > 0);
}

bool AsicSender::readFromFpga(std::vector<uint8_t>& data) {
    data.resize(BUF_LEN);
    
    int readRet = device_->ReadFromPipeOut(0xA0, data.size(), data.data());
    
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

void AsicSender::setDataAnalyzer(FpgaRawLogger* analyzer) {
    data_analyzer_ = analyzer;
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
