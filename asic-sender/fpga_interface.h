#ifndef FPGA_INTERFACE_H
#define FPGA_INTERFACE_H

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include "okFrontPanel.h"

class FpgaInterface {
public:
    FpgaInterface();
    ~FpgaInterface();
    
    // Initialize FPGA connection
    bool initialize(const std::string& deviceSerial, const std::string& bitfilePath);
    
    // Main data transfer loop
    void runDataTransfer();
    
    // Cleanup
    void cleanup();

private:
    okCFrontPanel* device_;
    bool initialized_;
    static const size_t BUF_LEN; // Must be multiple of 16 for USB 3.0
    static const int TIMEOUT_SECONDS;
    
    // Helper functions
    bool configureFpga(const std::string& bitfilePath);
    void resetFifo();
    bool readFromStdin(std::vector<uint8_t>& data);
    bool writeToFpga(const std::vector<uint8_t>& data);
    bool readFromFpga(std::vector<uint8_t>& data);
    void printDataArray(const std::vector<uint8_t>& data, const std::string& label);
};

#endif // FPGA_INTERFACE_H
