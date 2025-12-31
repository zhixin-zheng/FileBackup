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
    HuffmanNode(uint64_t f, HuffmanNode* l, HuffmanNode* r) : byte(0), freq(f), left(l), right(r) {}

    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

enum class CompressionAlgorithm {
    HUFFMAN = 0,
    LZSS = 1,
    JOINED = 2
};

class Compressor {
public:
    Compressor() = default;
    ~Compressor() = default;

    /**
     * @brief 压缩数据
     * @param input: 输入数据缓冲区
     * @param algo: 使用的压缩算法（可选 Huffman 或 LZSS，默认为 LZSS）
     * @return 压缩后的数据缓冲区
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input, CompressionAlgorithm algo = CompressionAlgorithm::LZSS);

    /**
     * @brief 解压缩数据
     * @param input: 压缩后的数据缓冲区
     * @return 解压缩后的数据缓冲区
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input);

private:

    // 联合压缩实现
    std::vector<uint8_t> compressJoined(const std::vector<uint8_t>& input);
    // 联合解压缩实现
    std::vector<uint8_t> decompressJoined(const std::vector<uint8_t>& input);

    // Huffman 压缩实现
    std::vector<uint8_t> compressHuffman(const std::vector<uint8_t>& input);
    // Huffman 解压缩实现
    std::vector<uint8_t> decompressHuffman(const std::vector<uint8_t>& input);

    /**
     * @brief 构建哈夫曼树
     * @param frequencies: 字节频率表
     * @return 哈夫曼树的根节点
     */
    HuffmanNode* buildHuffmanTree(const std::vector<uint64_t>& frequencies);

    /**
     * @brief 生成哈夫曼编码表
     * @param node: 当前节点
     * @param prefix: 当前编码前缀
     * @param codes: 哈夫曼编码表
     */
    void generateCodes(HuffmanNode* node, const std::string& prefix, std::unordered_map<uint8_t, std::string>& codes);

    /**
     * @brief 删除哈夫曼树
     * @param node: 当前节点
     */
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

    static const int LZSS_WINDOW_SIZE = 32767;                            // LZSS 窗口大小
    static const int LZSS_MIN_MATCH_LENGTH = 4;                          // LZSS 最小匹配长度
    static const int LZSS_MAX_MATCH_LENGTH = 255; // LZSS 最大匹配长度
    // static const int LZSS_WINDOW_SIZE = 4095;                            // LZSS 窗口大小
    // static const int LZSS_MIN_MATCH_LENGTH = 3;                          // LZSS 最小匹配长度
    // static const int LZSS_MAX_MATCH_LENGTH = 15 + LZSS_MIN_MATCH_LENGTH; // LZSS 最大匹配长度

    static const int HASH_BITS = 15;                                     // 哈希表位数
    static const int HASH_SIZE = 1 << HASH_BITS;                         // 哈希表大小 32K
    static constexpr int NIL = -1;                                       // 空指针标记

    // LZSS 压缩实现
    std::vector<uint8_t> compressLZSS(const std::vector<uint8_t>& input);
    // LZSS 解压缩实现
    std::vector<uint8_t> decompressLZSS(const std::vector<uint8_t>& input);

    struct Match {
        size_t offset;
        size_t length;
    };
    Match findLongestMatch(const std::vector<uint8_t> & input, size_t cursor);

    uint16_t hash_func(uint8_t b1, uint8_t b2, uint8_t b3) { return ((static_cast<uint32_t>(b1) << 10) ^ (static_cast<uint32_t>(b2) << 5) ^ b3) & (HASH_SIZE - 1);  }
};

}  // namespace Backup