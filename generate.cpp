#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint> // For uint16_t
#include <cmath>   // For round

// --- 檔案參數 ---
const int WIDTH = 640;
const int HEIGHT = 480;
const int FRAME_COUNT = 20;
const char* FILENAME = "gradient_480p_10bit_20f.yuv";

// YUV420 格式的平面大小計算
// 每個樣本使用 16 位元 (2 bytes) 來儲存 10-bit 數值
const int Y_PLANE_SIZE = WIDTH * HEIGHT;
const int UV_PLANE_SIZE = (WIDTH / 2) * (HEIGHT / 2); // 寬度和高度都是 Y 平面的一半

int main() {
    // 建立並以二進位模式開啟檔案
    std::ofstream file(FILENAME, std::ios::out | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << FILENAME << " for writing." << std::endl;
        return 1;
    }

    std::cout << "Generating file: " << FILENAME << std::endl;
    std::cout << "Resolution: " << WIDTH << "x" << HEIGHT << std::endl;
    std::cout << "Format: YUV420 Planar 10-bit" << std::endl;
    std::cout << "Frames: " << FRAME_COUNT << std::endl;

    // 建立 Y, U, V 平面的緩衝區
    // 使用 uint16_t 來儲存 10-bit 的資料
    std::vector<uint16_t> y_plane(Y_PLANE_SIZE);
    std::vector<uint16_t> u_plane(UV_PLANE_SIZE);
    std::vector<uint16_t> v_plane(UV_PLANE_SIZE);

    // --- 填充 U 和 V 平面 ---
    // 對於灰階影像，U 和 V 的值應為 10-bit 範圍的中點 (1024 / 2 = 512)
    // 只需要設定一次，因為每一幀的色度都相同
    const uint16_t neutral_chroma = 512;
    std::fill(u_plane.begin(), u_plane.end(), neutral_chroma);
    std::fill(v_plane.begin(), v_plane.end(), neutral_chroma);

    // --- 迴圈生成每一幀 ---
    for (int frame = 0; frame < FRAME_COUNT; ++frame) {
        // --- 填充 Y 平面 ---
        // 亮度從左 (0) 到右 (1023) 線性漸變
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                // 根據 x 座標計算亮度值
                double ratio = static_cast<double>(x) / (WIDTH - 1);
                uint16_t luma_value = static_cast<uint16_t>(round(ratio * 1023.0));
                y_plane[y * WIDTH + x] = luma_value;
            }
        }

        // --- 將三個平面依序寫入檔案 ---
        // 1. 寫入 Y 平面
        file.write(reinterpret_cast<const char*>(y_plane.data()), y_plane.size() * sizeof(uint16_t));
        
        // 2. 寫入 U 平面
        file.write(reinterpret_cast<const char*>(u_plane.data()), u_plane.size() * sizeof(uint16_t));

        // 3. 寫入 V 平面
        file.write(reinterpret_cast<const char*>(v_plane.data()), v_plane.size() * sizeof(uint16_t));
    }

    // 關閉檔案
    file.close();

    std::cout << "Successfully generated YUV file." << std::endl;
    
    // 計算並印出檔案大小
    long long expected_size = static_cast<long long>(Y_PLANE_SIZE + UV_PLANE_SIZE + UV_PLANE_SIZE) * sizeof(uint16_t) * FRAME_COUNT;
    std::cout << "Expected file size: " << expected_size / 1024 << " KB" << std::endl;

    return 0;
}