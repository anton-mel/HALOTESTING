#include "fpga_interface.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    
    // Default values (can be overridden by command line arguments)
    std::string deviceSerial = "2416001B97";  // Default from Python code
    std::string bitfilePath = "First.bit";   // Use the bitfile in interface folder
    
    // Parse command line arguments
    if (argc >= 2) {
        deviceSerial = argv[1];
    }
    if (argc >= 3) {
        bitfilePath = argv[2];
    }
    
    try {
        // Create FPGA interface
        FpgaInterface fpgaInterface;
        
        // Initialize FPGA connection
        if (!fpgaInterface.initialize(deviceSerial, bitfilePath)) {
            std::cerr << "Failed to initialize FPGA interface" << std::endl;
            return -1;
        }
        
        // Run data transfer loop
        fpgaInterface.runDataTransfer();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
