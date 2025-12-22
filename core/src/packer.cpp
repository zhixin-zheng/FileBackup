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

namespace Backup {

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
        std::cerr << "error: cannot create archive file " << outputArchivePath << std::endl;
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
                std::cerr << "warning: cannot write content for " << file.relativePath << std::endl;
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
    // 1. 名称 & 前缀 (Name & Prefix) - 路径拆分逻辑
    std::string path = file.relativePath;
    size_t pathLen = path.length();

    if (pathLen <= 100) {
        // Case A: 短路径，直接存入 name
        std::strncpy(header->name, path.c_str(), sizeof(header->name)); 
    } else {
        // Case B: 长路径，需要拆分为 prefix 和 name
        // 限制：name <= 100, prefix <= 155
        // 必须在 '/' 处切分，'/' 本身不存储
        
        bool splitFound = false;
        // 寻找合适的切分点。切分点 index 是 '/' 的位置。
        // name = path.substr(index + 1) -> 长度必须 <= 100
        // prefix = path.substr(0, index) -> 长度必须 <= 155
        
        // 为了使 name <= 100，切分点 index 至少要在 pathLen - 101 的位置
        size_t minSplitIndex = (pathLen > 101) ? (pathLen - 101) : 0;
        
        // 遍历寻找最合适的 '/'（通常找最右边的，让 name 尽可能短，prefix 利用率高）
        for (size_t i = minSplitIndex; i < pathLen && i <= 155; ++i) {
            if (path[i] == '/') {
                std::string prefixStr = path.substr(0, i);
                std::string nameStr = path.substr(i + 1);
                
                // 再次确认长度（逻辑上应该满足，但做个双重检查）
                if (prefixStr.length() <= 155 && nameStr.length() <= 100) {
                    std::strncpy(header->prefix, prefixStr.c_str(), sizeof(header->prefix));
                    std::strncpy(header->name, nameStr.c_str(), sizeof(header->name));
                    splitFound = true;
                    // 我们可以在找到第一个合法点时停止，或者继续找更优的。
                    // 这里找到满足条件的最靠前的切分点即可，或者上面的循环如果是从右向左找会更好。
                    // 现在的循环是从左向右找满足 name<=100 的点，所以找到的第一个 i 会让 name 接近 100。
                }
            }
        }

        if (!splitFound) {
            std::cerr << "Warning: Path too long to store in Tar header (truncated): " << path << std::endl;
            std::strncpy(header->name, path.c_str(), sizeof(header->name));
        }
    }

    // 2. 权限 & 元数据
    toOctal(header->mode, file.permissions & 0777, sizeof(header->mode));
    // 假设 FileInfo 已包含这些扩展字段
    toOctal(header->uid, file.UID, sizeof(header->uid));
    toOctal(header->gid, file.GID, sizeof(header->gid));
    toOctal(header->mtime, file.lastModified, sizeof(header->mtime));

    // 3. 类型 & 大小 & 链接名
    header->typeflag = '0'; // 默认常规文件
    uint64_t fileSize = 0;

    if (file.type == FileType::DIRECTORY) {
        header->typeflag = '5';
        // 目录大小为0
    } else if (file.type == FileType::SYMLINK) {
        header->typeflag = '2';
        // 符号链接大小为0，目标在 linkname 中
        // 假设 FileInfo 已经读取了 linkTarget
        std::strncpy(header->linkname, file.linkTarget.c_str(), sizeof(header->linkname) - 1);
    } else {
        header->typeflag = '0';
        fileSize = file.size;
    }
    
    toOctal(header->size, fileSize, sizeof(header->size));

    // 4. 幻数
    std::strncpy(header->magic, MAGIC, sizeof(header->magic));
    std::strncpy(header->version, VERSION, sizeof(header->version));

    // 5. 用户名和组名
    std::strncpy(header->uname, file.userName.c_str(), sizeof(header->uname));
    std::strncpy(header->gname, file.groupName.c_str(), sizeof(header->gname));

    // 6. 设备号（仅用于字符设备和块设备）
    if (file.type == FileType::CHARACTER_DEVICE || file.type == FileType::BLOCK_DEVICE) {
        header->typeflag = (file.type == FileType::CHARACTER_DEVICE) ? '3' : '4';
        toOctal(header->devmajor, file.deviceMajor, sizeof(header->devmajor));
        toOctal(header->devminor, file.deviceMinor, sizeof(header->devminor));
    } else {
        // 非设备文件填空字符或0
        // 标准通常留空，toOctal 填的是数字字符串，这里手动置零或空格均可，通常保持为0
    }

    // 7. 校验和 (必须是最后计算，因为它依赖于 header 中其他所有字段的值)
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

    if (!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
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

        std::filesystem::path destPath = std::filesystem::path(outputDir) / relPath;
        ensureParentDirExists(destPath.string());

        uint64_t fileSize = fromOctal(header.size, sizeof(header.size));
        char type = header.typeflag ? header.typeflag : '0';

        // 处理文件类型
        if (type == '5') { // 目录
            std::filesystem::create_directories(destPath);
        } 
        else if (type == '2') { // 符号链接
            std::string target = header.linkname;
            if (!target.empty()) {
                if (std::filesystem::exists(destPath)) std::filesystem::remove(destPath);
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
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
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