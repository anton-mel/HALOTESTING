#include "intan_reader.h"
#include <iostream>
#include <thread>
#include <chrono>

IntanReader::IntanReader() 
    : running_(false) {
}

IntanReader::~IntanReader() {
    try {
        stop();
    } catch (...) {
    }
}

bool IntanReader::initialize() {
    std::cout << "Initializing Intan Reader..." << std::endl;
    controller_ = std::make_unique<Rhd2000EvalBoardUsb3>();
    
    // Initialize shared memory writer with device parameters
    sharedMemoryWriter_ = std::make_unique<SharedMemoryWriter>();
    int numStreams = 1; // Will be set after device is opened
    int numChannels = CHANNELS_PER_STREAM; // 32 channels per stream
    int sampleRate = 30000; // 30kHz sample rate
    
    if (!sharedMemoryWriter_->initialize(numStreams, numChannels, sampleRate)) {
        std::cerr << "Failed to initialize shared memory writer." << std::endl;
        return false;
    }
    
    if (!openDevice()) {
        return false;
    }
    
    if (!uploadBitfile()) {
        return false;
    }
    
    if (!configureDevice()) {
        return false;
    }
    
    std::cout << "Intan Reader initialized successfully!" << std::endl;
    return true;
}

bool IntanReader::openDevice() {
    std::cout << "Opening Opal Kelly board..." << std::endl;
    int result = controller_->open();
    if (result != 1) {
        std::cerr << "Failed to open Opal Kelly board. Error code: " << result << std::endl;
        std::cerr << "No Intan device connected." << std::endl;
        return false;
    }
    return true;
}

bool IntanReader::uploadBitfile() {
    auto fileExists = [](const char* path) -> bool { 
        struct stat st{}; 
        return path && (::stat(path, &st) == 0) && S_ISREG(st.st_mode); 
    };
    
    const char* envPath = std::getenv("RHD_BITFILE");
    bool bitfileUploaded = false;
    
    if (envPath && fileExists(envPath)) {
        std::cout << "Uploading FPGA bitfile: " << envPath << std::endl;
        bitfileUploaded = controller_->uploadFpgaBitfile(std::string(envPath));
    } else if (fileExists("intan-reader/FPGA-bitfiles/ConfigRHDController.bit")) {
        std::cout << "Uploading FPGA bitfile: intan-reader/FPGA-bitfiles/ConfigRHDController.bit" << std::endl;
        bitfileUploaded = controller_->uploadFpgaBitfile(std::string("intan-reader/FPGA-bitfiles/ConfigRHDController.bit"));
    }
    
    if (!bitfileUploaded) {
        std::cerr << "FPGA bitfile not found or upload failed. Place ConfigRHDController.bit and rerun." << std::endl;
        return false;
    }
    
    return true;
}

bool IntanReader::configureDevice() {
    // Initialize the controller
    controller_->initialize();
    
    controller_->setSampleRate(Rhd2000EvalBoardUsb3::SampleRate30000Hz);
    controller_->setCableLengthFeet(Rhd2000EvalBoardUsb3::PortA, 3.0);
    controller_->enableDataStream(0, true);

    Rhd2000RegistersUsb3* chipRegisters = new Rhd2000RegistersUsb3(controller_->getSampleRate());
    
    // Before generating register configuration command sequences, set amplifier
    // bandwidth paramters.

    // set amplifier bandwidths and DSP cutoff before building register lists
    double dspCutoffFreq = chipRegisters->setDspCutoffFreq(10.0);
    chipRegisters->setLowerBandwidth(1.0);
    chipRegisters->setUpperBandwidth(7500.0);
    
    std::cout << "Amplifier configuration:" << std::endl;
    std::cout << "  DSP cutoff frequency: " << dspCutoffFreq << " Hz" << std::endl;
    std::cout << "  Lower bandwidth: 1.0 Hz" << std::endl;
    std::cout << "  Upper bandwidth: 7500.0 Hz" << std::endl;
    
    std::vector<int> commandList;
    int commandSequenceLength = 0;
    
    // AuxCmd1
    // First, let's create a command list for the AuxCmd1 slot.  This command
    // sequence will create a 1 kHz, full-scale sine wave for impedance testing.
    commandSequenceLength = chipRegisters->createCommandListZcheckDac(commandList, 1000.0, 128.0);
    controller_->uploadCommandList(commandList, Rhd2000EvalBoardUsb3::AuxCmd1, 0);
    controller_->selectAuxCommandLength(Rhd2000EvalBoardUsb3::AuxCmd1, 0, commandSequenceLength - 1);
    controller_->selectAuxCommandBank(Rhd2000EvalBoardUsb3::PortA, Rhd2000EvalBoardUsb3::AuxCmd1, 0);
    
    // AuxCmd2
    // Next, we'll create a command list for the AuxCmd2 slot.  This command sequence
    // will sample the temperature sensor and other auxiliary ADC inputs.
    commandSequenceLength = chipRegisters->createCommandListTempSensor(commandList);
    controller_->uploadCommandList(commandList, Rhd2000EvalBoardUsb3::AuxCmd2, 0);
    controller_->selectAuxCommandLength(Rhd2000EvalBoardUsb3::AuxCmd2, 0, commandSequenceLength - 1);
    controller_->selectAuxCommandBank(Rhd2000EvalBoardUsb3::PortA, Rhd2000EvalBoardUsb3::AuxCmd2, 0);
    
    // AuxCmd3
    // For the AuxCmd3 slot, we will create two command sequences.  Both sequences
    // will configure and read back the RHD2000 chip registers, but one sequence will
    // also run ADC calibration.
    int lenNoCal = chipRegisters->createCommandListRegisterConfig(commandList, false);
    controller_->uploadCommandList(commandList, Rhd2000EvalBoardUsb3::AuxCmd3, 0);
    int lenCal = chipRegisters->createCommandListRegisterConfig(commandList, true);
    controller_->uploadCommandList(commandList, Rhd2000EvalBoardUsb3::AuxCmd3, 1);
    
    // Run calibration once on bank 1
    controller_->selectAuxCommandLength(Rhd2000EvalBoardUsb3::AuxCmd3, 0, lenCal - 1);
    controller_->selectAuxCommandBank(Rhd2000EvalBoardUsb3::PortA, Rhd2000EvalBoardUsb3::AuxCmd3, 1);
    controller_->setMaxTimeStep(128);
    controller_->setContinuousRunMode(false);
    controller_->run();
    while (controller_->isRunning()) {}
    
    // Read calibration data
    Rhd2000DataBlockUsb3 calib(controller_->getNumEnabledDataStreams());
    controller_->readDataBlock(&calib);
    
    // Switch to bank 0 for normal acquisition
    controller_->selectAuxCommandLength(Rhd2000EvalBoardUsb3::AuxCmd3, 0, lenNoCal - 1);
    controller_->selectAuxCommandBank(Rhd2000EvalBoardUsb3::PortA, Rhd2000EvalBoardUsb3::AuxCmd3, 0);
    controller_->setContinuousRunMode(true);
    controller_->run();
    
    return true;
}

bool IntanReader::start() {
    if (running_) {
        std::cout << "Reader is already running." << std::endl;
        return true;
    }
    
    std::cout << "Starting continuous data acquisition..." << std::endl;
    
    running_ = true;
    
    // Start data reading loop in a separate thread
    std::thread dataThread(&IntanReader::readDataLoop, this);
    dataThread.detach();
    
    return true;
}

void IntanReader::stop() {
    if (!running_) {
        return;
    }
    
    std::cout << "Stopping data acquisition..." << std::endl;
    
    try {
        if (controller_) {
            controller_->flush();
        }
    } catch (...) {
        // Ignore any exceptions during flush
    }
    
    running_ = false;
    std::cout << "Data acquisition stopped." << std::endl;
}

void IntanReader::readDataLoop() {

    const int streams = controller_->getNumEnabledDataStreams();
    Rhd2000DataBlockUsb3 block(streams);
    const unsigned int wordsPerBlock = Rhd2000DataBlockUsb3::calculateDataBlockSizeInWords(streams);
    
    std::cout << "HW publisher: wordsPerBlock=" << wordsPerBlock << " streams=" << streams << std::endl;
    std::cout << "Reading waveform data continuously..." << std::endl;
    
    uint32_t timestamp = 0;
    
    while (running_) {
        unsigned int fifoWords = controller_->getNumWordsInFifo();
        if (fifoWords < wordsPerBlock) { 
            usleep(200); 
            continue; 
        }
        
        while (controller_->getNumWordsInFifo() >= wordsPerBlock) {
            if (!controller_->readDataBlock(&block)) break;
            
            if (sharedMemoryWriter_) {
                // Create a 3D array: [stream][channel][sample]
                std::vector<std::vector<std::vector<int>>> amplifierData;
                const int channelsPerStream = CHANNELS_PER_STREAM;
                const int samplesPerBlock = SAMPLES_PER_DATA_BLOCK;
                
                amplifierData.resize(streams);
                
                // Use EXACT same loop structure and data access as working Decoupled version
                auto computeIndex = [streams](int s, int ch, int t) { 
                    return (t * streams * CHANNELS_PER_STREAM) + (ch * streams) + s; 
                };
                
                for (int stream = 0; stream < streams; ++stream) {
                    amplifierData[stream].resize(channelsPerStream);
                    for (int ch = 0; ch < channelsPerStream; ++ch) {
                        amplifierData[stream][ch].resize(samplesPerBlock);
                        for (int t = 0; t < samplesPerBlock; ++t) {
                            amplifierData[stream][ch][t] = block.amplifierDataFast[computeIndex(stream, ch, t)];
                        }
                    }
                }
                
                sharedMemoryWriter_->writeDataBlock(timestamp, amplifierData);
            }
            
            timestamp += SAMPLES_PER_DATA_BLOCK;
        }
    }
    
}