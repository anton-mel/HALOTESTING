// This is a wrapper for the Intan RHX device FPGA reader.
// GOAL: decouple the data acquisition from the visualization SDK.
// It handles the main logic for the neural data acquisition pipeline
// using the Opal Kelly drivers in the intan_reader.cpp file.
// It links with shared_memory_writer.cpp to write the data to the SDK.

#include "main.h"

int main(int argc, char* argv[]) {
    

    try {
        // Create RHX controller for USB3 recording controller
        RHXController controller(ControllerRecordUSB3, SampleRate30000Hz, false);
        
        // List available devices
        std::vector<std::string> devices = controller.listAvailableDeviceSerials();
        if (devices.empty()) {
            std::cerr << "No Intan devices found. Please connect a device and try again." << std::endl;
            return -1;
        }
        
        std::cout << "Found " << devices.size() << " device(s):" << std::endl;
        for (size_t i = 0; i < devices.size(); ++i) {
            std::cout << "  " << i << ": " << devices[i] << std::endl;
        }
        
        // Open the first available device
        std::string deviceSerial = devices[0];
        std::cout << "Connecting to device: " << deviceSerial << std::endl;
        
        int result = controller.open(deviceSerial);
        if (result != 1) {
            std::cerr << "Failed to open device. Error code: " << result << std::endl;
            return -1;
        }
        
        // Upload FPGA bitfile
        std::string bitfilePath = "FPGA-bitfiles/ConfigRHDController.bit";
        std::cout << "Uploading FPGA bitfile: " << bitfilePath << std::endl;
        
        if (!controller.uploadFPGABitfile(bitfilePath)) {
            std::cerr << "Failed to upload FPGA bitfile." << std::endl;
            return -1;
        }
        
        // Initialize the controller
        controller.initialize();
        
        // Find connected chips
        std::vector<ChipType> chipTypes;
        std::vector<int> portIndices;
        std::vector<int> commandStreams;
        std::vector<int> numChannelsOnPort;
        
        std::cout << "Scanning for connected chips..." << std::endl;
        int numChips = controller.findConnectedChips(chipTypes, portIndices, commandStreams, numChannelsOnPort, false, false, false, 0, -1, -1);
        
        if (numChips == 0) {
            std::cout << "No chips found. Continuing with default configuration..." << std::endl;
        } else {
            std::cout << "Found " << numChips << " chip(s):" << std::endl;
            for (int i = 0; i < numChips; ++i) {
                std::cout << "  Chip " << i << ": Type=" << chipTypes[i] 
                         << ", Port=" << portIndices[i] 
                         << ", Stream=" << commandStreams[i] 
                         << ", Channels=" << numChannelsOnPort[i] << std::endl;
            }
        }
        
        // Enable data streams
        controller.enableDataStream(0, true);
        
        // Set up command list for register configuration
        std::vector<unsigned int> commandList;
        RHXRegisters registers(controller.getType(), controller.getSampleRate());
        registers.createCommandListRHDRegisterConfig(commandList, true, 60);
        controller.uploadCommandList(commandList, AbstractRHXController::AuxCmd1, 0);
        
        // Start data acquisition
        std::cout << "Starting data acquisition..." << std::endl;
        controller.run();
        
        // Create data block for reading
        RHXDataBlock dataBlock(controller.getType(), controller.getNumEnabledDataStreams());
        ControllerType controllerType = controller.getType();
        
        // Main data reading loop
        std::cout << "Reading data (Press Ctrl+C to stop)..." << std::endl;
        
        while (true) {
            if (controller.readDataBlock(&dataBlock)) {
                blockCount++;
                
                // Print data every 10 blocks to avoid flooding console
                if (blockCount % 10 == 0) {
                    std::cout << "\nBlock #" << blockCount << std::endl;
                    printDataBlock(&dataBlock, controllerType, 0);
                }
                
                // Check for pipe read errors
                if (controller.pipeReadError() != 0) {
                    std::cerr << "Pipe read error: " << controller.pipeReadError() << std::endl;
                    break;
                }
            } else {
                // No data available, sleep briefly
                usleep(1000); // 1ms
            }
        }
        
        std::cout << "\nStopping data acquisition..." << std::endl;
        controller.flush();
        
        std::cout << "Total blocks read: " << blockCount << std::endl;
        std::cout << "Device reader stopped successfully." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
