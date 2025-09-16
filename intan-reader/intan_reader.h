#ifndef INTAN_READER_H
#define INTAN_READER_H

#include <iostream>
#include <atomic>
#include <memory>
#include <vector>

// Include shared memory (which brings in Windows headers on _WIN32) BEFORE vendor headers
// to avoid 'using namespace std' in vendor headers causing 'byte' ambiguity with Windows RPC.
#include "shared_memory_writer.h"
#include "includes/rhd2000evalboardusb3.h"
#include "includes/rhd2000datablockusb3.h"
#include "includes/rhd2000registersusb3.h"

class IntanReader {
public:
    IntanReader();
    ~IntanReader();
    
    // Initialize the reader with default settings
    bool initialize();
    
    // Start continuous data acquisition
    bool start();
    
    // Stop data acquisition
    void stop();
    
    // Check if reader is running
    bool isRunning() const { return running_; }
    

private:
    std::unique_ptr<Rhd2000EvalBoardUsb3> controller_;
    std::atomic<bool> running_;
    std::unique_ptr<SharedMemoryWriter> sharedMemoryWriter_;
    
    // Internal methods
    bool openDevice();
    bool uploadBitfile();
    bool configureDevice();
    void readDataLoop();
};

#endif // INTAN_READER_H
