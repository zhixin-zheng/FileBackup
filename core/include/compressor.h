#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Backup {

struct HuffmanNode {
    uint8_t byte;
    uint64_t freq;
    HuffmanNode* left;
    HuffmanNode* right;

    HuffmanNode(uint8_t b, uint64_t f) : byte(b), freq(f), left(nullptr), right(nullptr) {}
    HuffmanNode(HuffmanNode* l, HuffmanNode* r) : byte(0), freq(l->freq + r->freq), left(l), right(r) {}

    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

class Compressor {
public:
    Compressor() = default;
    ~Compressor() = default;

    /**
     * @brief 压缩数据
     * @param input: 输入数据缓冲区
     * @return 压缩后的数据缓冲区
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input);

    /**
     * @brief 解压缩数据
     * @param input: 压缩后的数据缓冲区
     * @return 解压缩后的数据缓冲区
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input);

private:
    HuffmanNode* buildHuffmanTree(const std::vector<uint64_t>& frequencies);
    void generateCodes(HuffmanNode* node, const std::string& prefix, std::unordered_map<uint8_t, std::string>& codes);
    void deleteTree(HuffmanNode* node);

    /**
     * @brief 写入单个位到输出缓冲区
     * @param output: 输出数据缓冲区
     * @param bitBuffer: 当前的字节缓冲区
     * @param bitCount: 当前缓冲区中的位数
     * @param bit: 要写入的位 ('0' 或 '1')
     */
    void writeBit(std::vector<uint8_t>& output, uint8_t& bitBuffer, int& bitCount, char bit);

    /**
     * @brief 从输入缓冲区读取单个位
     * @param input: 输入数据缓冲区
     * @param byteIndex: 当前字节索引
     * @param bitIndex: 当前位索引
     * @return 读取的位
     */
    bool readBit(const std::vector<uint8_t>& input, size_t& byteIndex, int& bitIndex);
};

}  // namespace Backup