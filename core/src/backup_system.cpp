#include "backup_system.h"
#include "traverser.h"
#include "packer.h"
#include "compressor.h"
#include "encryptor.h"
#include "common.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <regex>

namespace Backup {

BackupSystem::BackupSystem() 
    : m_compressionAlgo(static_cast<int>(CompressionAlgorithm::LZSS)), 
      m_isEncrypted(false) {
      m_filter.enabled = false; // 默认不启用过滤器
}

BackupSystem::~BackupSystem() {}

void BackupSystem::setCompressionAlgorithm(int algo) {
    m_compressionAlgo = algo;
}

void BackupSystem::setPassword(const std::string& password) {
    m_password = password;
    m_isEncrypted = !password.empty();
}

void BackupSystem::setFilter(const Filter& filter) {
    m_filter = filter;
    m_filter.enabled = true;
}

// ---------------------------------------------------------
// 核心功能 1: 数据备份
// ---------------------------------------------------------
bool BackupSystem::backup(const std::string& srcDir, const std::string& dstFile) {
    std::cout << "[Backup] Starting backup: " << srcDir << " -> " << dstFile << std::endl;
    
    try {
        // 1. 遍历文件 (Traverse)
        Traverser traverser;
        std::vector<FileInfo> files = traverser.traverse(srcDir);
        if (files.empty()) {
            std::cerr << "[Backup] Warning: Source directory is empty or invalid." << std::endl;
            return false;
        }
        std::cout << "[Backup] Scanned " << files.size() << " files." << std::endl;


        if (m_filter.enabled) {
            files = applyFilter(files);
            std::cout << "[Backup] After filtering, " << files.size() << " files remain." << std::endl;
            if (files.empty()) {
                std::cerr << "[Backup] Warning: No files match the filter criteria." << std::endl;
                return false;
            }
        }

        // 2. 打包 (Pack) -> 这里需要 Packer 支持输出到内存流
        // 由于目前的 Packer::pack 是直接写文件的，我们需要稍微变通一下。
        // 为了不修改 Packer 接口，我们先打包到一个临时文件，然后读入内存。
        // *更好的做法* 是修改 Packer 使其支持 std::ostream，但为了兼容你现有的代码，我们用临时文件法。
        
        std::string tempTarFile = dstFile + ".tmp.tar";
        Packer packer;
        if (!packer.pack(files, tempTarFile)) {
            std::cerr << "[Backup] Packing failed." << std::endl;
            return false;
        }

        // 读取打包后的数据到内存
        std::vector<uint8_t> data = readFile(tempTarFile);
        std::filesystem::remove(tempTarFile); // 删除临时文件
        std::cout << "[Backup] Packed size: " << data.size() << " bytes." << std::endl;

        // 3. 压缩 (Compress)
        Compressor compressor;
        std::vector<uint8_t> compressedData = compressor.compress(data, static_cast<CompressionAlgorithm>(m_compressionAlgo));
        std::cout << "[Backup] Compressed size: " << compressedData.size() << " bytes." << std::endl;
        
        // 释放原始数据内存
        std::vector<uint8_t>().swap(data); 

        // 4. 加密 (Encrypt) - 如果设置了密码
        std::vector<uint8_t> finalData;
        if (m_isEncrypted) {
            Encryptor encryptor;
            encryptor.init(m_password);
            finalData = encryptor.encrypt(compressedData);
            std::cout << "[Backup] Encrypted size: " << finalData.size() << " bytes." << std::endl;
        } else {
            finalData = std::move(compressedData);
        }

        // 5. 写入目标文件
        if (!writeFile(dstFile, finalData)) {
            std::cerr << "[Backup] Failed to write output file." << std::endl;
            return false;
        }

        std::cout << "[Backup] Success!" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[Backup] Error: " << e.what() << std::endl;
        return false;
    }
}

std::string escapeRegex(const std::string& str) {
    // 需要转义的字符: . ^ $ | ( ) [ ] { } * + ? \
    // static const std::regex specialChars{R"([-[\]{}()*+?.,\^$|#\s])"};
    static const std::regex specialChars{R"([\[\]{}()*+?.,\^$|#\s-])"};
    return std::regex_replace(str, specialChars, R"(\$&)");
}

std::vector<FileInfo> BackupSystem::applyFilter(const std::vector<FileInfo>& files) {
    std::vector<FileInfo> results;
    std::regex namePattern;
    bool useRegex = false;

    if (!m_filter.nameKeywords.empty()) {
        std::string combinedPattern = ".*(";
        for (size_t i = 0; i < m_filter.nameKeywords.size(); ++i) {
            if (i > 0) combinedPattern += "|";
            combinedPattern += escapeRegex(m_filter.nameKeywords[i]);
        }
        combinedPattern += ").*"; // 匹配文件名中包含任意关键词
        
        try {
            namePattern = std::regex(combinedPattern);
            useRegex = true;
            // std::cout << "[Filter] Generated Regex from Keywords: " << combinedPattern << std::endl;
        } catch (...) {
            std::cerr << "[Filter] Error generating regex from keywords." << std::endl;
        }
    } 
    else if (!m_filter.nameRegex.empty()) {
        namePattern = std::regex(m_filter.nameRegex);
        useRegex = true;
    }
    for (const auto& file : files) {
        if (file.type == FileType::DIRECTORY) {
            results.push_back(file);
            continue;
        }

        if (m_filter.minSize > 0 && file.size < m_filter.minSize) continue;
        if (m_filter.maxSize > 0 && file.size > m_filter.maxSize) continue;

        if (m_filter.startTime > 0 && file.lastModified < m_filter.startTime) continue;
        if (m_filter.endTime > 0 && file.lastModified > m_filter.endTime) continue;

        if (!m_filter.userName.empty() && file.userName != m_filter.userName) continue;

        if (!m_filter.suffixes.empty()) {
            bool suffixMatch = false;
            for (const auto& suffix : m_filter.suffixes) {
                if (file.relativePath.size() >= suffix.size() && file.relativePath.substr(file.relativePath.size() - suffix.size(), suffix.size()) == suffix) {
                    suffixMatch = true;
                    break;
                }
            }
            if (!suffixMatch) continue;
        }

        if (useRegex && !std::regex_search(file.relativePath, namePattern)) continue;

        // 不包含相对路径的写法
        // if (!m_filter.nameRegex.empty()) {
        //     std::filesystem::path p(file.relativePath);
        //     std::string fname = p.filename().string();
        //     if (!std::regex_match(fname, namePattern)) continue;
        // }

        results.push_back(file);
    }
    return results;
}

// ---------------------------------------------------------
// 核心功能 2: 数据还原
// ---------------------------------------------------------
bool BackupSystem::restore(const std::string& srcFile, const std::string& dstDir) {
    std::cout << "[Restore] Starting restore: " << srcFile << " -> " << dstDir << std::endl;

    try {
        // 1. 读取备份文件
        std::vector<uint8_t> data = readFile(srcFile);
        if (data.empty()) {
            std::cerr << "[Restore] Error: Backup file is empty or cannot be read." << std::endl;
            return false;
        }

        // 2. 解密 (Decrypt) - 如果设置了密码
        if (m_isEncrypted) {
            std::cout << "[Restore] Decrypting..." << std::endl;
            Encryptor encryptor;
            encryptor.init(m_password);
            try {
                data = encryptor.decrypt(data);
            } catch (const std::exception& e) {
                std::cerr << "[Restore] Decryption failed. Wrong password? Error: " << e.what() << std::endl;
                return false;
            }
        }

        // 3. 解压 (Decompress)
        std::cout << "[Restore] Decompressing..." << std::endl;
        Compressor compressor;
        std::vector<uint8_t> tarData;
        try {
            tarData = compressor.decompress(data);
        } catch (const std::exception& e) {
            std::cerr << "[Restore] Decompression failed. Data corrupted? Error: " << e.what() << std::endl;
            return false;
        }
        
        // 释放压缩数据内存
        std::vector<uint8_t>().swap(data);

        // 4. 解包 (Unpack)
        //同样，Packer::unpack 需要读取文件。我们先把数据写入临时文件。
        std::string tempTarFile = srcFile + ".tmp.tar";
        if (!writeFile(tempTarFile, tarData)) {
            return false;
        }

        Packer packer;
        std::cout << "[Restore] Unpacking to " << dstDir << "..." << std::endl;
        bool result = packer.unpack(tempTarFile, dstDir);
        
        std::filesystem::remove(tempTarFile); // 删除临时文件

        if (result) {
            std::cout << "[Restore] Success!" << std::endl;
        } else {
            std::cerr << "[Restore] Unpacking failed." << std::endl;
        }
        return result;

    } catch (const std::exception& e) {
        std::cerr << "[Restore] Error: " << e.what() << std::endl;
        return false;
    }
}

// ---------------------------------------------------------
// 核心功能 3: 备份验证
// ---------------------------------------------------------
bool BackupSystem::verify(const std::string& backupFile) {
    std::cout << "[Verify] Verifying backup: " << backupFile << std::endl;
    // 验证逻辑：尝试解密 -> 尝试解压 -> 检查 Tar 头是否合法
    // 如果全过程无异常抛出，则认为文件完整性基本没问题。
    // 这比单纯比较哈希更强，因为它验证了逻辑结构。
    
    // 借用 restore 的前半部分逻辑，但不进行最后的 unpack 到磁盘
    try {
        std::vector<uint8_t> data = readFile(backupFile);
        if (data.empty()) return false;

        if (m_isEncrypted) {
            Encryptor encryptor;
            encryptor.init(m_password);
            data = encryptor.decrypt(data); // 如果密码错或数据坏，这里会抛出异常
        }

        Compressor compressor;
        std::vector<uint8_t> tarData = compressor.decompress(data); // 如果压缩数据坏，这里会抛出异常

        // 简单检查 Tar 数据是否合法（至少要有 512 字节且包含 magic）
        if (tarData.size() < 512) return false;
        
        // 检查 ustar 标记
        // magic 字段在偏移 257 处，长度 6，内容应该是 "ustar"
        if (tarData.size() >= 263) {
            std::string magic(reinterpret_cast<char*>(&tarData[257]), 5);
            if (magic != "ustar") {
                std::cerr << "[Verify] Invalid Tar magic signature." << std::endl;
                return false;
            }
        }

        std::cout << "[Verify] Backup is valid." << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[Verify] Verification failed: " << e.what() << std::endl;
        return false;
    }
}

// --- 辅助函数 ---

std::vector<uint8_t> BackupSystem::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Read error: " + path);
    }
    return buffer;
}

bool BackupSystem::writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

} // namespace Backup