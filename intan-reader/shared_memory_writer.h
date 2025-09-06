#ifndef SHARED_MEMORY_WRITER_H
#define SHARED_MEMORY_WRITER_H

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include <mutex>
#include <chrono>

// Intan data structures for shared memory communication
struct IntanDataHeader {
    uint32_t magic;        // Magic number "INTA" (0x494E5441)
    uint32_t timestamp;    // Timestamp
    uint32_t dataSize;     // Total size
    uint32_t streamCount;  // Number of streams
    uint32_t channelCount; // Number of channels
    uint32_t sampleRate;   // Sample rate
};

struct IntanDataBlock { 
    uint32_t streamId;
    uint32_t channelId; 
    float value;
};

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
    
    int shmFd;
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