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
     * @brief Traverse the directory at the given path and collect file information
     * @param path: The root path to start traversal
     * @return A vector of FileInfo structures representing files
    **/
    std::vector<FileInfo> traverse(const std::string & path);

private:
    /**
     * @brief Helper function to recursively traverse directories
     * @param currentPath: The current path being traversed
     * @param files: Reference to the vector collecting FileInfo structures
     */
    void traverseHelper(const std::string & currentPath, std::vector<FileInfo> & files);
};

} // namespace Backup