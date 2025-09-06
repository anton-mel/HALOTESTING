// We partition the pipeline into two parts:
// - reader: XEM7310 (Opal Kelly) for Intan RHX device
// - interface: XEM6310 (Opal Kelly) for seizure detection SDK
// This is essential since both drivers cannot run on the same 
// virtual environment. Their object files have symbol linking 
// conflicts.
// - log: data logger interface;
// we sample the detections given seizure outcomes.
// - Intan Visualizer: we modify the intan visualizer to 
// allow (advanced window) pipelined mode to handle the data
// supplied from external sources like this pipeline. Thus,
// one can enable/disable the visualizer based on the needs.

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include "intan-reader/intan_reader.h"
#include "asic-sender/asic_sender.h"
#include "data-analyser/fpga_raw_logger.h"

int main(int /* argc */, char* /* argv */[]) {
    std::cout << "Testing Pipeline - Main Entry Point" << std::endl;
    std::cout << "Starting Intan RHX Device Reader..." << std::endl;
    
    try {
        // Create and initialize the reader
        IntanReader reader;
        if (!reader.initialize()) {
            std::cerr << "Failed to initialize Intan Reader." << std::endl;
            return -1;
        }
        
        // DataLogger removed - only FPGA raw data logging needed
        
        // Create and initialize the ASIC sender (optional)
        AsicSender asicSender;
        bool asicInitialized = asicSender.initialize("2437001CWG", "asic-sender/First.bit");
        if (!asicInitialized) {
            std::cerr << "Warning: ASIC Sender not available, continuing without FPGA processing." << std::endl;
        }
        
        // Create and initialize the FPGA raw logger only if ASIC is available
        std::unique_ptr<FpgaRawLogger> fpgaRawLogger;
        if (asicInitialized) {
            fpgaRawLogger = std::make_unique<FpgaRawLogger>();
            // Connect the FPGA raw logger to the ASIC sender
            asicSender.setDataAnalyzer(fpgaRawLogger.get());
        }
        
        // Start data acquisition
        if (!reader.start()) {
            std::cerr << "Failed to start data acquisition." << std::endl;
            return -1;
        }
        
        // Start ASIC sender only if initialized
        std::thread asicThread;
        if (asicInitialized) {
            asicSender.startSending();
            
            // Create ASIC sender thread
            asicThread = std::thread([&asicSender]() {
                // Simulate sending waveform data periodically
                while (asicSender.isRunning()) {
                    // Generate dummy waveform data (replace with real data from reader)
                    std::vector<uint8_t> waveformData(16384, 0xAA); // 16KB of test data
                    for (size_t i = 0; i < waveformData.size(); ++i) {
                        waveformData[i] = 0xAA + (i % 256); // Fill with test pattern
                    }
                    
                    asicSender.sendWaveformData(waveformData);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Send every second
                }
            });
        }
        
        // Main loop - wait for reader to finish
        while (reader.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Stop ASIC sender and wait for thread
        if (asicInitialized) {
            asicSender.stopSending();
        }
        if (asicThread.joinable()) {
            asicThread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return -1;
    }
    
    return 0;
}
