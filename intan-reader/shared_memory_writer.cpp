#include "shared_memory_writer.h"
#include <iostream>
#include <cstring>

SharedMemoryWriter::SharedMemoryWriter() 
#ifdef _WIN32
    : shmHandle(nullptr), shmBase(nullptr), shmSize(0), shmName("intan_rhx_shm_v1"), frameCounter(0),
#else
    : shmFd(-1), shmBase(nullptr), shmSize(0), shmName("/intan_rhx_shm_v1"), frameCounter(0),
#endif
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
    // Calculate size: header + (streams * channels * samples * sizeof(IntanDataBlock))
    size_t blocks = (size_t)numStreams_ * numChannels_ * samplesPerBlock_;
    shmSize = sizeof(IntanDataHeader) + blocks * sizeof(IntanDataBlock);
    
    std::cout << "Setting up shared memory: streams=" << numStreams_ 
              << " channels=" << numChannels_ 
              << " samples=" << samplesPerBlock_ 
              << " size=" << shmSize << " bytes" << std::endl;
#ifdef _WIN32
    // Create or open a named file mapping object backed by the system paging file
    shmHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(shmSize), "intan_rhx_shm_v1");
    if (!shmHandle) {
        std::cerr << "Failed to create shared memory mapping" << std::endl;
        return false;
    }
    shmBase = MapViewOfFile(shmHandle, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0);
    if (!shmBase) {
        std::cerr << "Failed to map view of shared memory" << std::endl;
        CloseHandle(shmHandle);
        shmHandle = nullptr;
        return false;
    }
    std::cout << "Shared memory created successfully (Windows)" << std::endl;
    return true;
#else
    // Remove existing shared memory if it exists
    shm_unlink(shmName);
    
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
#endif
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
    
    // Debug output every 50 frames
    // if (frameCounter % 50 == 0) {
    //     std::cout << "SHM Published frame " << frameCounter << " ts=" << timestamp << std::endl;
    // }
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
#ifdef _WIN32
    if (shmBase) {
        UnmapViewOfFile(shmBase);
        shmBase = nullptr;
    }
    if (shmHandle) {
        CloseHandle(shmHandle);
        shmHandle = nullptr;
    }
#else
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
#endif
    std::cout << "Shared memory writer cleaned up" << std::endl;
}