#ifndef FPGA_RAW_LOGGER_H
#define FPGA_RAW_LOGGER_H

#include <vector>
#include <string>
#include <fstream>

class FpgaRawLogger {
private:
    std::ofstream logFile_;
    std::string logFilePath_;
    
public:
    FpgaRawLogger();
    ~FpgaRawLogger();
    
    // Analyze FPGA response data - just log raw data
    void analyzeFpgaData(const std::vector<uint8_t>& fpgaData);
    
private:
    void openLogFile();
    void writeToLog(const std::string& message);
    std::string get_date_string() const;
};

#endif // FPGA_RAW_LOGGER_H
