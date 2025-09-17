#ifndef FPGA_LOGGER_H
#define FPGA_LOGGER_H

#include <vector>
#include <string>
#include <memory>
#include <map>

#include "halo_response_decoder.h"

class FpgaLogger {
private:
    HaloResponseDecoder decoder_;
    std::map<int, std::unique_ptr<class Hdf5Writer>> hourlyWriters_; // hour -> writer
    int responseCount_;
    
public:
    FpgaLogger();
    ~FpgaLogger();
    
    // Analyze FPGA response data with HALO decoding
    void analyzeFpgaData(const std::vector<uint8_t>& fpgaData, const std::vector<uint8_t>& originalData);
    
    // Set HALO pipeline configuration
    void setHaloPipeline(HaloPipeline pipeline);
    
    // Set seizure detection thresholds
    void setThresholds(double lowThreshold, double highThreshold);
    
private:
    void logFpgaResponseToHdf5(const HaloResponse& response, const std::vector<uint8_t>& processedData, const std::vector<uint8_t>& originalData);
    void createHourlyWriter(int hour);
    std::string getDateString() const;
    std::string getHourString() const;
    int getCurrentHour() const;
};

#endif // FPGA_LOGGER_H
