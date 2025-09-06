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

#ifndef PIPELINEDATARHXCONTROLLER_H
#define PIPELINEDATARHXCONTROLLER_H

#include "synthdatablockgenerator.h"
#include "abstractrhxcontroller.h"
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
#include <deque>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

// Intan data structures for TCP communication
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

class PipelineDataRHXController : public AbstractRHXController
{
public:
    PipelineDataRHXController(ControllerType type_, AmplifierSampleRate sampleRate_);
    ~PipelineDataRHXController();

    // Pure virtual functions from AbstractRHXController
    bool isSynthetic() const override { return true; }
    bool isPlayback() const override { return false; }
    AcquisitionMode acquisitionMode() const override { return SyntheticMode; }
    int open(const std::string& /* boardSerialNumber */) override { return 1; }
    bool uploadFPGABitfile(const std::string& /* filename */) override { return true; }
    void resetBoard() override {}

    void run() override {}
    bool isRunning() override { return false; }
    void flush() override { dataGenerator->reset(); }
    void resetFpga() override {}

    bool readDataBlock(RHXDataBlock *dataBlock) override;
    bool readDataBlocks(int numBlocks, std::deque<RHXDataBlock*> &dataQueue) override;
    long readDataBlocksRaw(int numBlocks, uint8_t *buffer) override;

    void setContinuousRunMode(bool) override {}
    void setMaxTimeStep(unsigned int) override {}
    void setCableDelay(BoardPort port, int delay) override;
    void setDspSettle(bool) override {}
    void setDataSource(int stream, BoardDataSource dataSource) override;
    void setTtlOut(const int*) override {}
    void setDacManual(int) override {}
    void setLedDisplay(const int*) override {}
    void setSpiLedDisplay(const int*) override {}
    void setDacGain(int) override {}
    void setAudioNoiseSuppress(int) override {}
    void setExternalFastSettleChannel(int) override {}
    void setExternalDigOutChannel(BoardPort, int) override {}
    void setDacHighpassFilter(double) override {}
    void setDacThreshold(int, int, bool) override {}
    void setTtlMode(int) override {}
    void setDacRerefSource(int, int) override {}
    void setExtraStates(unsigned int) override {}
    void setStimCmdMode(bool) override {}
    void setAnalogInTriggerThreshold(double) override {}
    void setManualStimTrigger(int, bool) override {}
    void setGlobalSettlePolicy(bool, bool, bool, bool, bool) override {}
    void setTtlOutMode(bool, bool, bool, bool, bool, bool, bool, bool) override {}
    void setAmpSettleMode(bool) override {}
    void setChargeRecoveryMode(bool) override {}
    bool setSampleRate(AmplifierSampleRate newSampleRate) override;

    void enableDataStream(int stream, bool enabled) override;
    void enableDac(int, bool) override {}
    void enableExternalFastSettle(bool) override {}
    void enableExternalDigOut(BoardPort, bool) override {}
    void enableDacHighpassFilter(bool) override {}
    void enableDacReref(bool) override {}
    void enableDcAmpConvert(bool) override {}
    void enableAuxCommandsOnAllStreams() override {}
    void enableAuxCommandsOnOneStream(int) override {}

    void selectDacDataStream(int, int) override {}
    void selectDacDataChannel(int, int) override {}
    void selectAuxCommandLength(AuxCmdSlot, int, int) override {}
    void selectAuxCommandBank(BoardPort, AuxCmdSlot, int) override {}

    int getBoardMode() override;
    int getNumSPIPorts(bool& expanderBoardDetected) override;

    void clearTtlOut() override {}
    void resetSequencers() override {}
    void programStimReg(int, int, StimRegister, int) override {}
    void uploadCommandList(const std::vector<unsigned int>&, AuxCmdSlot, int) override {}

    int findConnectedChips(std::vector<ChipType> &chipType, std::vector<int> &portIndex, std::vector<int> &commandStream,
                           std::vector<int> &numChannelsOnPort, bool synthMaxChannels = false, bool returnToFastSettle = false,
                           bool usePreviousDelay = false, int selectedPort = 0, int lastDetectedChip = -1, int lastDetectedNumStreams = -1) override;

    // Shared memory fast path - all public
    bool connectToSharedMemory();
    void disconnectFromSharedMemory();
    void processTCPData();
    bool convertTCPDataToRHXBlock(const char* tcpData, size_t dataSize);
    void injectTCPDataIntoGenerator();
    void tcpThreadFunction();

private:
    unsigned int numWordsInFifo() override;
    bool isDcmProgDone() const override { return true; }
    bool isDataClockLocked() const override { return true; }
    void forceAllDataStreamsOff() override {}

    // Basic data generator (will be replaced with TCP data)
    SynthDataBlockGenerator* dataGenerator;
    
    // Shared memory fast path
    bool shmConnected = false;
    int shmFd = -1;
    void* shmBase = nullptr;
    size_t shmSize = 0;
    const char* shmName = "/intan_rhx_shm_v1";
    uint32_t lastShmTimestamp = 0;
    bool hasTCPData;
    std::thread tcpThread;
    bool tcpThreadRunning;
    
    // TCP data storage
    std::vector<std::vector<uint16_t>> tcpChannelData; // last value per channel (legacy)
    std::vector<std::vector<std::deque<uint16_t>>> tcpChannelFifo; // ring buffers with per-sample values
    std::vector<std::vector<uint16_t>> tcpLastValue; // fallback when fifo underflows
    std::mutex tcpDataMutex;
    size_t tcpFifoMaxDepth = 32768; // cap to avoid unbounded growth

    // Dynamic switching between TCP and dummy data
    std::chrono::steady_clock::time_point lastTCPDataTime;
    int tcpFreshTimeoutMs = 500; // consider TCP fresh if data within this window
    uint32_t tIndex = 0; // timestamp counter for synthetic/dummy/TCP-built blocks
    std::chrono::steady_clock::time_point pacingStart;
    double pacingDeficitNs = 0.0;
    double dataBlockPeriodNs = 0.0; // computed as samplesPerBlock / producerSampleRateHz
    double producerSampleRateHz = 0.0; // latest sample rate reported by producer (SHM header)

    // Helpers
    bool isTCPFresh();
    bool isPacingReady(int numBlocks);
    long writeBlocksFromTCP(int numBlocks, uint8_t* buffer);
    long writeBlocksDummy(int numBlocks, uint8_t* buffer);
    inline void writeWordLE(uint8_t*& pWrite, uint16_t word)
    {
        *pWrite = (uint8_t)(word & 0x00ffU);
        ++pWrite;
        *pWrite = (uint8_t)((word & 0xff00U) >> 8);
        ++pWrite;
    }
};

#endif // PIPELINEDATARHXCONTROLLER_H
