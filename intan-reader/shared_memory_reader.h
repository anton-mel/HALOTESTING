#ifndef SHARED_MEMORY_READER_H
#define SHARED_MEMORY_READER_H

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
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
    
    int shmFd;
    void* shmBase;
    size_t shmSize;
    const char* shmName;
    
    // Direct memory access pointers
    IntanDataHeader* header;
    IntanDataBlock* shmInput;
    uint32_t lastTimestamp;
};

#endif // SHARED_MEMORY_READER_H
