#ifndef SHARED_MEMORY_READER_H
#define SHARED_MEMORY_READER_H

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

class SharedMemoryReader {
public:
    SharedMemoryReader();
    ~SharedMemoryReader();
    
    bool initialize();
    bool readLatestData(std::vector<uint8_t>& waveformData);
    void cleanup();

private:
    bool openSharedMemory();
    
#ifdef _WIN32
    HANDLE shmHandle;
#else
    int shmFd;
    size_t shmSize;
#endif
    void* shmBase;
    const char* shmName;
    
    // Direct memory access pointers
    IntanDataHeader* header;
    IntanDataBlock* shmInput;
    uint32_t lastTimestamp;
};

#endif // SHARED_MEMORY_READER_H
