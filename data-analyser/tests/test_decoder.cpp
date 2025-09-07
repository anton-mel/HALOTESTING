#include "../halo_response_decoder.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== HALO ASIC Response Decoder Test ===" << std::endl;
    
    HaloResponseDecoder decoder;
    
    // Test 1: Counter pattern (like current logs)
    std::cout << "\n--- Test 1: Counter Pattern (Test Data) ---" << std::endl;
    std::vector<uint8_t> counterData = {170, 171, 172, 173, 174, 175, 176, 177, 178, 179};
    HaloResponse response1 = decoder.decodeResponse(counterData);
    
    std::cout << "Type: " << decoder.responseTypeToString(response1.type) << std::endl;
    std::cout << "Description: " << response1.description << std::endl;
    std::cout << "Confidence: " << response1.confidence << std::endl;
    std::cout << "Pipeline: " << decoder.getPipelineDescription(response1.pipeline) << std::endl;
    
    // Test 2: Random data (potential seizure)
    std::cout << "\n--- Test 2: Random Data (Potential Seizure) ---" << std::endl;
    std::vector<uint8_t> randomData = {200, 50, 180, 25, 220, 75, 190, 45, 210, 65};
    decoder.setPipeline(HaloPipeline::PIPELINE_6);  // NEO -> THR -> GATE
    HaloResponse response2 = decoder.decodeResponse(randomData);
    
    std::cout << "Type: " << decoder.responseTypeToString(response2.type) << std::endl;
    std::cout << "Description: " << response2.description << std::endl;
    std::cout << "Confidence: " << response2.confidence << std::endl;
    std::cout << "Pipeline: " << decoder.getPipelineDescription(response2.pipeline) << std::endl;
    
    // Test 3: Low activity data (normal)
    std::cout << "\n--- Test 3: Low Activity Data (Normal) ---" << std::endl;
    std::vector<uint8_t> lowActivityData = {128, 129, 130, 131, 132, 133, 134, 135};
    decoder.setPipeline(HaloPipeline::PIPELINE_0);  // LZ -> LIC
    HaloResponse response3 = decoder.decodeResponse(lowActivityData);
    
    std::cout << "Type: " << decoder.responseTypeToString(response3.type) << std::endl;
    std::cout << "Description: " << response3.description << std::endl;
    std::cout << "Confidence: " << response3.confidence << std::endl;
    std::cout << "Pipeline: " << decoder.getPipelineDescription(response3.pipeline) << std::endl;
    
    // Test 4: High activity data (seizure)
    std::cout << "\n--- Test 4: High Activity Data (Seizure) ---" << std::endl;
    std::vector<uint8_t> highActivityData = {255, 0, 255, 0, 255, 0, 255, 0, 255, 0};
    decoder.setPipeline(HaloPipeline::PIPELINE_9);  // DWT -> THR -> GATE
    HaloResponse response4 = decoder.decodeResponse(highActivityData);
    
    std::cout << "Type: " << decoder.responseTypeToString(response4.type) << std::endl;
    std::cout << "Description: " << response4.description << std::endl;
    std::cout << "Confidence: " << response4.confidence << std::endl;
    std::cout << "Pipeline: " << decoder.getPipelineDescription(response4.pipeline) << std::endl;
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}
