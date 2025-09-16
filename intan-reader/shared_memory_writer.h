#ifndef SHARED_MEMORY_WRITER_H
#define SHARED_MEMORY_WRITER_H

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <vector>
#include <mutex>
#include <chrono>

#include "intan_data_types.h"

class SharedMemoryWriter {
public:
    SharedMemoryWriter();
    ~SharedMemoryWriter();
    
    bool initialize(int numStreams, int numChannels, int sampleRate);
    void writeDataBlock(uint32_t timestamp, const std::vector<std::vector<std::vector<int>>>& amplifierData);
    void cleanup();

private:
    bool createSharedMemory();
    void initializeHeader(int numStreams, int numChannels, int sampleRate);
    void writeDataBlocks(const std::vector<std::vector<std::vector<int>>>& amplifierData);
    
#ifdef _WIN32
    HANDLE shmHandle;
#else
    int shmFd;
#endif
    void* shmBase;
    size_t shmSize;
    const char* shmName;
    std::mutex writeMutex;
    uint32_t frameCounter;
    
    // Direct memory access pointers
    IntanDataHeader* header;
    IntanDataBlock* shmOutput;
    int numStreams_;
    int numChannels_;
    int samplesPerBlock_;
};

#endif // SHARED_MEMORY_WRITER_H