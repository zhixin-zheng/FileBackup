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

    try {
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
        deleteTree(root);
        return output;
    
    }
    catch (...) {
        deleteTree(root);
        throw;
    }
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

    try {
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
        deleteTree(root);
        return output;
    
    }
    catch (...) {
        deleteTree(root);
        throw;
    }
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

HuffmanNode* Compressor::buildHuffmanTree(const std::vector<uint64_t>& frequencies) {
    auto cmp = [](HuffmanNode* left, HuffmanNode* right) { return left->freq > right->freq; };
    std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, decltype(cmp)> minHeap(cmp);

    for (uint16_t i = 0; i < frequencies.size(); i++) {
        if (frequencies[i] > 0) {
            minHeap.push(new HuffmanNode(static_cast<uint8_t>(i), frequencies[i]));
        }
    }

    if (minHeap.size() == 1) {
        HuffmanNode* onlyNode = minHeap.top(); minHeap.pop();
        HuffmanNode* root = new HuffmanNode(onlyNode->freq, onlyNode, nullptr);
        return root;
    }

    while (minHeap.size() > 1) {
        HuffmanNode* left = minHeap.top(); minHeap.pop();
        HuffmanNode* right = minHeap.top(); minHeap.pop();
        HuffmanNode* parent = new HuffmanNode(left->freq + right->freq, left, right);
        minHeap.push(parent);
    }

    return minHeap.empty() ? nullptr : minHeap.top();
}

void Compressor::generateCodes(HuffmanNode* node, const std::string& prefix, std::unordered_map<uint8_t, std::string>& codes) {
    if (!node) return;
    if (node->isLeaf()) {
        codes[node->byte] = prefix;
        return;
    }
    if (node->left) {
        generateCodes(node->left, prefix + "0", codes);
    }
    if (node->right) {
        generateCodes(node->right, prefix + "1", codes);
    }
}

void Compressor::deleteTree(HuffmanNode* node) {
    if (!node) return;
    deleteTree(node->left);
    deleteTree(node->right);
    delete node;
}

} // namespace Backup