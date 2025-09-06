//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "pipelinedatarhxcontroller.h"

PipelineDataRHXController::PipelineDataRHXController(ControllerType type_, AmplifierSampleRate sampleRate_) :
    AbstractRHXController(type_, sampleRate_),
    dataGenerator(new SynthDataBlockGenerator(type, getSampleRate(sampleRate))),
    hasTCPData(false),
    tcpThreadRunning(false)
{
    std::cout << "=== PIPELINE DATA RHX CONTROLLER INITIALIZED ===" << std::endl;
    std::cout << "This is our custom pipeline controller, not the synthetic controller!" << std::endl;
    std::cout << "Controller type: " << type << std::endl;
    std::cout << "Sample rate: " << getSampleRate(sampleRate) << " Hz" << std::endl;
    std::cout << "================================================" << std::endl;

    std::cout << "===== ATTEMPTING TO CONNECT TO SHARED MEMORY SUPPLIER ======" << std::endl;
    // Initialize TCP data storage to buffer 4 streams (32 channels each)
    tcpChannelData.resize(4);   // Default to 4 streams
    for (auto& stream : tcpChannelData) {
        stream.resize(32);      // Default to 32 channels per stream
    }
    tcpChannelFifo.resize(4);
    tcpLastValue.resize(4);
    for (int i = 0; i < 4; ++i) {
        tcpChannelFifo[i].resize(32);
        tcpLastValue[i].resize(32, 0);
    }
    
    // Shared memory (intra-host) for maximum throughput/low latency
    if (connectToSharedMemory()) {
        std::cout << "Connected to Shared Memory supplier - starting socket thread" << std::endl;
        tcpThreadRunning = true;
        tcpThread = std::thread(&PipelineDataRHXController::tcpThreadFunction, this);
    } else {
        std::cout << "Shared Memory supplier not found yet - will keep retrying in background" << std::endl;
        tcpThreadRunning = true;
        tcpThread = std::thread(&PipelineDataRHXController::tcpThreadFunction, this);
    }
    std::cout << "================================================" << std::endl;

    // Mark TCP as stale initially so we start with dummy/synthetic until data arrives
    lastTCPDataTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(100000);
    // Initialize pacing to match synthetic generator timing
    pacingStart = std::chrono::steady_clock::now();
    // Initialize pacing to match producer once we know its advertised rate; default to controller's
    dataBlockPeriodNs = 1.0e9 * ((double)RHXDataBlock::samplesPerDataBlock(type)) / getSampleRate(sampleRate);
    pacingDeficitNs = 0.0;
}

PipelineDataRHXController::~PipelineDataRHXController()
{
    std::cout << "PipelineDataRHXController destructor called" << std::endl;

    // Stop TCP thread
    tcpThreadRunning = false;
    if (tcpThread.joinable()) {
        tcpThread.join();
    }

    // Clean up shared memory
    disconnectFromSharedMemory();
    
    // Clean up data generator
    if (dataGenerator) {
        delete dataGenerator;
        dataGenerator = nullptr;
    }

    std::cout << "PipelineDataRHXController cleanup complete" << std::endl;
}

// For a physical board, read data block from the USB interface. Fill given dataBlock from USB buffer.
bool PipelineDataRHXController::readDataBlock(RHXDataBlock *dataBlock)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    unsigned int numBytesToRead = BytesPerWord * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);

    if (numBytesToRead > usbBufferSize) {
        std::cerr << "Error in PipelineDataRHXController::readDataBlock: USB buffer size exceeded.  " <<
                "Increase value of MAX_NUM_BLOCKS.\n";
        return false;
    }

    dataBlock->fillFromUsbBuffer(usbBuffer, 0);
    
    return true;
}

// For a physical board, read a certain number of USB data blocks, and append them to queue.
// Return true if data blocks were available.
bool PipelineDataRHXController::readDataBlocks(int numBlocks, std::deque<RHXDataBlock*> &dataQueue)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    unsigned int numWordsToRead = numBlocks * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);

    if (numWordsInFifo() < numWordsToRead)
        return false;

    unsigned int numBytesToRead = BytesPerWord * numWordsToRead;

    if (numBytesToRead > usbBufferSize) {
        std::cerr << "Error in PipelineDataRHXController::readDataBlocks: USB buffer size exceeded.  " <<
                "Increase value of MAX_NUM_BLOCKS.\n";
        return false;
    }
    
    for (int i = 0; i < numBlocks; ++i) {
        RHXDataBlock* dataBlock = new RHXDataBlock(type, numDataStreams);
        dataBlock->fillFromUsbBuffer(usbBuffer, i);
        dataQueue.push_back(dataBlock);
    }
    
    return true;
}

// For a physical board, read a certain number of USB data blocks, and write the raw bytes to a buffer.
// Return total number of bytes read.
long PipelineDataRHXController::readDataBlocksRaw(int numBlocks, uint8_t *buffer)
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    // Pacing: ensure we don't produce blocks faster than expected; mimic synthetic timing
    if (!isPacingReady(numBlocks)) return 0;

    if (isTCPFresh()) {
        long bytes = writeBlocksFromTCP(numBlocks, buffer);
        if (bytes > 0) return bytes;
    }
    // If TCP not fresh or composing failed, write dummy pattern
    {
        long bytes = writeBlocksDummy(numBlocks, buffer);
        if (bytes > 0) return bytes;
    }
    // Final fallback
    return dataGenerator->readSynthDataBlocksRaw(numBlocks, buffer, numDataStreams);
}

// Set the delay for sampling the MISO line on a particular SPI port (PortA - PortH), in integer clock steps, where each
// clock step is 1/2800 of a per-channel sampling period.  Note: Cable delay must be updated after sampleRate is changed,
// since cable delay calculations are based on the clock frequency!
void PipelineDataRHXController::setCableDelay(BoardPort port, int delay)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if ((delay < 0) || (delay > 15)) {
        std::cerr << "Warning in PipelineDataRHXController::setCableDelay: delay out of range: " << delay << '\n';
        if (delay < 0) delay = 0;
        else if (delay > 15) delay = 15;
    }

    switch (port) {
    case PortA:
        cableDelay[0] = delay;
        break;
    case PortB:
        cableDelay[1] = delay;
        break;
    case PortC:
        cableDelay[2] = delay;
        break;
    case PortD:
        cableDelay[3] = delay;
        break;
    case PortE:
        cableDelay[4] = delay;
        break;
    case PortF:
        cableDelay[5] = delay;
        break;
    case PortG:
        cableDelay[6] = delay;
        break;
    case PortH:
        cableDelay[7] = delay;
        break;
    default:
        std::cerr << "Error in PipelineDataRHXController::setCableDelay: unknown port.\n";
    }
}

// Assign a particular data source (e.g., PortA1, PortA2, PortB1,...) to one of the eight available USB data streams (0-7).
// Used only with ControllerRecordUSB2.
void PipelineDataRHXController::setDataSource(int stream, BoardDataSource dataSource)
{
    if (type != ControllerRecordUSB2) return;

    if ((stream < 0) || (stream > 7)) {
        std::cerr << "Error in PipelineDataRHXController::setDataSource: stream out of range.\n";
        return;
    }
    boardDataSources[stream] = dataSource;
}

// Set the per-channel sampling rate of the RHD/RHS chips connected to the FPGA.
bool PipelineDataRHXController::setSampleRate(AmplifierSampleRate newSampleRate)
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    sampleRate = newSampleRate;
    return true;
}

// Enable or disable one of the 32 available USB data streams (0-31).
void PipelineDataRHXController::enableDataStream(int stream, bool enabled)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if (stream < 0 || stream > (maxNumDataStreams() - 1)) {
        std::cerr << "Error in PipelineDataRHXController::enableDataStream: stream out of range.\n";
        return;
    }

    if (enabled) {
        if (dataStreamEnabled[stream] == 0) {
            dataStreamEnabled[stream] = 1;
            numDataStreams++;
        }
    } else {
        if (dataStreamEnabled[stream] == 1) {
            dataStreamEnabled[stream] = 0;
            numDataStreams--;
        }
    }
}

// Return 4-bit "board mode" input.
int PipelineDataRHXController::getBoardMode()
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    return boardMode(type);
}

// Return number of SPI ports and if I/O expander board is present.
int PipelineDataRHXController::getNumSPIPorts(bool &expanderBoardDetected)
{
    expanderBoardDetected = true;
    return (type == ControllerRecordUSB3 ? 8 : 4);
}

// Scan all SPI ports to find all connected RHD/RHS amplifier chips.  Read the chip ID from on-chip ROM
// register to determine the number of amplifier channels on each port.  This process is repeated at all
// possible MISO delays in the FPGA to determine the optimum MISO delay for each port to compensate for
// variable cable length.
//
// This function returns three vectors of length maxNumDataStreams(): chipType (the type of chip connected
// to each data stream); portIndex (the SPI port number [A=1, B=2,...] associated with each data stream);
// and commandStream (the stream index for sending commands to the FPGA for a particular read stream index).
// This function also returns a vector of length maxNumSPIPorts(): numChannelsOnPort (the total number of
// amplifier channels on each port).
//
// This function normally returns 1, but returns a negative number if a ControllerRecordUSB2 devices is used
// and its 256-channel capacity (limited by USB2 bus speed) is exceeded.  A value of -1 is returned, or a value
// of -2 if RHD2216 devices are present so that the user can be reminded that RHD2216 devices consume 32 channels
// of USB bus bandwidth.
int PipelineDataRHXController::findConnectedChips(std::vector<ChipType> &chipType, std::vector<int> &portIndex, std::vector<int> &commandStream,
                                               std::vector<int> &numChannelsOnPort, bool synthMaxChannels, bool /* returnToFastSettle */,
                                               bool /* usePreviousDelay */, int /* selectedPort */, int /* lastDetectedChip */, int /* lastDetectedNumStreams */)
{
    int maxNumStreams = AbstractRHXController::maxNumDataStreams(type);
    int maxSPIPorts = AbstractRHXController::maxNumSPIPorts(type);

    chipType.resize(maxNumStreams);
    fill(chipType.begin(), chipType.end(), NoChip);

    portIndex.resize(maxNumStreams);
    fill(portIndex.begin(), portIndex.end(), -1);

    commandStream.resize(maxNumStreams);
    fill(commandStream.begin(), commandStream.end(), -1);

    numChannelsOnPort.resize(maxSPIPorts);
    fill (numChannelsOnPort.begin(), numChannelsOnPort.end(), 0);

    setCableDelay(PortA, 2);
    setCableDelay(PortB, 1);
    setCableDelay(PortC, 1);
    setCableDelay(PortD, 1);
    if (type == ControllerRecordUSB3) {
        setCableDelay(PortE, 1);
        setCableDelay(PortF, 1);
        setCableDelay(PortG, 1);
        setCableDelay(PortH, 1);
    }

    // When highestCapacity is false, the default synthetic data of 32 channels on Ports A and B will be set up.
    // When highestCapacity is true, the maximum # of channels per port will be set up.
    //bool highestCapacity = false;
    //bool highestCapacity = true;

    if (synthMaxChannels) {
        if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
            // For USB Interface Board, 128 channels on A and 128 channels on B
            // For Recording Controller, 128 channels on each port A-H
            int numPorts = type == ControllerRecordUSB2 ? 2 : 8;
            for (int thisPort = 0; thisPort < numPorts; ++thisPort) {
                int offset = thisPort * 4;
                for (int thisStream = 0; thisStream < 4; ++thisStream) {
                    // Even streams, RHD2164. Odd streams, RHD2164MISOBChip
                    int streamIndex = offset + thisStream;
                    chipType[streamIndex] = (thisStream % 2 == 0) ? RHD2164Chip : RHD2164MISOBChip; // Even - RHD2164. Odd - RHD2164MISOBChip
                    enableDataStream(streamIndex, true);
                    portIndex[streamIndex] = thisPort;
                    commandStream[streamIndex] = streamIndex;
                }
                if (type == ControllerRecordUSB2) {
                    setDataSource(0, PortA1);
                    setDataSource(1, PortA1Ddr);
                    setDataSource(2, PortA2);
                    setDataSource(3, PortA2Ddr);
                    setDataSource(4, PortB1);
                    setDataSource(5, PortB1Ddr);
                    setDataSource(6, PortB2);
                    setDataSource(7, PortB2Ddr);
                }
                numChannelsOnPort[thisPort] = 128;
            }
        }

        else if (type == ControllerStimRecord) {
            // For Stim/Recording Controller, 32 channels on each port A-D
            for (int thisPort = 0; thisPort < 4; ++thisPort) {
                int offset = thisPort * 2;
                for (int thisStream = 0; thisStream < 2; ++thisStream) {
                    int streamIndex = offset + thisStream;
                    chipType[streamIndex] = RHS2116Chip;
                    enableDataStream(streamIndex, true);
                    portIndex[streamIndex] = thisPort;
                    commandStream[streamIndex] = streamIndex;
                }
                numChannelsOnPort[thisPort] = 32;
            }
        }
    }

    else {
        if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
            chipType[0] = RHD2132Chip;
            enableDataStream(0, true);
            if (type == ControllerRecordUSB2) {
                setDataSource(0, PortA1);
            }
            portIndex[0] = 0;  // Port A
            commandStream[0] = 0;
            numChannelsOnPort[0] = 32;

            chipType[1] = RHD2132Chip;
            enableDataStream(1, true);
            if (type == ControllerRecordUSB2) {
                setDataSource(1, PortB1);
            }
            portIndex[1] = 1;  // Port B
            commandStream[1] = 1;
            numChannelsOnPort[1] = 32;

            for (int stream = 2; stream < maxNumStreams; stream++) enableDataStream(stream, false);
        }

        else if (type == ControllerStimRecord) {
            chipType[0] = RHS2116Chip;
            chipType[1] = RHS2116Chip;
            enableDataStream(0, true);
            enableDataStream(1, true);
            portIndex[0] = 0;  // Port A
            portIndex[1] = 0;  // Port A
            commandStream[0] = 0;
            commandStream[1] = 1;
            numChannelsOnPort[0] = 32;

            chipType[2] = RHS2116Chip;
            chipType[3] = RHS2116Chip;
            enableDataStream(2, true);
            enableDataStream(3, true);
            portIndex[2] = 1;  // Port B
            portIndex[3] = 1;  // Port B
            commandStream[2] = 2;
            commandStream[3] = 3;
            numChannelsOnPort[1] = 32;

            for (int stream = 4; stream < maxNumStreams; stream++) enableDataStream(stream, false);
        }
    }

    return 1;
}

// Return the number of 16-bit words in the USB FIFO.  The user should never attempt to read more data than the
// FIFO currently contains, as it is not protected against underflow.
unsigned int PipelineDataRHXController::numWordsInFifo()
{
    numWordsHasBeenUpdated = true;
    return MaxNumBlocksToRead * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);
}


bool PipelineDataRHXController::connectToSharedMemory()
{
    if (shmConnected) return true;
    // Layout: [Header | IntanDataBlock repeated ...], producer updates header->dataSize and posts sem
    shmFd = shm_open(shmName, O_RDWR, 0666);
    if (shmFd < 0) return false;
    struct stat st;
    if (fstat(shmFd, &st) == 0) {
        shmSize = (size_t)st.st_size;
    } else {
        shmSize = 0;
    }
    if (shmSize == 0) {
        close(shmFd);
        shmFd = -1;
        return false;
    }
    shmBase = mmap(nullptr, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (shmBase == MAP_FAILED) {
        close(shmFd);
        shmFd = -1;
        return false;
    }
    shmConnected = true;
    return true;
}

void PipelineDataRHXController::disconnectFromSharedMemory()
{
    if (shmBase && shmBase != MAP_FAILED) {
        munmap(shmBase, shmSize);
        shmBase = nullptr;
        shmSize = 0;
    }
    if (shmFd >= 0) {
        close(shmFd);
        shmFd = -1;
    }
    shmConnected = false;
}

void PipelineDataRHXController::tcpThreadFunction()
{
    std::cout << "TCP thread started" << std::endl;
    
    // If shared memory is connected, poll header for new frames (timestamp change)
    std::vector<uint8_t> frameBuf;
    frameBuf.reserve(1 << 20);
    while (tcpThreadRunning) {
        // Retry SHM connection if not connected yet
        if (!shmConnected) {
            if (connectToSharedMemory()) {
                std::cout << "Connected to Shared Memory supplier" << std::endl;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }
        }

        if (shmBase && shmSize >= sizeof(IntanDataHeader)) {
            const char* base = reinterpret_cast<const char*>(shmBase);
            const IntanDataHeader* hdr = reinterpret_cast<const IntanDataHeader*>(base);
            if (hdr->dataSize >= sizeof(IntanDataHeader) && hdr->dataSize <= shmSize) {
                if (hdr->timestamp != lastShmTimestamp) {
                    lastShmTimestamp = hdr->timestamp;
                    // Update pacing to producer's advertised sample rate so our consumer rate matches
                    if (hdr->sampleRate > 0) {
                        producerSampleRateHz = static_cast<double>(hdr->sampleRate);
                        dataBlockPeriodNs = 1.0e9 * ((double)RHXDataBlock::samplesPerDataBlock(type)) / producerSampleRateHz;
                    }
                    if (convertTCPDataToRHXBlock(base, hdr->dataSize)) {
                        hasTCPData = true;
                        lastTCPDataTime = std::chrono::steady_clock::now();
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    
    std::cout << "TCP thread stopped" << std::endl;
}

bool PipelineDataRHXController::convertTCPDataToRHXBlock(const char* tcpData, size_t dataSize)
{
    if (dataSize < sizeof(IntanDataHeader)) {
        return false;
    }
    
    // Parse the header
    const IntanDataHeader* header = reinterpret_cast<const IntanDataHeader*>(tcpData);

    // Check magic number
    if (header->magic != 0x494E5441) { // "INTA"
        std::cout << "Invalid magic number in TCP data" << std::endl;
        return false;
    }
    
    std::cout << "Processing TCP data: streams=" << header->streamCount 
              << " channels=" << header->channelCount 
              << " sampleRate=" << header->sampleRate << std::endl;

    // Update TCP data storage size if needed
    if (header->streamCount > 0 && header->channelCount > 0) {
        std::lock_guard<std::mutex> lock(tcpDataMutex);
        tcpChannelData.resize(header->streamCount);
        for (size_t i = 0; i < header->streamCount; ++i) {
            tcpChannelData[i].resize(header->channelCount, 0);
        }
        tcpChannelFifo.resize(header->streamCount);
        tcpLastValue.resize(header->streamCount);
        for (size_t i = 0; i < header->streamCount; ++i) {
            tcpChannelFifo[i].resize(header->channelCount);
            tcpLastValue[i].resize(header->channelCount, 0);
        }
    }

    // Parse data blocks and store the values (sample-major layout)
    size_t offset = sizeof(IntanDataHeader);
    const uint64_t blocksAvailable = (dataSize - sizeof(IntanDataHeader)) / sizeof(IntanDataBlock);
    const uint64_t channelsPerFrame = static_cast<uint64_t>(header->streamCount) * header->channelCount;
    if (channelsPerFrame == 0) return false;
    const uint64_t samplesPerFrame = blocksAvailable / channelsPerFrame;
    for (uint64_t sample = 0; sample < samplesPerFrame && offset + sizeof(IntanDataBlock) <= dataSize; ++sample) {
        for (uint32_t stream = 0; stream < header->streamCount && offset + sizeof(IntanDataBlock) <= dataSize; ++stream) {
            for (uint32_t channel = 0; channel < header->channelCount && offset + sizeof(IntanDataBlock) <= dataSize; ++channel) {
                const IntanDataBlock* block = reinterpret_cast<const IntanDataBlock*>(tcpData + offset);
                offset += sizeof(IntanDataBlock);
                if (stream < tcpChannelData.size() && channel < tcpChannelData[stream].size()) {
                    // Convert float value to Intan format (microvolts to 16-bit)
                    double electrodeValue = static_cast<double>(block->value);
                    int result = round(electrodeValue / 0.195) + 32768;
                    if (result < 0) result = 0;
                    else if (result > 65535) result = 65535;
                    
                    std::lock_guard<std::mutex> lock(tcpDataMutex);
                    uint16_t code = static_cast<uint16_t>(result);
                    tcpChannelData[stream][channel] = code; // keep last
                    tcpLastValue[stream][channel] = code;
                    auto& q = tcpChannelFifo[stream][channel];
                    if (q.size() >= tcpFifoMaxDepth) q.pop_front();
                    q.push_back(code);
                }
            }
        }
    }

    // Update freshness on successful parse
    lastTCPDataTime = std::chrono::steady_clock::now();
    return true;
}

void PipelineDataRHXController::injectTCPDataIntoGenerator()
{
    if (hasTCPData && dataGenerator) {
        std::lock_guard<std::mutex> lock(tcpDataMutex);
        std::cout << "TCP data available: " << tcpChannelData.size() << " streams, "
                  << (tcpChannelData.empty() ? 0 : tcpChannelData[0].size()) << " channels" << std::endl;
    }
}

void PipelineDataRHXController::processTCPData()
{
    // This is called from the TCP thread
    injectTCPDataIntoGenerator();
} 

bool PipelineDataRHXController::isTCPFresh()
{
    auto now = std::chrono::steady_clock::now();
    auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTCPDataTime).count();
    return hasTCPData && (ageMs >= 0) && (ageMs < tcpFreshTimeoutMs);
}

bool PipelineDataRHXController::isPacingReady(int numBlocks)
{
    auto now = std::chrono::steady_clock::now();
    double elapsedNs = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(now - pacingStart).count();
    // Pace to the producer's sample rate if known to keep receive and render frequencies aligned
    double targetNs = (double)numBlocks * dataBlockPeriodNs;
    double excessNs = elapsedNs - (targetNs - pacingDeficitNs);

    if (excessNs < 0.0) return false; // not ready yet

    pacingStart = std::chrono::steady_clock::now();
    pacingDeficitNs = excessNs;
    if (pacingDeficitNs > targetNs) pacingDeficitNs = targetNs; // clamp
    return true;
}

long PipelineDataRHXController::writeBlocksFromTCP(int numBlocks, uint8_t* buffer)
{
    if (type == ControllerStimRecord) return 0; // not supported in this simple path
    if (!hasTCPData) return 0;

    uint8_t* pWrite = buffer;
    int streams = numDataStreams;
    int channels = RHXDataBlock::channelsPerStream(type);

    for (int block = 0; block < numBlocks; ++block) {
        for (int sample = 0; sample < RHXDataBlock::samplesPerDataBlock(type); ++sample) {
            // Header magic number (64-bit split across four 16-bit words)
            uint64_t header = RHXDataBlock::headerMagicNumber(type);
            writeWordLE(pWrite, (uint16_t)((header & 0x000000000000ffffULL) >> 0));
            writeWordLE(pWrite, (uint16_t)((header & 0x00000000ffff0000ULL) >> 16));
            writeWordLE(pWrite, (uint16_t)((header & 0x0000ffff00000000ULL) >> 32));
            writeWordLE(pWrite, (uint16_t)((header & 0xffff000000000000ULL) >> 48));

            // Timestamp (two 16-bit words)
            writeWordLE(pWrite, (uint16_t)((tIndex & 0x0000ffffU) >> 0));
            writeWordLE(pWrite, (uint16_t)((tIndex & 0xffff0000U) >> 16));

            // Aux results 0-2: zeros for simplicity
            for (int ch = 0; ch < RHXDataBlock::numAuxChannels(type); ++ch) {
                for (int s = 0; s < streams; ++s) writeWordLE(pWrite, 0U);
            }

            // Amplifier data (channel-major, then stream): consume from per-channel FIFO
            for (int ch = 0; ch < channels; ++ch) {
                for (int s = 0; s < streams; ++s) {
                    uint16_t code = 0U;
                    {
                        std::lock_guard<std::mutex> lock(tcpDataMutex);
                        if (s < (int)tcpChannelFifo.size() && ch < (int)tcpChannelFifo[s].size()) {
                            auto& q = tcpChannelFifo[s][ch];
                            if (!q.empty()) {
                                code = q.front();
                                q.pop_front();
                            } else if (s < (int)tcpLastValue.size() && ch < (int)tcpLastValue[s].size()) {
                                code = tcpLastValue[s][ch];
                            }
                        }
                    }
                    writeWordLE(pWrite, code);
                }
            }

            // Filler words
            int filler = (type == ControllerRecordUSB2) ? streams : (streams % 4);
            for (int i = 0; i < filler; ++i) writeWordLE(pWrite, 0U);

            // ADCs (8)
            for (int i = 0; i < 8; ++i) writeWordLE(pWrite, 0U);
            // TTL in/out
            writeWordLE(pWrite, 0U);
            writeWordLE(pWrite, 0U);

            ++tIndex;
        }
    }

    long numWords = numBlocks * RHXDataBlock::dataBlockSizeInWords(type, streams);
    return BytesPerWord * numWords;
}

long PipelineDataRHXController::writeBlocksDummy(int numBlocks, uint8_t* buffer)
{
    if (type == ControllerStimRecord) return 0; // not supported in this simple path
    if (numDataStreams <= 0) return 0;

    uint8_t* pWrite = buffer;
    int streams = numDataStreams;
    int channels = RHXDataBlock::channelsPerStream(type);

    for (int block = 0; block < numBlocks; ++block) {
        for (int sample = 0; sample < RHXDataBlock::samplesPerDataBlock(type); ++sample) {
            uint64_t header = RHXDataBlock::headerMagicNumber(type);
            writeWordLE(pWrite, (uint16_t)((header & 0x000000000000ffffULL) >> 0));
            writeWordLE(pWrite, (uint16_t)((header & 0x00000000ffff0000ULL) >> 16));
            writeWordLE(pWrite, (uint16_t)((header & 0x0000ffff00000000ULL) >> 32));
            writeWordLE(pWrite, (uint16_t)((header & 0xffff000000000000ULL) >> 48));

            writeWordLE(pWrite, (uint16_t)((tIndex & 0x0000ffffU) >> 0));
            writeWordLE(pWrite, (uint16_t)((tIndex & 0xffff0000U) >> 16));

            // Aux zeros
            for (int ch = 0; ch < RHXDataBlock::numAuxChannels(type); ++ch) {
                for (int s = 0; s < streams; ++s) writeWordLE(pWrite, 0U);
            }

            // Alternate 0 and 0xFFFF per channel for clear visualization
            for (int ch = 0; ch < channels; ++ch) {
                uint16_t val = (ch % 2 == 0) ? 0x0000U : 0xFFFFU;
                for (int s = 0; s < streams; ++s) writeWordLE(pWrite, val);
            }

            int filler = (type == ControllerRecordUSB2) ? streams : (streams % 4);
            for (int i = 0; i < filler; ++i) writeWordLE(pWrite, 0U);

            for (int i = 0; i < 8; ++i) writeWordLE(pWrite, 0U);
            writeWordLE(pWrite, 0U);
            writeWordLE(pWrite, 0U);

            ++tIndex;
        }
    }

    long numWords = numBlocks * RHXDataBlock::dataBlockSizeInWords(type, streams);
    return BytesPerWord * numWords;
}
