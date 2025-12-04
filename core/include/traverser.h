#pragma once

#include "common.h"
#include <vector>
#include <string>

namespace Backup{

class Traverser {
public:
    Traverser() = default;
    ~Traverser() = default;

    /** 
     * @brief 遍历给定路径下的所有文件和目录
     * @param path: 开始遍历的根路径
     * @return 包含文件信息 FileInfo 的 vector
    **/
    std::vector<FileInfo> traverse(const std::string & path);

private:
    /**
     * @brief 递归遍历目录的辅助函数
     * @param currentDir: 当前被遍历的目录
     * @param rootDir: 遍历开始的根目录
     * @param files: 用于存储结果的文件信息列表
     */
    void traverseHelper(const std::string & currentDir, const std::string & rootDir, std::vector<FileInfo> & files);
    
    /**
     * @brief 获取给定路径的文件信息
     * @param fullPath: The full path of the file
     * @param rootDir: The root directory to calculate relative paths
     * @return A FileInfo structure containing metadata about the file
     */
    FileInfo getFileInfo(const std::string & fullPath, const std::string & rootDir);
};

} // namespace Backup