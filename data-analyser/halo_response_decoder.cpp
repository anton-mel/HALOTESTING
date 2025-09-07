#include "halo_response_decoder.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>

HaloResponseDecoder::HaloResponseDecoder() 
    : currentPipeline_(HaloPipeline::PIPELINE_0)
    , lowThreshold_(0.3)
    , highThreshold_(0.7) {
}

HaloResponseDecoder::~HaloResponseDecoder() {
}

HaloResponse HaloResponseDecoder::decodeResponse(const std::vector<uint8_t>& rawData) {
    HaloResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.raw_data = rawData.empty() ? 0 : rawData[0];
    
    // Check if this is test pattern data
    if (isTestPattern(rawData)) {
        response.type = HaloResponseType::TEST_PATTERN;
        response.description = "Test pattern detected - sequential counter data";
        response.confidence = 1.0;
        response.pipeline = currentPipeline_;
        return response;
    }
    
    // Analyze based on current pipeline
    switch (currentPipeline_) {
        case HaloPipeline::PIPELINE_0:
            response = analyzePipeline0(rawData);
            break;
        case HaloPipeline::PIPELINE_1:
            response = analyzePipeline1(rawData);
            break;
        case HaloPipeline::PIPELINE_2:
            response = analyzePipeline2(rawData);
            break;
        case HaloPipeline::PIPELINE_6:
            response = analyzePipeline6(rawData);
            break;
        case HaloPipeline::PIPELINE_9:
            response = analyzePipeline9(rawData);
            break;
        default:
            response.type = HaloResponseType::UNKNOWN;
            response.description = "Unsupported pipeline";
            response.confidence = 0.0;
            break;
    }
    
    response.pipeline = currentPipeline_;
    return response;
}

void HaloResponseDecoder::setPipeline(HaloPipeline pipeline) {
    currentPipeline_ = pipeline;
}

void HaloResponseDecoder::setThresholds(double lowThreshold, double highThreshold) {
    lowThreshold_ = lowThreshold;
    highThreshold_ = highThreshold;
}

std::string HaloResponseDecoder::getPipelineDescription(HaloPipeline pipeline) const {
    switch (pipeline) {
        case HaloPipeline::PIPELINE_0: return "ADC -> LZ -> LIC -> Sink";
        case HaloPipeline::PIPELINE_1: return "ADC -> LZ -> MA -> RC -> Sink";
        case HaloPipeline::PIPELINE_2: return "ADC -> DWT -> TOK -> MA -> RC -> Sink";
        case HaloPipeline::PIPELINE_3: return "ADC -> Sink";
        case HaloPipeline::PIPELINE_4: return "ADC_b -> NEO -> THR -> GATE, ADC_a -> LZ -> LIC -> GATE, GATE -> Sink";
        case HaloPipeline::PIPELINE_5: return "ADC_b -> NEO -> THR -> GATE, ADC_a -> LZ -> MA -> RC -> GATE, GATE -> Sink";
        case HaloPipeline::PIPELINE_6: return "ADC_b -> NEO -> THR -> GATE, ADC_a -> GATE, GATE -> Sink";
        case HaloPipeline::PIPELINE_7: return "ADC_b -> DWT -> THR -> GATE, ADC_a -> LZ -> LIC -> GATE, GATE -> Sink";
        case HaloPipeline::PIPELINE_8: return "ADC_b -> DWT -> THR -> GATE, ADC_a -> LZ -> MA -> RC -> GATE, GATE -> Sink";
        case HaloPipeline::PIPELINE_9: return "ADC_b -> DWT -> THR -> GATE, ADC_a -> GATE, GATE -> Sink";
        default: return "Unknown pipeline";
    }
}

bool HaloResponseDecoder::isTestPattern(const std::vector<uint8_t>& data) const {
    if (data.size() < 10) return false;
    
    // Check for sequential counter pattern (170,171,172...255,0,1,2...)
    bool isSequential = true;
    for (size_t i = 1; i < std::min(data.size(), size_t(100)); ++i) {
        uint8_t expected = (data[i-1] + 1) % 256;
        if (data[i] != expected) {
            isSequential = false;
            break;
        }
    }
    
    return isSequential;
}

HaloResponseType HaloResponseDecoder::analyzeForSeizure(const std::vector<uint8_t>& data) const {
    if (isTestPattern(data)) {
        return HaloResponseType::TEST_PATTERN;
    }
    
    double activityLevel = calculateActivityLevel(data);
    
    if (activityLevel > highThreshold_) {
        return HaloResponseType::SEIZURE_DETECTED;
    } else if (activityLevel > lowThreshold_) {
        return HaloResponseType::THRESHOLD_EXCEEDED;
    } else {
        return HaloResponseType::NORMAL_ACTIVITY;
    }
}

bool HaloResponseDecoder::detectCounterPattern(const std::vector<uint8_t>& data) const {
    return isTestPattern(data);
}

bool HaloResponseDecoder::detectSeizurePattern(const std::vector<uint8_t>& data) const {
    // Simple seizure detection based on high-frequency activity
    double activityLevel = calculateActivityLevel(data);
    return activityLevel > highThreshold_;
}

double HaloResponseDecoder::calculateActivityLevel(const std::vector<uint8_t>& data) const {
    if (data.empty()) return 0.0;
    
    // Calculate variance as a measure of activity
    double mean = 0.0;
    for (uint8_t value : data) {
        mean += value;
    }
    mean /= data.size();
    
    double variance = 0.0;
    for (uint8_t value : data) {
        double diff = value - mean;
        variance += diff * diff;
    }
    variance /= data.size();
    
    // Normalize to 0-1 range
    return std::min(1.0, variance / (128.0 * 128.0));
}

HaloResponse HaloResponseDecoder::analyzePipeline0(const std::vector<uint8_t>& data) const {
    HaloResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.raw_data = data.empty() ? 0 : data[0];
    
    // Pipeline 0: ADC -> LZ -> LIC -> Sink
    // LZ: Lempel-Ziv compression (8-bit input, 64-bit output)
    // LIC: Lempel-Ziv decompression (64-bit input, 8-bit output)
    
    double activityLevel = calculateActivityLevel(data);
    response.confidence = activityLevel;
    
    if (activityLevel > highThreshold_) {
        response.type = HaloResponseType::SEIZURE_DETECTED;
        response.description = "High activity detected in LZ-LIC pipeline";
    } else if (activityLevel > lowThreshold_) {
        response.type = HaloResponseType::THRESHOLD_EXCEEDED;
        response.description = "Elevated activity in LZ-LIC pipeline";
    } else {
        response.type = HaloResponseType::NORMAL_ACTIVITY;
        response.description = "Normal activity in LZ-LIC pipeline";
    }
    
    response.activity_level = activityLevel;
    response.secondary_metric = data.size() > 0 ? (data.size() / 8.0) : 0.0;
    
    return response;
}

HaloResponse HaloResponseDecoder::analyzePipeline1(const std::vector<uint8_t>& data) const {
    HaloResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.raw_data = data.empty() ? 0 : data[0];
    
    // Pipeline 1: ADC -> LZ -> MA -> RC -> Sink
    // MA: Matrix operations (64-bit input/output)
    // RC: Run-length coding (64-bit input, 8-bit output)
    
    double activityLevel = calculateActivityLevel(data);
    response.confidence = activityLevel;
    
    if (activityLevel > highThreshold_) {
        response.type = HaloResponseType::SEIZURE_DETECTED;
        response.description = "High activity detected in LZ-MA-RC pipeline";
    } else if (activityLevel > lowThreshold_) {
        response.type = HaloResponseType::THRESHOLD_EXCEEDED;
        response.description = "Elevated activity in LZ-MA-RC pipeline";
    } else {
        response.type = HaloResponseType::NORMAL_ACTIVITY;
        response.description = "Normal activity in LZ-MA-RC pipeline";
    }
    
    response.activity_level = activityLevel;
    response.secondary_metric = data.size() > 0 ? (data.size() / 8.0) : 0.0;
    
    return response;
}

HaloResponse HaloResponseDecoder::analyzePipeline2(const std::vector<uint8_t>& data) const {
    HaloResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.raw_data = data.empty() ? 0 : data[0];
    
    // Pipeline 2: ADC -> DWT -> TOK -> MA -> RC -> Sink
    // DWT: Discrete Wavelet Transform (8-bit input, 16-bit output)
    // TOK: Tokenization (16-bit input, 64-bit output)
    // MA: Matrix operations (64-bit input/output)
    // RC: Run-length coding (64-bit input, 8-bit output)
    
    double activityLevel = calculateActivityLevel(data);
    response.confidence = activityLevel;
    
    if (activityLevel > highThreshold_) {
        response.type = HaloResponseType::SEIZURE_DETECTED;
        response.description = "High activity detected in DWT-TOK-MA-RC pipeline";
    } else if (activityLevel > lowThreshold_) {
        response.type = HaloResponseType::THRESHOLD_EXCEEDED;
        response.description = "Elevated activity in DWT-TOK-MA-RC pipeline";
    } else {
        response.type = HaloResponseType::NORMAL_ACTIVITY;
        response.description = "Normal activity in DWT-TOK-MA-RC pipeline";
    }
    
    response.activity_level = activityLevel;
    response.secondary_metric = data.size() > 0 ? (data.size() / 2.0) : 0.0;
    
    return response;
}

HaloResponse HaloResponseDecoder::analyzePipeline6(const std::vector<uint8_t>& data) const {
    HaloResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.raw_data = data.empty() ? 0 : data[0];
    
    // Pipeline 6: ADC_b -> NEO -> THR -> GATE, ADC_a -> GATE, GATE -> Sink
    // NEO: Neural operator (8-bit input, 32-bit output)
    // THR: Thresholding (32-bit input, 1-bit output)
    // GATE: Gating logic (8-bit input/output)
    
    double activityLevel = calculateActivityLevel(data);
    response.confidence = activityLevel;
    
    if (activityLevel > highThreshold_) {
        response.type = HaloResponseType::SEIZURE_DETECTED;
        response.description = "High activity detected in NEO-THR-GATE pipeline";
    } else if (activityLevel > lowThreshold_) {
        response.type = HaloResponseType::THRESHOLD_EXCEEDED;
        response.description = "Elevated activity in NEO-THR-GATE pipeline";
    } else {
        response.type = HaloResponseType::NORMAL_ACTIVITY;
        response.description = "Normal activity in NEO-THR-GATE pipeline";
    }
    
    response.activity_level = activityLevel;
    response.secondary_metric = data.size() > 0 ? (data.size() / 4.0) : 0.0;
    
    return response;
}

HaloResponse HaloResponseDecoder::analyzePipeline9(const std::vector<uint8_t>& data) const {
    HaloResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.raw_data = data.empty() ? 0 : data[0];
    
    // Pipeline 9: ADC_b -> DWT -> THR -> GATE, ADC_a -> GATE, GATE -> Sink
    // DWT: Discrete Wavelet Transform (8-bit input, 16-bit output)
    // THR: Thresholding (32-bit input, 1-bit output)
    // GATE: Gating logic (8-bit input/output)
    
    double activityLevel = calculateActivityLevel(data);
    response.confidence = activityLevel;
    
    if (activityLevel > highThreshold_) {
        response.type = HaloResponseType::SEIZURE_DETECTED;
        response.description = "High activity detected in DWT-THR-GATE pipeline";
    } else if (activityLevel > lowThreshold_) {
        response.type = HaloResponseType::THRESHOLD_EXCEEDED;
        response.description = "Elevated activity in DWT-THR-GATE pipeline";
    } else {
        response.type = HaloResponseType::NORMAL_ACTIVITY;
        response.description = "Normal activity in DWT-THR-GATE pipeline";
    }
    
    response.activity_level = activityLevel;
    response.secondary_metric = data.size() > 0 ? (data.size() / 2.0) : 0.0;
    
    return response;
}

std::string HaloResponseDecoder::responseTypeToString(HaloResponseType type) const {
    switch (type) {
        case HaloResponseType::SEIZURE_DETECTED: return "SEIZURE_DETECTED";
        case HaloResponseType::NORMAL_ACTIVITY: return "NORMAL_ACTIVITY";
        case HaloResponseType::THRESHOLD_EXCEEDED: return "THRESHOLD_EXCEEDED";
        case HaloResponseType::PROCESSING_ERROR: return "PROCESSING_ERROR";
        case HaloResponseType::TEST_PATTERN: return "TEST_PATTERN";
        case HaloResponseType::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

std::string HaloResponseDecoder::pipelineToString(HaloPipeline pipeline) const {
    return getPipelineDescription(pipeline);
}
