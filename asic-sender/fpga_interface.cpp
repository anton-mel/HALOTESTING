#include "fpga_interface.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <cstring>

// Static member definitions
const size_t FpgaInterface::BUF_LEN = 16384; // Must be multiple of 16 for USB 3.0
const int FpgaInterface::TIMEOUT_SECONDS = 1;

FpgaInterface::FpgaInterface() : device_(nullptr), initialized_(false) {
    device_ = new okCFrontPanel();
}

FpgaInterface::~FpgaInterface() {
    cleanup();
    if (device_) {
        delete device_;
        device_ = nullptr;
    }
}

bool FpgaInterface::initialize(const std::string& deviceSerial, const std::string& bitfilePath) {
    std::cout << "Initializing FPGA Interface..." << std::endl;
    
    // Open device by serial number
    int error = device_->OpenBySerial(deviceSerial.c_str());
    std::cout << "OpenBySerial ret value: " << error << std::endl;
    
    if (error != okCFrontPanel::NoError) {
        std::cerr << "Failed to open device with serial: " << deviceSerial << std::endl;
        return false;
    }
    
    // Configure FPGA with bitfile
    if (!configureFpga(bitfilePath)) {
        std::cerr << "Failed to configure FPGA" << std::endl;
        return false;
    }
    
    // Reset FIFO
    resetFifo();
    
    initialized_ = true;
    std::cout << "FPGA Interface initialized successfully" << std::endl;
    return true;
}

bool FpgaInterface::configureFpga(const std::string& bitfilePath) {
    std::cout << "Configuring FPGA with bitfile: " << bitfilePath << std::endl;
    
    int error = device_->ConfigureFPGA(bitfilePath.c_str());
    
    return (error == okCFrontPanel::NoError);
}

void FpgaInterface::resetFifo() {
    // Send reset signal to FIFO
    device_->SetWireInValue(0x10, 0xff, 0x01);
    device_->UpdateWireIns();
    
    device_->SetWireInValue(0x10, 0x00, 0x01);
    device_->UpdateWireIns();
}

bool FpgaInterface::readFromStdin(std::vector<uint8_t>& data) {
    data.resize(BUF_LEN);
    
    // Set stdin to non-blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    // Use select to check if data is available
    fd_set readfds;
    struct timeval timeout;
    
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    
    int result = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
    
    if (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        ssize_t bytesRead = read(STDIN_FILENO, data.data(), BUF_LEN);
        if (bytesRead > 0) {
            data.resize(bytesRead);
            return true;
        }
    }
    
    return false;
}

bool FpgaInterface::writeToFpga(const std::vector<uint8_t>& data) {
    int writeRet = device_->WriteToPipeIn(0x80, data.size(), data.data());
    
    return (writeRet == okCFrontPanel::NoError);
}

bool FpgaInterface::readFromFpga(std::vector<uint8_t>& data) {
    data.resize(BUF_LEN);
    
    int readRet = device_->ReadFromPipeOut(0xA0, data.size(), data.data());
    
    if (readRet == okCFrontPanel::NoError) {
        data.resize(BUF_LEN); // Assume full buffer read
        return true;
    }
    
    return false;
}

void FpgaInterface::runDataTransfer() {
    if (!initialized_) {
        std::cerr << "FPGA Interface not initialized" << std::endl;
        return;
    }
    
    std::cout << "Starting data transfer loop..." << std::endl;
    
    std::vector<uint8_t> dataIn, dataOut;
    
    while (true) {
        // Wait for input data with timeout
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(TIMEOUT_SECONDS);
        bool dataPresent = false;
        
        while (std::chrono::steady_clock::now() < deadline && !dataPresent) {
            dataPresent = readFromStdin(dataIn);
            if (!dataPresent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        if (!dataPresent) {
            std::cout << "CPP INTERFACE: input data timeout" << std::endl;
            break;
        }
        
        // Print received data
        printDataArray(dataIn, "Received input data");
        
        // Send data to FPGA
        if (!writeToFpga(dataIn)) {
            std::cerr << "Failed to write to FPGA" << std::endl;
            continue;
        }
        
        // Read processed data from FPGA
        if (!readFromFpga(dataOut)) {
            std::cerr << "Failed to read from FPGA" << std::endl;
            continue;
        }
        
        // Print output data
        printDataArray(dataOut, "dataout");
    }
}

void FpgaInterface::cleanup() {
    if (initialized_) {
        initialized_ = false;
    }
}
