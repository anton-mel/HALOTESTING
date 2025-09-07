#include "shared_memory_reader.h"
#include <iostream>
#include <cstring>

SharedMemoryReader::SharedMemoryReader() 
    : shmFd(-1), shmBase(nullptr), shmSize(0), shmName("/intan_rhx_shm_v1"), 
      header(nullptr), shmInput(nullptr), lastTimestamp(0) {
}

SharedMemoryReader::~SharedMemoryReader() {
    cleanup();
}

bool SharedMemoryReader::initialize() {
    return openSharedMemory();
}

bool SharedMemoryReader::openSharedMemory() {
    // Open existing shared memory
    shmFd = shm_open(shmName, O_RDONLY, 0666);
    if (shmFd == -1) {
        std::cerr << "Failed to open shared memory: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Get shared memory size
    struct stat shmStat;
    if (fstat(shmFd, &shmStat) == -1) {
        std::cerr << "Failed to get shared memory size: " << strerror(errno) << std::endl;
        close(shmFd);
        return false;
    }
    shmSize = shmStat.st_size;
    
    // Map shared memory
    shmBase = mmap(nullptr, shmSize, PROT_READ, MAP_SHARED, shmFd, 0);
    if (shmBase == MAP_FAILED) {
        std::cerr << "Failed to map shared memory: " << strerror(errno) << std::endl;
        close(shmFd);
        return false;
    }
    
    // Set up pointers
    header = static_cast<IntanDataHeader*>(shmBase);
    shmInput = reinterpret_cast<IntanDataBlock*>(static_cast<char*>(shmBase) + sizeof(IntanDataHeader));
    
    std::cout << "Shared memory reader initialized successfully (size=" << shmSize << " bytes)" << std::endl;
    return true;
}

bool SharedMemoryReader::readLatestData(std::vector<uint8_t>& waveformData) {
    if (!shmBase || !header || !shmInput) {
        return false;
    }
    
    // Check if we have new data
    if (header->timestamp == lastTimestamp) {
        return false; // No new data
    }
    
    lastTimestamp = header->timestamp;
    
    // Calculate number of data blocks
    size_t headerSize = sizeof(IntanDataHeader);
    size_t dataSize = header->dataSize;
    size_t numBlocks = dataSize / sizeof(IntanDataBlock);
    
    // Convert neural data to waveform format
    waveformData.clear();
    waveformData.reserve(numBlocks * sizeof(float));
    
    for (size_t i = 0; i < numBlocks; ++i) {
        IntanDataBlock& block = shmInput[i];
        
        // Convert float to uint8_t (scale from neural range to 0-255)
        // Neural data is typically in microvolts, scale to reasonable range
        float scaledValue = (block.value + 1000.0f) / 8.0f; // Scale to 0-255 range
        scaledValue = std::max(0.0f, std::min(255.0f, scaledValue));
        
        uint8_t byteValue = static_cast<uint8_t>(scaledValue);
        waveformData.push_back(byteValue);
    }
    
    return true;
}

void SharedMemoryReader::cleanup() {
    if (shmBase && shmBase != MAP_FAILED) {
        munmap(shmBase, shmSize);
        shmBase = nullptr;
    }
    
    if (shmFd != -1) {
        close(shmFd);
        shmFd = -1;
    }
    
    header = nullptr;
    shmInput = nullptr;
}
