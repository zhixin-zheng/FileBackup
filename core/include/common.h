#pragma once
#include <string>
#include <sys/types.h>

namespace Backup{

enum class FileType {
    REGULAR, 
    DIRECTORY, 
    SYMLINK, 
    CHARACTER_DEVICE,
    BLOCK_DEVICE, 
    FIFO, 
    SOCKET, 
    UNKNOWN
};

// File metadata structure
struct FileInfo {
    std::string relativePath;   // 相对路径
    std::string absolutePath;   // 绝对路径
    FileType type;              // 文件类型
    uint64_t size;              // 文件大小
    mode_t permissions;         // 文件权限
    time_t lastModified;        // 最后修改时间
    uid_t UID;                  // 用户ID
    gid_t GID;                  // 组ID
    std::string userName;       // 用户名
    std::string groupName;      // 组名
    std::string linkTarget;     // 符号链接的目标路径（仅用于符号链接）
    unsigned int deviceMajor;   // 设备主号（仅用于字符设备和块设备）
    unsigned int deviceMinor;   // 设备副号（仅用于字符设备和块设备）
};

} // namespace Backup