#include "compressor.h"
#include <queue>
#include <vector>
#include <iostream>

namespace Backup {

std::vector<uint8_t> Compressor::compress(const std::vector<uint8_t>& input, CompressionAlgorithm algo) {
    std::vector<uint8_t> output;
    output.push_back(static_cast<uint8_t>(algo)); // 在压缩数据前添加算法标识符

    // std::cout << "Using compression algorithm: " << (algo == CompressionAlgorithm::HUFFMAN ? "Huffman" : "LZSS") << std::endl;

    std::vector<uint8_t> compressedData;

    if (algo == CompressionAlgorithm::HUFFMAN) {
        compressedData = compressHuffman(input);
    } else if (algo == CompressionAlgorithm::LZSS) {
        compressedData = compressLZSS(input);
    } else if (algo == CompressionAlgorithm::JOINED) {
        compressedData = compressJoined(input);
    } else {
        throw std::runtime_error("Unsupported compression algorithm");
    }

    output.insert(output.end(), compressedData.begin(), compressedData.end());
    return output;
}

std::vector<uint8_t> Compressor::decompress(const std::vector<uint8_t>& input) {
    if (input.empty()) {
        return std::vector<uint8_t>();
    }

    CompressionAlgorithm algo = static_cast<CompressionAlgorithm>(input[0]);
    std::vector<uint8_t> compressedData(input.begin() + 1, input.end());

    // std::cout << "Using compression algorithm: " << (algo == CompressionAlgorithm::HUFFMAN ? "Huffman" : "LZSS") << std::endl;

    if (algo == CompressionAlgorithm::HUFFMAN) {
        return decompressHuffman(compressedData);
    } else if (algo == CompressionAlgorithm::LZSS) {
        return decompressLZSS(compressedData);
    } else if (algo == CompressionAlgorithm::JOINED) {
        return decompressJoined(compressedData);
    } else {
        throw std::runtime_error("Unsupported compression algorithm");
    }
}

// ==========================================
// Joined 压缩与解压缩实现
// ==========================================

std::vector<uint8_t> Compressor::compressJoined(const std::vector<uint8_t>& input) {
    return compressLZSS(compressHuffman(input));
}

std::vector<uint8_t> Compressor::decompressJoined(const std::vector<uint8_t>& input) {
    return decompressHuffman(decompressLZSS(input));
}

// ==========================================
// Huffman 压缩与解压缩实现
// ==========================================

std::vector<uint8_t> Compressor::compressHuffman(const std::vector<uint8_t>& input) {
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

std::vector<uint8_t> Compressor::decompressHuffman(const std::vector<uint8_t>& input) {
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

// ==========================================
// LZSS 压缩与解压缩实现
// ==========================================

// Compressor::Match Compressor::findLongestMatch(const std::vector<uint8_t> & input, size_t cursor) {
//     Match bestMatch = {0, 0};
//     size_t searchStart = (cursor > LZSS_WINDOW_SIZE) ? (cursor - LZSS_WINDOW_SIZE) : 0; // 不能写成 size_t searchStart = std::max(0, cursor - LZSS_WINDOW_SIZE);

//     for (size_t i = searchStart; i < cursor; i++) {
//         size_t len = 0;
//         while (len < LZSS_MAX_MATCH_LENGTH && cursor + len < input.size() && input[i + len] == input[cursor + len]) {
//             len++;
//         }

//         if (len > bestMatch.length) {
//             bestMatch.offset = cursor - i;
//             bestMatch.length = len;
//         }
//     }
//     return bestMatch;
// }

// std::vector<uint8_t> Compressor::compressLZSS(const std::vector<uint8_t>& input) {
//     std::vector<uint8_t> output;
//     output.reserve(input.size());
//     size_t cursor = 0;

//     while (cursor < input.size()) {
//         uint8_t flag = 0;
//         std::vector<uint8_t> buffer; // [Flag Byte] [Token 1] [Token 2] ... [Token 8]
//         for (int i = 0; i < 8 && cursor < input.size(); i++) {
//             Match match = findLongestMatch(input, cursor);
//             if (match.length >= LZSS_MIN_MATCH_LENGTH) {
//                 flag |= (1 << i);
//                 uint16_t off = static_cast<uint16_t>(match.offset);
//                 uint8_t len = static_cast<uint8_t>(match.length - LZSS_MIN_MATCH_LENGTH);
//                 buffer.push_back((off >> 4) & 0xFF);
//                 buffer.push_back(((off & 0x0F) << 4) | (len & 0x0F));
//                 cursor += match.length;
//             } else {
//                 buffer.push_back(input[cursor]);
//                 cursor += 1;
//             }
//         }
//         output.push_back(flag);
//         output.insert(output.end(), buffer.begin(), buffer.end());
//     }
//     return output;
// }

std::vector<uint8_t> Compressor::compressLZSS(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    output.reserve(input.size());

    // head: 存储某个 Hash 值“最近一次”出现的位置下标
    std::vector<int> head(HASH_SIZE, NIL);
    
    // prev: 链表结构，存储“上上一次”出现的位置（用于解决哈希冲突和回溯）
    std::vector<int> prev(input.size(), NIL);

    size_t cursor = 0;
    
    // 需要能够预读3个字节来计算哈希
    size_t limit = (input.size() > LZSS_MIN_MATCH_LENGTH) ? input.size() - LZSS_MIN_MATCH_LENGTH : 0;
    
    while (cursor < input.size()) {
        uint8_t flag = 0;
        std::vector<uint8_t> buffer;
        
        int bufferCount = 0;
        int startCursorOfGroup = cursor; // 一组数据的起始位置

        for (int i = 0; i < 8 && cursor < input.size(); i++) {
            
            Match bestMatch = {0, 0};

            // 只有剩余数据足够 3 字节，才尝试查找匹配
            if (cursor < limit) {
                uint16_t h = hash_func(input[cursor], input[cursor+1], input[cursor+2]);
                
                int matchCursor = head[h];
                // 更新哈希表和链表
                prev[cursor] = head[h];
                head[h] = cursor;

                // 沿着链表向回查找 （限制查找次数）
                int chainLen = 0;
                int maxChain = 128; // 性能与压缩率的权衡参数：只看最近的128个匹配
                
                while (matchCursor != NIL && chainLen++ < maxChain) {
                    // 距离超过窗口大小，停止查找（因为是向回找，后面的更远）
                    if (cursor - matchCursor > LZSS_WINDOW_SIZE) {
                        break;
                    }

                    if (input[matchCursor] == input[cursor]) {
                        size_t len = 0;
                        while (len < LZSS_MAX_MATCH_LENGTH && cursor + len < input.size() && input[matchCursor + len] == input[cursor + len]) {
                            len++;
                        }

                        if (len > bestMatch.length) {
                            bestMatch.length = len;
                            bestMatch.offset = cursor - matchCursor;
                            if (len >= LZSS_MAX_MATCH_LENGTH) break; // 已经找到最大匹配，停止查找
                        }
                    }

                    matchCursor = prev[matchCursor];
                }
            }
            // 写入逻辑
            if (bestMatch.length >= LZSS_MIN_MATCH_LENGTH) {
                flag |= (1 << i);
                uint16_t off = static_cast<uint16_t>(bestMatch.offset);
                uint8_t len = static_cast<uint8_t>(bestMatch.length - LZSS_MIN_MATCH_LENGTH);
                buffer.push_back((off >> 4) & 0xFF);
                buffer.push_back(((off & 0x0F) << 4) | (len & 0x0F));
                cursor += bestMatch.length;
            } else {
                buffer.push_back(input[cursor]);
                cursor += 1; 
            }
        }
        
        output.push_back(flag);
        output.insert(output.end(), buffer.begin(), buffer.end());
    }
    
    return output;
}

std::vector<uint8_t> Compressor::decompressLZSS(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    output.reserve(input.size() * 2);

    size_t cursor = 0;
    while (cursor < input.size()) {
        uint8_t flag = input[cursor++];
        for (int i = 0; i < 8; i++) {
            if (cursor >= input.size()) break; // 修复：防止读取越界，处理最后一个不完整的块

            if (flag & (1 << i)) {
                if (cursor + 2 > input.size()) {
                    throw std::runtime_error("LZSS decompression error: unexpected end of data");
                }

                uint16_t off = (static_cast<uint16_t>(input[cursor]) << 4) | (input[cursor + 1] >> 4);
                uint8_t len = (input[cursor + 1] & 0x0F) + LZSS_MIN_MATCH_LENGTH;
                
                if (off > output.size() || off == 0) {
                     throw std::runtime_error("LZSS decompression error: invalid offset");
                }

                size_t startPos = output.size() - off;

                for (int j = 0; j < len; j++) {
                    output.push_back(output[startPos + j]);
                }
                cursor += 2;
            } else {
                output.push_back(input[cursor]);
                cursor += 1;
            }
        }
    }
    return output;
}

} // namespace Backup