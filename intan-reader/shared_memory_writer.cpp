#include "shared_memory_writer.h"
#include <iostream>
#include <cstring>

SharedMemoryWriter::SharedMemoryWriter() 
    : shmFd(-1), shmBase(nullptr), shmSize(0), shmName("/intan_rhx_shm_v1"), frameCounter(0),
      header(nullptr), shmOutput(nullptr), numStreams_(0), numChannels_(0), samplesPerBlock_(128) {
}

SharedMemoryWriter::~SharedMemoryWriter() {
    cleanup();
}

bool SharedMemoryWriter::initialize(int numStreams, int numChannels, int sampleRate) {
    std::cout << "Initializing Shared Memory Writer..." << std::endl;
    
    numStreams_ = numStreams;
    numChannels_ = numChannels;
    
    if (!createSharedMemory()) {
        return false;
    }
    
    // Set up direct memory access pointers
    header = reinterpret_cast<IntanDataHeader*>(shmBase);
    shmOutput = reinterpret_cast<IntanDataBlock*>((uint8_t*)shmBase + sizeof(IntanDataHeader));
    
    // Initialize header once at startup
    initializeHeader(numStreams, numChannels, sampleRate);
    
    std::cout << "Shared memory initialized successfully (size=" << shmSize << " bytes)" << std::endl;
    return true;
}

bool SharedMemoryWriter::createSharedMemory() {
    // Remove existing shared memory if it exists
    shm_unlink(shmName);
    
    // Calculate size: header + (streams * channels * samples * sizeof(IntanDataBlock))
    size_t blocks = (size_t)numStreams_ * numChannels_ * samplesPerBlock_;
    shmSize = sizeof(IntanDataHeader) + blocks * sizeof(IntanDataBlock);
    
    std::cout << "Setting up shared memory: streams=" << numStreams_ 
              << " channels=" << numChannels_ 
              << " samples=" << samplesPerBlock_ 
              << " size=" << shmSize << " bytes" << std::endl;
    
    // Create shared memory segment
    shmFd = shm_open(shmName, O_CREAT | O_RDWR, 0666);
    if (shmFd < 0) {
        std::cerr << "Failed to create shared memory: " << strerror(errno) << std::endl;
        return false;
    }
    
    if (ftruncate(shmFd, shmSize) < 0) {
        std::cerr << "Failed to set shared memory size: " << strerror(errno) << std::endl;
        close(shmFd);
        shm_unlink(shmName);
        return false;
    }
    
    // Map shared memory
    shmBase = mmap(nullptr, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (shmBase == MAP_FAILED) {
        std::cerr << "Failed to map shared memory: " << strerror(errno) << std::endl;
        close(shmFd);
        shm_unlink(shmName);
        return false;
    }
    
    std::cout << "Shared memory created successfully: " << shmName << " (" << shmSize << " bytes)" << std::endl;
    return true;
}

void SharedMemoryWriter::writeDataBlock(uint32_t timestamp, const std::vector<std::vector<std::vector<int>>>& amplifierData) {
    std::lock_guard<std::mutex> lock(writeMutex);
    
    if (!shmOutput || amplifierData.empty() || amplifierData[0].empty()) {
        std::cerr << "SharedMemoryWriter: Invalid data or no shared memory" << std::endl;
        return;
    }
    
    // Write data blocks 
    writeDataBlocks(amplifierData);
    
    // Update timestamp
    header->timestamp = timestamp;
    
    frameCounter++;
}

void SharedMemoryWriter::initializeHeader(int numStreams, int numChannels, int sampleRate) {
    // Initialize header
    header->magic = 0x494E5441;  // "INTA"
    header->streamCount = numStreams;
    header->channelCount = numChannels;
    header->sampleRate = sampleRate;
    header->dataSize = static_cast<uint32_t>(shmSize);
    header->timestamp = 0;
}

void SharedMemoryWriter::writeDataBlocks(const std::vector<std::vector<std::vector<int>>>& amplifierData) {
    if (!shmOutput || amplifierData.empty() || amplifierData[0].empty()) {
        return;
    }
    
    size_t w = 0;
    
    for (int t = 0; t < samplesPerBlock_; ++t) {
        for (int s = 0; s < numStreams_; ++s) {
            for (int ch = 0; ch < numChannels_; ++ch) {
                int code = amplifierData[s][ch][t];
                float uV = (float)((code - 32768) * 0.195f);  // Convert to microvolts (EXACTLY like working Decoupled version)
                shmOutput[w++] = { (uint32_t)s, (uint32_t)ch, uV };
            }
        }
    }
}

void SharedMemoryWriter::cleanup() {
    if (shmBase && shmBase != MAP_FAILED) {
        munmap(shmBase, shmSize);
        shmBase = nullptr;
    }
    if (shmFd >= 0) {
        close(shmFd);
        shmFd = -1;
    }
    // Clean up shared memory segment
    shm_unlink(shmName);
    std::cout << "Shared memory writer cleaned up" << std::endl;
}