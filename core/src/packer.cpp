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

// Constants for Tar Header
const int BLOCK_SIZE = 512;
const char* MAGIC = "ustar"; 
const char* VERSION = "00";

// --- Utility Helpers ---

// Format number to octal string (for Header)
template<typename T>
void toOctal(char* dest, T value, size_t size) {
    std::string fmt = "%0" + std::to_string(size - 1) + "lo"; 
    snprintf(dest, size, fmt.c_str(), (unsigned long)value);
}

// Parse octal string to number (for Unpack)
uint64_t Packer::fromOctal(const char* ptr, size_t len) {
    std::string str(ptr, strnlen(ptr, len));
    if (str.empty()) return 0;
    try {
        return std::stoul(str, nullptr, 8);
    } catch (...) {
        return 0;
    }
}

// --- Pack Implementation ---

bool Packer::pack(const std::vector<FileInfo>& files, const std::string& outputArchivePath) {
    std::ofstream archive(outputArchivePath, std::ios::binary | std::ios::trunc);
    if (!archive.is_open()) {
        std::cerr << "Error: Could not create archive file: " << outputArchivePath << std::endl;
        return false;
    }

    for (const auto& file : files) {
        TarHeader header;
        std::memset(&header, 0, sizeof(TarHeader)); 
        fillHeader(file, &header);

        archive.write(reinterpret_cast<const char*>(&header), sizeof(TarHeader));

        // Only REGULAR files have data blocks in Tar. 
        // Symlinks store target in header.linkname, Directories have no data.
        if (file.type == FileType::REGULAR) {
            if (!writeFileContent(file, archive)) {
                std::cerr << "Warning: Failed to write content for " << file.relativePath << std::endl;
            }
        }
    }

    // Write End of Archive (Two empty 512-byte blocks)
    char endBlocks[BLOCK_SIZE * 2];
    std::memset(endBlocks, 0, sizeof(endBlocks));
    archive.write(endBlocks, sizeof(endBlocks));

    archive.close();
    std::cout << "Packing completed: " << outputArchivePath << std::endl;
    return true;
}

void Packer::fillHeader(const FileInfo& file, TarHeader* header) {
    // 1. Name & Prefix
    std::strncpy(header->name, file.relativePath.c_str(), sizeof(header->name) - 1);

    // 2. Mode & Metadata
    toOctal(header->mode, file.permissions & 0777, sizeof(header->mode));
    toOctal(header->uid, 0, sizeof(header->uid)); // Placeholder
    toOctal(header->gid, 0, sizeof(header->gid)); // Placeholder
    toOctal(header->mtime, file.lastModified, sizeof(header->mtime));

    // 3. Type & Size & Linkname
    header->typeflag = '0'; // Default regular
    uint64_t fileSize = 0;

    if (file.type == FileType::DIRECTORY) {
        header->typeflag = '5';
        // Directories have size 0
    } else if (file.type == FileType::SYMLINK) {
        header->typeflag = '2';
        // In POSIX ustar, symlink size is 0, target is in linkname
        // Assuming FileInfo has a member 'linkTarget' based on your provided snippet
        // If not, this line needs adjustment.
        // std::strncpy(header->linkname, file.linkTarget.c_str(), sizeof(header->linkname) - 1);
    } else {
        header->typeflag = '0';
        fileSize = file.size;
    }
    
    toOctal(header->size, fileSize, sizeof(header->size));

    // 4. Magic
    std::strncpy(header->magic, MAGIC, sizeof(header->magic));
    std::strncpy(header->version, VERSION, sizeof(header->version));

    // 5. Checksum
    calculateChecksum(header);
}

void Packer::calculateChecksum(TarHeader* header) {
    std::memset(header->chksum, ' ', 8); // Treat chksum as spaces for calculation
    unsigned long sum = 0;
    unsigned char* bytes = reinterpret_cast<unsigned char*>(header);
    for (size_t i = 0; i < sizeof(TarHeader); ++i) sum += bytes[i];
    snprintf(header->chksum, sizeof(header->chksum), "%06lo", sum);
}

bool Packer::writeFileContent(const FileInfo& file, std::ofstream& archive) {
    std::ifstream input(file.absolutePath, std::ios::binary);
    if (!input.is_open()) return false;

    archive << input.rdbuf();

    // Padding to 512 bytes
    size_t padding = (BLOCK_SIZE - (file.size % BLOCK_SIZE)) % BLOCK_SIZE;
    if (padding > 0) {
        char pad[BLOCK_SIZE] = {0};
        archive.write(pad, padding);
    }
    return true;
}

// --- Unpack Implementation ---

bool Packer::unpack(const std::string& inputArchivePath, const std::string& outputDir) {
    std::ifstream archive(inputArchivePath, std::ios::binary);
    if (!archive.is_open()) {
        std::cerr << "Error: Failed to open archive: " << inputArchivePath << std::endl;
        return false;
    }

    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    TarHeader header;
    while (archive.read(reinterpret_cast<char*>(&header), sizeof(TarHeader))) {
        // Check for End of Archive (empty block)
        if (header.name[0] == '\0') {
            // Read potentially second empty block and break
            break; 
        }

        if (!verifyChecksum(&header)) {
            std::cerr << "Error: Checksum mismatch for file " << header.name << std::endl;
            return false;
        }

        std::string relPath = header.name;
        // Basic path security check: prevent ".." traversal
        if (relPath.find("..") != std::string::npos) {
            std::cerr << "Warning: Skipping unsafe path " << relPath << std::endl;
            continue; 
        }

        fs::path destPath = fs::path(outputDir) / relPath;
        ensureParentDirExists(destPath.string());

        uint64_t fileSize = fromOctal(header.size, sizeof(header.size));
        char type = header.typeflag ? header.typeflag : '0';

        // Process Type
        if (type == '5') { // Directory
            fs::create_directories(destPath);
        } 
        else if (type == '2') { // Symlink
            std::string target = header.linkname;
            if (!target.empty()) {
                if (fs::exists(destPath)) fs::remove(destPath);
                // Create symlink
                if (symlink(target.c_str(), destPath.string().c_str()) != 0) {
                    std::cerr << "Warning: Failed to create symlink " << destPath << std::endl;
                }
            }
        } 
        else { // Regular File ('0' or '\0')
            extractFileContent(archive, destPath.string(), fileSize);
        }

        // Restore Metadata (Permissions & Time)
        restoreMetadata(destPath.string(), &header);
    }

    std::cout << "Unpacking completed to: " << outputDir << std::endl;
    return true;
}

bool Packer::verifyChecksum(const TarHeader* header) {
    TarHeader temp = *header;
    uint64_t storedSum = fromOctal(header->chksum, sizeof(header->chksum));
    calculateChecksum(&temp); // Recalculate based on current data
    uint64_t calcedSum = fromOctal(temp.chksum, sizeof(temp.chksum));
    return storedSum == calcedSum;
}

void Packer::extractFileContent(std::ifstream& archive, const std::string& destPath, uint64_t size) {
    std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot create file " << destPath << std::endl;
        // Skip data in archive to keep alignment
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

    // Skip padding in archive
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
    // 1. Permissions
    mode_t mode = static_cast<mode_t>(fromOctal(header->mode, sizeof(header->mode)));
    chmod(path.c_str(), mode);

    // 2. Times (Access & Modification)
    time_t mtime = static_cast<time_t>(fromOctal(header->mtime, sizeof(header->mtime)));
    struct timeval times[2];
    times[0].tv_sec = mtime; // Access Time (using mtime as fallback)
    times[0].tv_usec = 0;
    times[1].tv_sec = mtime; // Modification Time
    times[1].tv_usec = 0;
    
    // utimes works on symlink target usually, lutimes needed for link itself (less portable)
    // For this lab, applying to file/dir path is sufficient.
    utimes(path.c_str(), times);
}

} // namespace Backup