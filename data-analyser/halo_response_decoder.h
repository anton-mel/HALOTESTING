#ifndef HALO_RESPONSE_DECODER_H
#define HALO_RESPONSE_DECODER_H

#include <vector>
#include <string>
#include <map>
#include <chrono>

// HALO Pipeline Definitions (from VerifyHalo documentation)
enum class HaloPipeline {
    PIPELINE_0 = 0,  // ADC -> LZ -> LIC -> Sink
    PIPELINE_1 = 1,  // ADC -> LZ -> MA -> RC -> Sink
    PIPELINE_2 = 2,  // ADC -> DWT -> TOK -> MA -> RC -> Sink
    PIPELINE_3 = 3,  // ADC -> Sink
    PIPELINE_4 = 4,  // ADC_b -> NEO -> THR -> GATE, ADC_a -> LZ -> LIC -> GATE, GATE -> Sink
    PIPELINE_5 = 5,  // ADC_b -> NEO -> THR -> GATE, ADC_a -> LZ -> MA -> RC -> GATE, GATE -> Sink
    PIPELINE_6 = 6,  // ADC_b -> NEO -> THR -> GATE, ADC_a -> GATE, GATE -> Sink
    PIPELINE_7 = 7,  // ADC_b -> DWT -> THR -> GATE, ADC_a -> LZ -> LIC -> GATE, GATE -> Sink
    PIPELINE_8 = 8,  // ADC_b -> DWT -> THR -> GATE, ADC_a -> LZ -> MA -> RC -> GATE, GATE -> Sink
    PIPELINE_9 = 9   // ADC_b -> DWT -> THR -> GATE, ADC_a -> GATE, GATE -> Sink
};

// HALO Response Types
enum class HaloResponseType {
    SEIZURE_DETECTED,     // Seizure pattern detected
    NORMAL_ACTIVITY,      // Normal neural activity
    THRESHOLD_EXCEEDED,   // Threshold exceeded but not seizure
    PROCESSING_ERROR,     // Processing error
    TEST_PATTERN,         // Test/pattern data (like current logs)
    UNKNOWN               // Unknown response type
};

// HALO Response Structure
struct HaloResponse {
    std::chrono::system_clock::time_point timestamp;
    HaloResponseType type;
    HaloPipeline pipeline;
    uint8_t raw_data;
    std::string description;
    double confidence;  // Confidence level (0.0 to 1.0)
    double activity_level;  // Main activity metric
    double secondary_metric;  // Secondary metric (compression ratio, etc.)
    
    HaloResponse() : type(HaloResponseType::UNKNOWN), pipeline(HaloPipeline::PIPELINE_0), 
                     raw_data(0), confidence(0.0), activity_level(0.0), secondary_metric(0.0) {}
};

class HaloResponseDecoder {
public:
    HaloResponseDecoder();
    ~HaloResponseDecoder();
    
    // Decode raw FPGA response data
    HaloResponse decodeResponse(const std::vector<uint8_t>& rawData);
    
    // Set current pipeline configuration
    void setPipeline(HaloPipeline pipeline);
    
    // Set thresholds for seizure detection
    void setThresholds(double lowThreshold, double highThreshold);
    
    // Get pipeline description
    std::string getPipelineDescription(HaloPipeline pipeline) const;
    
    // Check if data appears to be test pattern
    bool isTestPattern(const std::vector<uint8_t>& data) const;
    
    // Analyze data for seizure patterns
    HaloResponseType analyzeForSeizure(const std::vector<uint8_t>& data) const;
    
    // Utility functions
    std::string responseTypeToString(HaloResponseType type) const;
    std::string pipelineToString(HaloPipeline pipeline) const;

private:
    HaloPipeline currentPipeline_;
    double lowThreshold_;
    double highThreshold_;
    
    // Pattern detection methods
    bool detectCounterPattern(const std::vector<uint8_t>& data) const;
    bool detectSeizurePattern(const std::vector<uint8_t>& data) const;
    double calculateActivityLevel(const std::vector<uint8_t>& data) const;
    
    // Pipeline-specific analysis
    HaloResponse analyzePipeline0(const std::vector<uint8_t>& data) const;  // LZ -> LIC
    HaloResponse analyzePipeline1(const std::vector<uint8_t>& data) const;  // LZ -> MA -> RC
    HaloResponse analyzePipeline2(const std::vector<uint8_t>& data) const;  // DWT -> TOK -> MA -> RC
    HaloResponse analyzePipeline6(const std::vector<uint8_t>& data) const;  // NEO -> THR -> GATE
    HaloResponse analyzePipeline9(const std::vector<uint8_t>& data) const;  // DWT -> THR -> GATE
    
};

#endif // HALO_RESPONSE_DECODER_H
