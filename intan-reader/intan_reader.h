#ifndef INTAN_READER_H
#define INTAN_READER_H

#include <iostream>
#include <atomic>
#include <memory>
#include <vector>

#include "includes/rhd2000evalboardusb3.h"
#include "includes/rhd2000datablockusb3.h"
#include "includes/rhd2000registersusb3.h"
#include "shared_memory_writer.h"

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
