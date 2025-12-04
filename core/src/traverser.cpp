#include "traverser.h"
#include <string>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>

namespace Backup {

std::string joinPaths(const std::string & base, const std::string & addition) {
    if (base.empty()) return addition;
    if (addition.empty()) return base;
    if (base.back() == '/') {
        return base + addition;
    } else {
        return base + "/" + addition;
    }
}

std::vector<Backup::FileInfo> Backup::Traverser::traverse(const std::string & path) {
    std::vector<FileInfo> files;

    // 检查路径是否存在
    struct stat pathStat;
    if (lstat(path.c_str(), &pathStat) == -1) {
        throw std::runtime_error("Cannot open directory: " + path);
    }

    traverseHelper(path, path, files);
    return files;
}

void Traverser::traverseHelper(const std::string & currentDir, const std::string & rootDir, std::vector<FileInfo> & files) {
    DIR *dir = opendir(currentDir.c_str());
    if (!dir) {
        throw std::runtime_error("Cannot open directory: " + currentDir);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string entryName = entry->d_name;
        if (entryName == "." || entryName == "..") {
            continue;
        }
        if (entryName == ".DS_Store") {
            continue; // 跳过 .DS_Store 文件
        }

        std::string fullPath = joinPaths(currentDir, entryName);
        
        Backup::FileInfo fileInfo = getFileInfo(fullPath, rootDir);
        files.push_back(fileInfo);

        if (fileInfo.type == FileType::DIRECTORY) {
            traverseHelper(fullPath, rootDir, files);
        }
    }

    closedir(dir);
}

FileInfo Traverser::getFileInfo(const std::string & fullPath, const std::string & rootDir) {
    FileInfo info;
    info.absolutePath = fullPath;

    if (fullPath.find(rootDir) == 0) {
        info.relativePath = fullPath.substr(rootDir.length()); // 移除 rootDir 前缀 (fullPath = rootDir + relativePath)
        if (!info.relativePath.empty() && info.relativePath[0] == '/') {
            info.relativePath = info.relativePath.substr(1); // 移除开头的 "/"
        }
    } else {
        info.relativePath = fullPath; // Fallback to absolute path if rootDir is not a prefix
    }

    struct stat fileStat;
    if (lstat(fullPath.c_str(), &fileStat) == -1) {
        throw std::runtime_error("Cannot stat file: " + fullPath);
    }
    info.size = fileStat.st_size;
    info.permissions = fileStat.st_mode;
    info.lastModified = fileStat.st_mtime;
    info.UID = fileStat.st_uid;
    info.GID = fileStat.st_gid;

    if (S_ISREG(fileStat.st_mode)) {
        info.type = FileType::REGULAR;
    } else if (S_ISDIR(fileStat.st_mode)) {
        info.type = FileType::DIRECTORY;
    } else if (S_ISLNK(fileStat.st_mode)) {
        info.type = FileType::SYMLINK;
    } else if (S_ISFIFO(fileStat.st_mode)) {
        info.type = FileType::FIFO;
    } else if (S_ISCHR(fileStat.st_mode)) {
        info.type = FileType::CHARACTER_DEVICE;
    } else if (S_ISBLK(fileStat.st_mode)) {
        info.type = FileType::BLOCK_DEVICE;
    } else if (S_ISSOCK(fileStat.st_mode)) {
        info.type = FileType::SOCKET;
    } else {
        info.type = FileType::UNKNOWN;
    }

    return info;
}
} // namespace Backup