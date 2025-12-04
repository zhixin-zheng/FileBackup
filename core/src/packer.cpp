#include "packer.h"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h> // for symlink, unlink

namespace fs = std::filesystem;

namespace Backup {

// Tar头部常量
const int BLOCK_SIZE = 512;
const char* MAGIC = "ustar"; 
const char* VERSION = "00";

// --- 工具辅助函数 ---

// 将数字格式化为八进制字符串(用于头部)
template<typename T>
void toOctal(char* dest, T value, size_t size) {
    std::string fmt = "%0" + std::to_string(size - 1) + "lo"; 
    snprintf(dest, size, fmt.c_str(), (unsigned long)value);
}

// 将八进制字符串解析为数字(用于提取)
uint64_t Packer::fromOctal(const char* ptr, size_t len) {
    std::string str(ptr, strnlen(ptr, len));
    if (str.empty()) return 0;
    try {
        return std::stoul(str, nullptr, 8);
    } catch (...) {
        return 0;
    }
}

// --- 打包实现 ---

bool Packer::pack(const std::vector<FileInfo>& files, const std::string& outputArchivePath) {
    std::ofstream archive(outputArchivePath, std::ios::binary | std::ios::trunc);
    if (!archive.is_open()) {
        std::cerr << "错误: 无法创建归档文件: " << outputArchivePath << std::endl;
        return false;
    }

    for (const auto& file : files) {
        TarHeader header;
        std::memset(&header, 0, sizeof(TarHeader)); 
        fillHeader(file, &header);

        archive.write(reinterpret_cast<const char*>(&header), sizeof(TarHeader));

        // 只有常规文件在Tar中有数据块。
        // 符号链接将目标存储在header.linkname中，目录没有数据。
        if (file.type == FileType::REGULAR) {
            if (!writeFileContent(file, archive)) {
                std::cerr << "警告: 无法写入内容 " << file.relativePath << std::endl;
            }
        }
    }

    // 写入归档结束标记(两个空的512字节块)
    char endBlocks[BLOCK_SIZE * 2];
    std::memset(endBlocks, 0, sizeof(endBlocks));
    archive.write(endBlocks, sizeof(endBlocks));

    archive.close();
    std::cout << "打包完成: " << outputArchivePath << std::endl;
    return true;
}

void Packer::fillHeader(const FileInfo& file, TarHeader* header) {
    // 1. 名称 & 前缀
    std::strncpy(header->name, file.relativePath.c_str(), sizeof(header->name) - 1);

    // 2. 权限 & 元数据
    toOctal(header->mode, file.permissions & 0777, sizeof(header->mode));
    toOctal(header->uid, 0, sizeof(header->uid)); // 占位符
    toOctal(header->gid, 0, sizeof(header->gid)); // 占位符
    toOctal(header->mtime, file.lastModified, sizeof(header->mtime));

    // 3. 类型 & 大小 & 链接名
    header->typeflag = '0'; // 默认常规文件
    uint64_t fileSize = 0;

    if (file.type == FileType::DIRECTORY) {
        header->typeflag = '5';
        // 目录大小为0
    } else if (file.type == FileType::SYMLINK) {
        header->typeflag = '2';
        // 在POSIX ustar中，符号链接大小为0，目标在linkname中
        // 假课FileInfo有一个'linkTarget'成员(基于提供的代码片段)
        // 如果没有，此行需要调整。
        // std::strncpy(header->linkname, file.linkTarget.c_str(), sizeof(header->linkname) - 1);
    } else {
        header->typeflag = '0';
        fileSize = file.size;
    }
    
    toOctal(header->size, fileSize, sizeof(header->size));

    // 4. 幻数
    std::strncpy(header->magic, MAGIC, sizeof(header->magic));
    std::strncpy(header->version, VERSION, sizeof(header->version));

    // 5. 校验和
    calculateChecksum(header);
}

void Packer::calculateChecksum(TarHeader* header) {
    std::memset(header->chksum, ' ', 8); // 将校验和字段视为空格进行计算
    unsigned long sum = 0;
    unsigned char* bytes = reinterpret_cast<unsigned char*>(header);
    for (size_t i = 0; i < sizeof(TarHeader); ++i) sum += bytes[i];
    snprintf(header->chksum, sizeof(header->chksum), "%06lo", sum);
}

bool Packer::writeFileContent(const FileInfo& file, std::ofstream& archive) {
    std::ifstream input(file.absolutePath, std::ios::binary);
    if (!input.is_open()) return false;

    archive << input.rdbuf();

    // 填充至512字节
    size_t padding = (BLOCK_SIZE - (file.size % BLOCK_SIZE)) % BLOCK_SIZE;
    if (padding > 0) {
        char pad[BLOCK_SIZE] = {0};
        archive.write(pad, padding);
    }
    return true;
}

// --- 提取实现 ---

bool Packer::unpack(const std::string& inputArchivePath, const std::string& outputDir) {
    std::ifstream archive(inputArchivePath, std::ios::binary);
    if (!archive.is_open()) {
        std::cerr << "错误: 无法打开归档文件: " << inputArchivePath << std::endl;
        return false;
    }

    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    TarHeader header;
    while (archive.read(reinterpret_cast<char*>(&header), sizeof(TarHeader))) {
        // 检查归档结束(空块)
        if (header.name[0] == '\0') {
            // 读取可能的第二个空块并退出
            break; 
        }

        if (!verifyChecksum(&header)) {
            std::cerr << "错误: 文件校验和不匹配 " << header.name << std::endl;
            return false;
        }

        std::string relPath = header.name;
        // 基本路径安全检查: 防止".."遍历
        if (relPath.find("..") != std::string::npos) {
            std::cerr << "警告: 跳过不安全的路径 " << relPath << std::endl;
            continue; 
        }

        fs::path destPath = fs::path(outputDir) / relPath;
        ensureParentDirExists(destPath.string());

        uint64_t fileSize = fromOctal(header.size, sizeof(header.size));
        char type = header.typeflag ? header.typeflag : '0';

        // 处理文件类型
        if (type == '5') { // 目录
            fs::create_directories(destPath);
        } 
        else if (type == '2') { // 符号链接
            std::string target = header.linkname;
            if (!target.empty()) {
                if (fs::exists(destPath)) fs::remove(destPath);
                // 创建符号链接
                if (symlink(target.c_str(), destPath.string().c_str()) != 0) {
                    std::cerr << "警告: 无法创建符号链接 " << destPath << std::endl;
                }
            }
        } 
        else { // 常规文件 ('0' 或 '\0')
            extractFileContent(archive, destPath.string(), fileSize);
        }

        // 恢复元数据(权限和时间)
        restoreMetadata(destPath.string(), &header);
    }

    std::cout << "提取完成到: " << outputDir << std::endl;
    return true;
}

bool Packer::verifyChecksum(const TarHeader* header) {
    TarHeader temp = *header;
    uint64_t storedSum = fromOctal(header->chksum, sizeof(header->chksum));
    calculateChecksum(&temp); // 基于当前数据重新计算
    uint64_t calcedSum = fromOctal(temp.chksum, sizeof(temp.chksum));
    return storedSum == calcedSum;
}

void Packer::extractFileContent(std::ifstream& archive, const std::string& destPath, uint64_t size) {
    std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "错误: 无法创建文件 " << destPath << std::endl;
        // 跳过归档中的数据以保持对齐
        archive.seekg((size + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE, std::ios::cur);
        return;
    }

    const size_t bufSize = 4096;
    char buffer[bufSize];
    uint64_t remaining = size;

    while (remaining > 0) {
        size_t toRead = (remaining < bufSize) ? remaining : bufSize;
        archive.read(buffer, toRead);
        out.write(buffer, toRead);
        remaining -= toRead;
    }

    // 跳过归档中的填充数据
    size_t padding = (BLOCK_SIZE - (size % BLOCK_SIZE)) % BLOCK_SIZE;
    if (padding > 0) {
        archive.ignore(padding);
    }
}

void Packer::ensureParentDirExists(const std::string& path) {
    fs::path p(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }
}

void Packer::restoreMetadata(const std::string& path, const TarHeader* header) {
    // 1. 权限
    mode_t mode = static_cast<mode_t>(fromOctal(header->mode, sizeof(header->mode)));
    chmod(path.c_str(), mode);

    // 2. 时间信息(访问时间和修改时间)
    time_t mtime = static_cast<time_t>(fromOctal(header->mtime, sizeof(header->mtime)));
    struct timeval times[2];
    times[0].tv_sec = mtime; // 访问时间(使用修改时间作为备选)
    times[0].tv_usec = 0;
    times[1].tv_sec = mtime; // 修改时间
    times[1].tv_usec = 0;
    
    // utimes通常作用于符号链接的目标，lutimes需要用于链接本身(可移植性较差)
    // 对于本项目，应用于文件/目录路径就足够了。
    utimes(path.c_str(), times);
}

} // namespace Backup