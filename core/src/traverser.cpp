#include "traverser.h"
#include <string>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>

namespace Backup {

std::vector<Backup::FileInfo> Backup::Traverser::traverse(const std::string & path) {
    std::vector<FileInfo> files;

    // Check if the path exists
    struct stat pathStat;
    if (lstat(path.c_str(), &pathStat) == -1) {
        throw std::runtime_error("Cannot access path: " + path);
    }

    traverseHelper(path, files);
    return files;
}

} //namespace Backup