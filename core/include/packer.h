#pragma once

#include "common.h"
#include <vector>
#include <string>
#include <fstream>

namespace Backup {

/**
 * @brief Packer class responsible for archiving/extracting files using .tar format (POSIX UStar).
 */
class Packer {
public:
    Packer() = default;
    ~Packer() = default;

    /**
     * @brief Packs the given list of files into a single .tar archive.
     * @param files: The list of files metadata collected by Traverser.
     * @param outputArchivePath: The path where the .tar file will be created.
     * @return true if packing is successful, false otherwise.
     */
    bool pack(const std::vector<FileInfo>& files, const std::string& outputArchivePath);

    /**
     * @brief Extracts files from a .tar archive to a destination directory.
     * @param inputArchivePath: The path to the existing .tar file.
     * @param outputDir: The destination directory for extracted files.
     * @return true if unpacking is successful, false otherwise.
     */
    bool unpack(const std::string& inputArchivePath, const std::string& outputDir);

private:
    // POSIX UStar Header Structure (512 bytes)
    struct TarHeader {
        char name[100];     // File name
        char mode[8];       // Permissions
        char uid[8];        // User ID
        char gid[8];        // Group ID
        char size[12];      // File size
        char mtime[12];     // Modification time
        char chksum[8];     // Checksum
        char typeflag;      // File type
        char linkname[100]; // Link name (target of symlink)
        char magic[6];      // "ustar"
        char version[2];    // "00"
        char uname[32];     // User name
        char gname[32];     // Group name
        char devmajor[8];   // Device major number
        char devminor[8];   // Device minor number
        char prefix[155];   // Filename prefix
        char padding[12];   // Padding to 512 bytes
    };

    // --- Helpers for Packing ---
    void fillHeader(const FileInfo& file, TarHeader* header);
    void calculateChecksum(TarHeader* header);
    bool writeFileContent(const FileInfo& file, std::ofstream& archive);

    // --- Helpers for Unpacking ---
    bool verifyChecksum(const TarHeader* header);
    void extractFileContent(std::ifstream& archive, const std::string& destPath, uint64_t size);
    void ensureParentDirExists(const std::string& path);
    void restoreMetadata(const std::string& path, const TarHeader* header);
    
    // --- Utils ---
    uint64_t fromOctal(const char* ptr, size_t len);
};

} // namespace Backup