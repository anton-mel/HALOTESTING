#ifndef INTAN_DATA_TYPES_H
#define INTAN_DATA_TYPES_H

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

#endif // INTAN_DATA_TYPES_H
