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

// 文件元数据结构体
struct FileInfo {
    std::string relativePath;
    std::string absolutePath;
    FileType type;
    uint64_t size;
    mode_t permissions;
    time_t lastModified;
};

} // namespace Backup