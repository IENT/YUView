#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint> // For uint16_t
#include <cmath>   // For round

// --- File parameters ---
const int WIDTH = 1280;
const int HEIGHT = 720;
const int FRAME_COUNT = 10;
const char* FILENAME = "gradient_720p_10bit_10f_yuv444.yuv";

// YUV444 plane size calculation
// Each sample uses 16 bits (2 bytes) to store 10-bit values
// YUV444: U and V planes are the same size as Y plane (no subsampling)
const int Y_PLANE_SIZE = WIDTH * HEIGHT;
const int U_PLANE_SIZE = WIDTH * HEIGHT;  // YUV444: U平面與Y平面相同大小
const int V_PLANE_SIZE = WIDTH * HEIGHT;  // YUV444: V平面與Y平面相同大小

int main() {
    // Create and open file in binary mode
    std::ofstream file(FILENAME, std::ios::out | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << FILENAME << " for writing." << std::endl;
        return 1;
    }

    std::cout << "Generating file: " << FILENAME << std::endl;
    std::cout << "Resolution: " << WIDTH << "x" << HEIGHT << " (720p)" << std::endl;
    std::cout << "Format: YUV444 Planar 10-bit" << std::endl;
    std::cout << "Frames: " << FRAME_COUNT << std::endl;

    // Create buffers for Y, U, V planes using uint16_t for 10-bit data
    std::vector<uint16_t> y_plane(Y_PLANE_SIZE);
    std::vector<uint16_t> u_plane(U_PLANE_SIZE);
    std::vector<uint16_t> v_plane(V_PLANE_SIZE);

    // --- Fill U and V planes ---
    // For grayscale, U and V should be the midpoint of 10-bit range (1024 / 2 = 512)
    const uint16_t neutral_chroma = 512;
    std::fill(u_plane.begin(), u_plane.end(), neutral_chroma);
    std::fill(v_plane.begin(), v_plane.end(), neutral_chroma);

    // --- Generate frames ---
    for (int frame = 0; frame < FRAME_COUNT; ++frame) {
        // --- Fill Y plane ---
        // Luma gradient from left (0) to right (1023)
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                // Compute luma value based on x coordinate
                double ratio = static_cast<double>(x) / (WIDTH - 1);
                uint16_t luma_value = static_cast<uint16_t>(round(ratio * 1023.0));
                y_plane[y * WIDTH + x] = luma_value;
            }
        }

        // --- Write three planes sequentially ---
        // 1. Write Y plane
        file.write(reinterpret_cast<const char*>(y_plane.data()), y_plane.size() * sizeof(uint16_t));
        
        // 2. Write U plane
        file.write(reinterpret_cast<const char*>(u_plane.data()), u_plane.size() * sizeof(uint16_t));

        // 3. Write V plane
        file.write(reinterpret_cast<const char*>(v_plane.data()), v_plane.size() * sizeof(uint16_t));
    }

    // Close file
    file.close();

    std::cout << "Successfully generated YUV file." << std::endl;
    
    // Compute and print expected file size
    long long expected_size = static_cast<long long>(Y_PLANE_SIZE + U_PLANE_SIZE + V_PLANE_SIZE) * sizeof(uint16_t) * FRAME_COUNT;
    std::cout << "Expected file size: " << expected_size / 1024 / 1024 << " MB" << std::endl;

    return 0;
}