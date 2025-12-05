#include "compressor.h"
#include <queue>
#include <vector>

namespace Backup {

std::vector<uint8_t> Compressor::compress(const std::vector<uint8_t>& input) {
    // 计算字节频率
    std::vector<uint64_t> frequencies(256, 0);
    for (uint8_t byte : input) {
        frequencies[byte]++;
    }

    // 构建霍夫曼树
    HuffmanNode* root = buildHuffmanTree(frequencies);

    // 生成编码表
    std::unordered_map<uint8_t, std::string> codes;
    generateCodes(root, "", codes);

    std::vector<uint8_t> output;

    for (uint64_t freq:frequencies) {
        for (int i = 0; i < 8; i++) {
            output.push_back((freq >> (i * 8)) & 0xFF); // 以小端格式存储频率，将64位数切分位8个字节
        }
    }

    uint64_t originalSize = input.size();
    for (int i = 0; i < 8; i++) {
        output.push_back((originalSize >> (i * 8)) & 0xFF); // 以小端格式存储原始数据大小
    }

    uint8_t bitBuffer = 0;
    int bitCount = 0;

    for (uint8_t byte : input) {
        const std::string & code = codes[byte];
        for (char bit : code) {
            writeBit(output, bitBuffer, bitCount, bit);
        }
    }
    if (bitCount > 0) {
        output.push_back(bitBuffer);
    }

    return output;
}

std::vector<uint8_t> Compressor::decompress(const std::vector<uint8_t>& input) {
    size_t headerSize = 256 * 8 + 8; // 256个频率(8字节) + 原始大小(8字节)
    if (input.size() < headerSize) {
        throw std::runtime_error("Compressed data is too small to contain header");
    }

    std::vector<uint64_t> frequencies(256, 0);
    size_t inputIdx = 0;
    for (size_t i = 0; i < 256; i++) {
        uint64_t freq = 0;
        for (size_t j = 0; j < 8; j++) {
            freq |= static_cast<uint64_t>(input[inputIdx++]) << (j * 8); // 以小端格式读取频率，将8个字节合成64位数
        }
        frequencies[i] = freq;
    }

    uint64_t originalSize = 0;
    for (size_t i = 0; i < 8; i++) {
        originalSize |= static_cast<uint64_t>(input[inputIdx++]) << (i * 8);
    }

    if (originalSize == 0) {
        return std::vector<uint8_t>();
    }

    HuffmanNode* root = buildHuffmanTree(frequencies);
    std::vector<uint8_t> output;
    output.reserve(originalSize);

    HuffmanNode* currentNode = root;
    size_t byteIdx = inputIdx;
    int bitIdx = 0;

    while(output.size() < originalSize) {
        if (byteIdx >= input.size()) {
            throw std::runtime_error("Unexpected end of compressed data");
        }
        
        bool bit = readBit(input, byteIdx, bitIdx);

        currentNode = bit ? currentNode->right : currentNode->left;

        if (currentNode->isLeaf()) {
            output.push_back(currentNode->byte);
            currentNode = root;
        }
    }

    return output;
}

void Compressor::writeBit(std::vector<uint8_t>& output, uint8_t& bitBuffer, int& bitCount, char bit) {
    if (bit == '1') {
        bitBuffer |= (1 << (7 - bitCount)); // 从高位向低位填充
    }
    bitCount++;
    if (bitCount == 8) {
        output.push_back(bitBuffer);
        bitBuffer = 0;
        bitCount = 0;
    }
}

bool Compressor::readBit(const std::vector<uint8_t>& input, size_t& byteIdx, int& bitIdx) {
    bool bit = (input[byteIdx] >> (7 - bitIdx)) & 1;
    bitIdx++;
    if (bitIdx == 8) {
        bitIdx = 0;
        byteIdx++;
    }
    return bit;
}


} // namespace Backup