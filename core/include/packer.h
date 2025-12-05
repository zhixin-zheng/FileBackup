#pragma once

#include "common.h"
#include <vector>
#include <string>
#include <fstream>

namespace Backup {

// Tar头部常量
const int BLOCK_SIZE = 512;
const char* MAGIC = "ustar"; 
const char* VERSION = "00";

/**
 * @brief Packer类负责使用.tar格式(POSIX UStar)对文件进行归档/提取操作。
 */
class Packer {
public:
    Packer() = default;
    ~Packer() = default;

    /**
     * @brief 将给定的文件列表打包成一个.tar归档文件。
     * @param files: 由Traverser收集的文件元数据列表。
     * @param outputArchivePath: .tar文件将被创建的路径。
     * @return 如果打包成功返回true，否则返回false。
     */
    bool pack(const std::vector<FileInfo>& files, const std::string& outputArchivePath);

    /**
     * @brief 从.tar归档文件中提取文件到目标目录。
     * @param inputArchivePath: 现有.tar文件的路径。
     * @param outputDir: 提取文件的目标目录。
     * @return 如果提取成功返回true，否则返回false。
     */
    bool unpack(const std::string& inputArchivePath, const std::string& outputDir);

private:
    // POSIX UStar头部结构 (512字节)
    struct TarHeader {
        char name[100];     // 文件名
        char mode[8];       // 文件权限
        char uid[8];        // 用户ID
        char gid[8];        // 组ID
        char size[12];      // 文件大小
        char mtime[12];     // 修改时间
        char chksum[8];     // 校验和
        char typeflag;      // 文件类型
        char linkname[100]; // 链接名称(符号链接的目标)
        char magic[6];      // "ustar"
        char version[2];    // "00"
        char uname[32];     // 用户名
        char gname[32];     // 组名
        char devmajor[8];   // 设备主号
        char devminor[8];   // 设备副号
        char prefix[155];   // 文件名前缀
        char padding[12];   // 填充至512字节
    };

    // --- 打包辅助函数 ---
    void fillHeader(const FileInfo& file, TarHeader* header);
    void calculateChecksum(TarHeader* header);
    bool writeFileContent(const FileInfo& file, std::ofstream& archive);

    // --- 提取辅助函数 ---
    bool verifyChecksum(const TarHeader* header);
    void extractFileContent(std::ifstream& archive, const std::string& destPath, uint64_t size);
    void ensureParentDirExists(const std::string& path);
    void restoreMetadata(const std::string& path, const TarHeader* header);
    
    // --- 工具函数 ---
    uint64_t fromOctal(const char* ptr, size_t len);
};

} // namespace Backup