#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <random>
#include "../include/compressor.h"

class CompressorTest : public ::testing::Test {
protected:
    Backup::Compressor compressor;

    // 辅助验证函数
    void verifyRoundTrip(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> compressed = compressor.compress(input);
        
        // 如果输入不为空，压缩数据至少应该包含头部 (256*8 + 8 = 2056 bytes)
        if (!input.empty()) {
            ASSERT_GE(compressed.size(), 2056);
        }

        std::vector<uint8_t> decompressed = compressor.decompress(compressed);
        ASSERT_EQ(input, decompressed) << "Decompressed data mismatch!";
    }
};

// 1. 基础字符串测试
TEST_F(CompressorTest, BasicString) {
    std::string str = "Hello, Huffman!";
    std::vector<uint8_t> input(str.begin(), str.end());
    verifyRoundTrip(input);
}

// 2. 空数据测试
TEST_F(CompressorTest, EmptyData) {
    std::vector<uint8_t> input;
    verifyRoundTrip(input);
}

// 3. 单一重复字符 (边界情况：树只有单边)
TEST_F(CompressorTest, SingleCharacterRepeated) {
    std::vector<uint8_t> input(1000, 'A'); // 1000个 'A'
    verifyRoundTrip(input);
    
    // 验证压缩效果：
    // Header (2056B) + Data. 'A'编码为1位 -> 1000 bits = 125 bytes.
    // Total approx 2181 bytes. 相比原始 1000 bytes 变大了 (因为Header开销)，这是正常的。
    // 但对于非常大的单一字符文件，压缩效果会体现出来。
}

// 4. 包含所有字节的二进制数据 (0x00 - 0xFF)
TEST_F(CompressorTest, FullByteRange) {
    std::vector<uint8_t> input;
    for (int i = 0; i < 256; ++i) {
        input.push_back(static_cast<uint8_t>(i));
    }
    // 重复几次以构建复杂的树
    for (int i = 0; i < 10; ++i) {
        input.insert(input.end(), input.begin(), input.begin() + 256);
    }
    verifyRoundTrip(input);
}

// 5. 较大的随机数据
TEST_F(CompressorTest, LargeRandomData) {
    std::vector<uint8_t> input(1024 * 1024); // 1MB
    // 使用固定的种子以保证测试可重复
    std::mt19937 gen(12345);
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& b : input) {
        b = static_cast<uint8_t>(dis(gen));
    }
    verifyRoundTrip(input);
}

// 6. 异常测试：解压垃圾数据
TEST_F(CompressorTest, DecompressCorruptedData) {
    // 1. 数据太小，连Header都不够
    std::vector<uint8_t> smallData(100, 0); 
    EXPECT_THROW(compressor.decompress(smallData), std::runtime_error);

    // 2. 数据够 Header 大小，但 Body 缺失
    std::string str = "Short";
    std::vector<uint8_t> validCompressed = compressor.compress({str.begin(), str.end()});
    
    // 截断数据，使其只有 Header，没有 Body
    // Header size = 2056. 
    if (validCompressed.size() > 2056) {
        validCompressed.resize(2056 + 1); // 只留 1 bit body，肯定不够
        // 这里可能会抛出异常，也可能不抛出（如果1 bit正好凑够了，虽然不太可能）
        // 主要是为了测试代码健壮性，不崩溃即可
        try {
            compressor.decompress(validCompressed);
        } catch (const std::exception&) {
            // 抛出异常是符合预期的
        }
    }
}