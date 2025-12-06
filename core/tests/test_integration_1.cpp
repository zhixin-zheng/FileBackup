#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cassert>
#include <random>

#include "traverser.h"
#include "packer.h"
#include "compressor.h"

namespace fs = std::filesystem;
using namespace Backup;

std::vector<uint8_t> readFileToBuffer(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + path);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    return {};
}

void writeBufferToFile(const std::string& path, const std::vector<uint8_t>& buffer) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

void createTestEnvironment(const std::string& root) {
    if (fs::exists(root)) fs::remove_all(root);
    fs::create_directories(root);

    std::ofstream(root + "/hello.txt") << "Hello World! Huffman coding is cool.";
    std::ofstream(root + "/config.ini") << "setting=true\nvalue=100";
    
    std::string big_txt = "";
    std::mt19937 gen(42);
    std::normal_distribution<> dis(101.0, 15.0); 
    for (int i = 0; i < 1000000; i++) {
        int val = static_cast<int>(std::round(dis(gen)));
        // 限制在可打印字符范围内 (32-126)
        if (val < 32) val = 32;
        if (val > 126) val = 126;
        big_txt += static_cast<char>(val);
    }
    std::ofstream(root + "/bigfile.txt") << big_txt;

    std::string repeat_txt = "";
    for (int i = 0; i < 50000; i++) {
        repeat_txt += "ABCD1234";
    }
    std::ofstream(root + "/repeatfile.txt") << repeat_txt;
    
    // 创建子目录
    fs::create_directories(root + "/logs");
    std::ofstream(root + "/logs/app.log") << "[INFO] System started.";
}

bool compareFiles(const std::string& p1, const std::string& p2) {
    std::ifstream f1(p1, std::ios::binary | std::ios::ate);
    std::ifstream f2(p2, std::ios::binary | std::ios::ate);

    if (f1.fail() || f2.fail()) return false;
    if (f1.tellg() != f2.tellg()) return false; // 大小不同

    f1.seekg(0, std::ios::beg);
    f2.seekg(0, std::ios::beg);

    std::istreambuf_iterator<char> begin1(f1);
    std::istreambuf_iterator<char> begin2(f2);
    std::istreambuf_iterator<char> end;

    return std::equal(begin1, end, begin2);
}

int main() {
    const std::string TEST_ROOT = "./test_examples/";
    const std::string SRC_DIR = TEST_ROOT + "test_data_src";       // 原始目录
    const std::string DST_DIR = TEST_ROOT + "test_data_restore";   // 恢复目录
    const std::string TEMP_PACK = TEST_ROOT + "temp.pack";         // 中间打包文件(未压缩)
    const std::string FINAL_BACKUP = TEST_ROOT + "backup.bin";     // 最终文件(已压缩)
    const std::string RESTORED_PACK = TEST_ROOT + "restored.pack"; // 恢复的中间文件(已解压)

    try {
        std::cout << "[1/6] Setting up environment..." << std::endl;
        // createTestEnvironment(SRC_DIR);
        fs::remove(TEMP_PACK);
        fs::remove(FINAL_BACKUP);
        fs::remove(RESTORED_PACK);
        if (fs::exists(DST_DIR)) fs::remove_all(DST_DIR);

        Traverser traverser;
        Packer packer;
        Compressor compressor;

        std::cout << "[2/6] Starting BACKUP pipeline..." << std::endl;
        
        auto files = traverser.traverse(SRC_DIR);
        std::cout << "  - Scanned " << files.size() << " files." << std::endl;

        packer.pack(files, TEMP_PACK);
        std::cout << "  - Packed to intermediate file." << std::endl;

        std::vector<uint8_t> rawData = readFileToBuffer(TEMP_PACK);
        std::vector<uint8_t> compressedData = compressor.compress(rawData, CompressionAlgorithm::LZSS);
        writeBufferToFile(FINAL_BACKUP, compressedData);
        
        std::cout << "  - Compressed: " << rawData.size() << " bytes -> " 
                  << compressedData.size() << " bytes." << std::endl;

        std::cout << "[3/6] Starting RESTORE pipeline..." << std::endl;

        std::vector<uint8_t> readBackData = readFileToBuffer(FINAL_BACKUP);
        std::vector<uint8_t> decompressedData = compressor.decompress(readBackData);
        writeBufferToFile(RESTORED_PACK, decompressedData);
        std::cout << "  - Decompressed to intermediate file." << std::endl;

        packer.unpack(RESTORED_PACK, DST_DIR);
        std::cout << "  - Unpacked to destination directory." << std::endl;

        std::cout << "[4/6] Verifying integrity..." << std::endl;

        // bool ok1 = compareFiles(SRC_DIR + "/王学扬_郑智馨-电影鉴赏作业.mp4", DST_DIR + "/王学扬_郑智馨-电影鉴赏作业.mp4");
        bool ok1 = compareFiles(SRC_DIR + "/遮天（精校版）.txt", DST_DIR + "/遮天（精校版）.txt");
        // bool ok1 = compareFiles(SRC_DIR + "/hello.txt", DST_DIR + "/hello.txt");
        // bool ok2 = compareFiles(SRC_DIR + "/logs/app.log", DST_DIR + "/logs/app.log");
        // bool ok3 = compareFiles(SRC_DIR + "/bigfile.txt", DST_DIR + "/bigfile.txt");
        // bool ok4 = compareFiles(SRC_DIR + "/repeatfile.txt", DST_DIR + "/repeatfile.txt");

        if (ok1) {
            std::cout << "SUCCESS: Files verify matched!" << std::endl;
        } else {
            std::cerr << "FAILURE: Content mismatch!" << std::endl;
            return 1;
        }

        std::cout << "[5/6] Cleaning up..." << std::endl;
        // fs::remove(TEMP_PACK);
        // fs::remove(FINAL_BACKUP);
        // fs::remove(RESTORED_PACK);
        // fs::remove_all(SRC_DIR);
        // fs::remove_all(DST_DIR);

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION CAUGHT: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}