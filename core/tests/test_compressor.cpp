#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <random>
#include "../include/compressor.h"

using namespace Backup;

class CompressorTest : public ::testing::Test {
protected:
    Compressor compressor;

    // 辅助验证函数
    void verifyRoundTrip(const std::vector<uint8_t>& input, CompressionAlgorithm algo) {
        std::vector<uint8_t> compressed = compressor.compress(input, algo);
        
        // 2. 验证压缩数据的基本特征
        if (!input.empty()) {
            // 检查算法标识头 (第1个字节)
            ASSERT_FALSE(compressed.empty());
            EXPECT_EQ(compressed[0], static_cast<uint8_t>(algo));

            if (algo == CompressionAlgorithm::HUFFMAN) {
                // Huffman: AlgoByte(1) + FreqTable(2048) + Size(8) = 2057 bytes
                ASSERT_GE(compressed.size(), 2057);
            } else {
                // LZSS: AlgoByte(1) + Flag(1) + ... >= 2 bytes
                ASSERT_GE(compressed.size(), 2);
            }
        }

        std::vector<uint8_t> decompressed = compressor.decompress(compressed);
        
        ASSERT_EQ(input, decompressed) << "Decompressed data mismatch for algo: " << (int)algo;
    }
};

// ==========================================
// Part 1: Huffman 算法测试 (保留原有逻辑)
// ==========================================

TEST_F(CompressorTest, Huffman_BasicString) {
    std::string str = "Hello, Huffman!";
    std::vector<uint8_t> input(str.begin(), str.end());
    verifyRoundTrip(input, CompressionAlgorithm::HUFFMAN);
}

TEST_F(CompressorTest, Huffman_SingleCharacterRepeated) {
    std::vector<uint8_t> input(1000, 'A'); 
    verifyRoundTrip(input, CompressionAlgorithm::HUFFMAN);
}

TEST_F(CompressorTest, Huffman_FullByteRange) {
    std::vector<uint8_t> input;
    for (int i = 0; i < 256; ++i) input.push_back(static_cast<uint8_t>(i));
    for (int i = 0; i < 10; ++i) input.insert(input.end(), input.begin(), input.begin() + 256);
    verifyRoundTrip(input, CompressionAlgorithm::HUFFMAN);
}

TEST_F(CompressorTest, Huffman_LargeRandomData) {
    std::vector<uint8_t> input(1024 * 512); // 512KB
    std::mt19937 gen(12345);
    std::uniform_int_distribution<> dis(0, 255);
    for (auto& b : input) b = static_cast<uint8_t>(dis(gen));
    
    verifyRoundTrip(input, CompressionAlgorithm::HUFFMAN);
}

// ==========================================
// Part 2: LZSS (LZSS) 算法测试 (新增)
// ==========================================

TEST_F(CompressorTest, LZSS_BasicString) {
    std::string str = "Hello, LZSS!";
    std::vector<uint8_t> input(str.begin(), str.end());
    verifyRoundTrip(input, CompressionAlgorithm::LZSS);
}

TEST_F(CompressorTest, LZSS_SingleCharacterRepeated) {
    std::vector<uint8_t> input(1000, 'A'); 
    verifyRoundTrip(input, CompressionAlgorithm::LZSS);
}

// LZSS 强项测试：高度重复的文本
TEST_F(CompressorTest, LZSS_HighlyRepetitiveData) {
    std::string text = "";
    for(int i=0; i<100; ++i) {
        text += "The quick brown fox jumps over the lazy dog.\n";
    }
    std::vector<uint8_t> input(text.begin(), text.end());
    
    // 验证无损还原
    verifyRoundTrip(input, CompressionAlgorithm::LZSS);

    // 压缩率应该非常高
    auto compressed = compressor.compress(input, CompressionAlgorithm::LZSS);
    double ratio = (double)compressed.size() / input.size();
    
    // 这是一个非常容易压缩的模式，压缩率应该优于 50% (ratio < 0.5)
    EXPECT_LT(ratio, 0.5) << "LZSS compression ratio is unexpectedly poor for repetitive text.";
    std::cout << "[LZSS Info] Repetitive text compressed size: " << compressed.size() 
              << " / " << input.size() << " (" << ratio * 100 << "%)" << std::endl;
}

// LZSS 边界测试：长度刚好为3的重复（最小匹配）
TEST_F(CompressorTest, LZSS_MinMatchLength) {
    // "abc" 重复出现
    std::string str = "abc123abc456abc"; 
    std::vector<uint8_t> input(str.begin(), str.end());
    verifyRoundTrip(input, CompressionAlgorithm::LZSS);
}

// LZSS 边界测试：非常远的重复（超过窗口大小 4096）
TEST_F(CompressorTest, LZSS_FarDistanceMatch) {
    std::vector<uint8_t> input;
    // 写入一个模式
    std::string pattern = "PatternData";
    input.insert(input.end(), pattern.begin(), pattern.end());
    
    // 填充超过 4096 字节的垃圾数据
    for(int i=0; i<5000; ++i) input.push_back('x');
    
    // 再次写入模式
    input.insert(input.end(), pattern.begin(), pattern.end());

    // LZSS 默认窗口 4096，第二个 PatternData 可能无法匹配到第一个（太远了）
    verifyRoundTrip(input, CompressionAlgorithm::LZSS);
}

// ==========================================
// Part 3: 通用与异常测试
// ==========================================

TEST_F(CompressorTest, Universal_EmptyData) {
    std::vector<uint8_t> input;
    
    // 测试 Huffman 空
    auto comp1 = compressor.compress(input, CompressionAlgorithm::HUFFMAN);
    auto decomp1 = compressor.decompress(comp1);
    EXPECT_TRUE(decomp1.empty());

    // 测试 LZSS 空
    auto comp2 = compressor.compress(input, CompressionAlgorithm::LZSS);
    auto decomp2 = compressor.decompress(comp2);
    EXPECT_TRUE(decomp2.empty());
}

TEST_F(CompressorTest, Exception_InvalidHeader) {
    // 1. 数据为空（连算法头都没有）
    std::vector<uint8_t> emptyData;
    auto res = compressor.decompress(emptyData);
    EXPECT_TRUE(res.empty());

    // 2. 只有算法头，没有数据
    std::vector<uint8_t> headerOnly = { static_cast<uint8_t>(CompressionAlgorithm::HUFFMAN) };
    // Huffman 需要 Header，抛异常
    EXPECT_THROW(compressor.decompress(headerOnly), std::runtime_error);

    // 3. 未知的算法
    std::vector<uint8_t> badAlgo = { 0xFF, 0x01, 0x02 }; 
    EXPECT_THROW(compressor.decompress(badAlgo), std::runtime_error);
}

TEST_F(CompressorTest, Exception_LZSS_Corrupted) {
    // 构造一个看起来像 LZSS 但数据不完整的包
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(CompressionAlgorithm::LZSS));
    data.push_back(0xFF); // Flag: 全是引用 (需要后面有很多数据)
    data.push_back(0x00); // 只给 1 个字节数据，不够引用所需的 2 字节

    EXPECT_THROW(compressor.decompress(data), std::runtime_error);
}