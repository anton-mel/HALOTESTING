#ifndef FPGA_LOGGER_H
#define FPGA_LOGGER_H

#include <vector>
#include <string>
#include <fstream>

#include "halo_response_decoder.h"

class FpgaLogger {
private:
    std::ofstream logFile_;
    std::string logFilePath_;
    HaloResponseDecoder decoder_;
    
public:
    FpgaLogger();
    ~FpgaLogger();
    
    // Analyze FPGA response data with HALO decoding
    void analyzeFpgaData(const std::vector<uint8_t>& fpgaData);
    
    // Set HALO pipeline configuration
    void setHaloPipeline(HaloPipeline pipeline);
    
    // Set seizure detection thresholds
    void setThresholds(double lowThreshold, double highThreshold);
    
private:
    void openLogFile();
    void writeToLog(const std::string& message);
    void writeDecodedResponse(const HaloResponse& response);
    std::string get_date_string() const;
};

#endif // FPGA_LOGGER_H
