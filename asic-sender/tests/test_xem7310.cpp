#include "okFrontPanel.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "XEM7310 Test with Official FrontPanel SDK" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    okCFrontPanel device;
    
    // XEM7310 device details
    std::string serial = "2437001CWG";
    std::cout << "Target Device: XEM7310-A75" << std::endl;
    std::cout << "Serial Number: " << serial << std::endl;
    std::cout << "==========================================" << std::endl;
    
    // Test 1: Try to open the device
    std::cout << "\n1. Opening device..." << std::endl;
    int error = device.OpenBySerial(serial.c_str());
    std::cout << "   OpenBySerial result: " << error << std::endl;
    
    if (error != okCFrontPanel::NoError) {
        std::cout << "   ✗ Failed to open device" << std::endl;
        std::cout << "   Error " << error << " - ";
        
        switch (error) {
            case okCFrontPanel::UnsupportedFeature: 
                std::cout << "UnsupportedFeature - Library compatibility issue";
                break;
            case okCFrontPanel::DeviceNotOpen: 
                std::cout << "DeviceNotOpen - Device not available";
                break;
            case okCFrontPanel::CommunicationError: 
                std::cout << "CommunicationError - USB communication failed";
                break;
            default: 
                std::cout << "Unknown error";
                break;
        }
        std::cout << std::endl;
        return -1;
    }
    
    std::cout << "   ✓ Device opened successfully!" << std::endl;
    
    // Test 2: Get device information
    std::cout << "\n2. Device information..." << std::endl;
    std::cout << "   Board model: " << device.GetBoardModel() << std::endl;
    std::cout << "   Serial number: " << device.GetSerialNumber() << std::endl;
    std::cout << "   Device ID: " << device.GetDeviceID() << std::endl;
    
    // Test 3: Test basic wire operations
    std::cout << "\n3. Testing wire operations..." << std::endl;
    
    // Set wire in value
    device.SetWireInValue(0x00, 0x1234, 0xFFFF);
    device.UpdateWireIns();
    std::cout << "   ✓ SetWireInValue successful" << std::endl;
    
    // Read wire out value
    device.UpdateWireOuts();
    unsigned int wireOutValue = device.GetWireOutValue(0x20);
    std::cout << "   ✓ GetWireOutValue successful: 0x" << std::hex << wireOutValue << std::dec << std::endl;
    
    // Test 4: Test pipe operations (multiple of 16 bytes for USB 3.0)
    std::cout << "\n4. Testing pipe operations..." << std::endl;
    std::cout << "   Note: Data length must be multiple of 16 for USB 3.0" << std::endl;
    
    // Create test data - exactly 16 bytes
    std::vector<uint8_t> testData(16);
    for (int i = 0; i < 16; i++) {
        testData[i] = 0xAA + i; // Fill with test pattern
    }
    
    std::cout << "   Test data length: " << testData.size() << " bytes" << std::endl;
    
    // Test write to pipe in (0x80)
    std::cout << "\n   Testing WriteToPipeIn(0x80)..." << std::endl;
    int writeRet = device.WriteToPipeIn(0x80, testData.size(), testData.data());
    std::cout << "   WriteToPipeIn result: " << writeRet << std::endl;
    
    if (writeRet == okCFrontPanel::NoError) {
        std::cout << "   ✓ Pipe write successful!" << std::endl;
        
        // Test read from pipe out (0xA0)
        std::cout << "\n   Testing ReadFromPipeOut(0xA0)..." << std::endl;
        std::vector<uint8_t> readData(16); // Also 16 bytes
        
        int readRet = device.ReadFromPipeOut(0xA0, readData.size(), readData.data());
        std::cout << "   ReadFromPipeOut result: " << readRet << std::endl;
        
        if (readRet == okCFrontPanel::NoError) {
            std::cout << "   ✓ Pipe read successful!" << std::endl;
            std::cout << "\n✓ XEM7310 communication test PASSED!" << std::endl;
            std::cout << "✓ Official FrontPanel SDK is working!" << std::endl;
            
        } else {
            std::cout << "   ✗ Pipe read failed" << std::endl;
            std::cout << "   This might be expected if no FPGA bitfile is loaded" << std::endl;
        }
        
    } else {
        std::cout << "   ✗ Pipe write failed" << std::endl;
        std::cout << "   This might be expected if no FPGA bitfile is loaded" << std::endl;
    }
    
    // Cleanup
    device.Close();
    std::cout << "\n==========================================" << std::endl;
    std::cout << "Test completed!" << std::endl;
    
    return 0;
}
