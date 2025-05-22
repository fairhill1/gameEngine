#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include "model.h"

// Simple example program to test direct binary mesh loading

// Simple command-line tool to convert .bin GLTF buffer to the format we need
int main() {
    // Initialize model class
    Model::init();
    
    // Path to the binary model
    const char* duckModelPath = "build/assets/rubber_duck_toy_1k.gltf/rubber_duck_toy.bin";
    
    // Load file into memory
    std::ifstream file(duckModelPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << duckModelPath << std::endl;
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
    
    std::cout << "Loaded bin file, size: " << size << " bytes" << std::endl;
    
    // Create a model and try to load the bin data
    Model model;
    if (model.processBinaryMesh(buffer)) {
        std::cout << "Successfully processed binary mesh!" << std::endl;
    } else {
        std::cerr << "Failed to process binary mesh" << std::endl;
        return 1;
    }
    
    // Clean up (not really needed since we're exiting)
    model.unload();
    
    return 0;
}