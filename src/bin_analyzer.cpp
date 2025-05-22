#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <iomanip>

// Simple analyzer for binary files to understand their structure
// specifically for glTF bin files

void printHexDump(const uint8_t* data, size_t size, size_t bytesPerRow = 16) {
    for (size_t i = 0; i < size; i += bytesPerRow) {
        // Print offset
        std::cout << std::setw(8) << std::setfill('0') << std::hex << i << ": ";
        
        // Print hex values
        for (size_t j = 0; j < bytesPerRow; j++) {
            if (i + j < size) {
                std::cout << std::setw(2) << std::setfill('0') << std::hex 
                          << static_cast<int>(data[i + j]) << " ";
            } else {
                std::cout << "   ";
            }
        }
        
        // Print ASCII representation
        std::cout << "  ";
        for (size_t j = 0; j < bytesPerRow; j++) {
            if (i + j < size) {
                uint8_t c = data[i + j];
                if (c >= 32 && c <= 126) {
                    std::cout << static_cast<char>(c);
                } else {
                    std::cout << ".";
                }
            } else {
                std::cout << " ";
            }
        }
        
        std::cout << std::endl;
    }
}

void analyzeFloats(const uint8_t* data, size_t size, size_t offset = 0, size_t count = 100) {
    const float* floats = reinterpret_cast<const float*>(data + offset);
    size_t floatCount = (size - offset) / sizeof(float);
    
    // Limit to requested count
    floatCount = std::min(floatCount, count);
    
    std::cout << "Analyzing as float array starting at offset " << offset << ":" << std::endl;
    
    for (size_t i = 0; i < floatCount; i++) {
        if (i % 3 == 0) {
            std::cout << std::endl << "  [" << i/3 << "] ";
        }
        std::cout << std::fixed << std::setprecision(6) << floats[i] << " ";
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // Default file path
    std::string filePath = "build/assets/rubber_duck_toy_1k.gltf/rubber_duck_toy.bin";
    
    // Allow user to specify file path
    if (argc > 1) {
        filePath = argv[1];
    }
    
    // Load file into memory
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << filePath << std::endl;
        return 1;
    }
    
    // Get file size and read data
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Failed to read file data" << std::endl;
        return 1;
    }
    
    std::cout << "Loaded bin file: " << filePath << std::endl;
    std::cout << "Size: " << size << " bytes" << std::endl << std::endl;
    
    // Print first 128 bytes as hex dump to understand header
    std::cout << "First 128 bytes:" << std::endl;
    printHexDump(buffer.data(), std::min(size_t(128), size_t(size)));
    
    // Analyze as float array (positions are typically float triplets at the start)
    analyzeFloats(buffer.data(), size);
    
    // Check if there appears to be a section with indices (uint16_t values)
    // Usually after the vertex positions, normals, etc.
    size_t positionEnd = size / 2; // Guess that positions take up first half
    std::cout << "\nPossible indices section:" << std::endl;
    printHexDump(buffer.data() + positionEnd, std::min(size_t(64), size - positionEnd));
    
    return 0;
}