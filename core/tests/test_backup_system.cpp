#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include "../include/backup_system.h" // 假设 BackupSystem 头文件路径

using namespace Backup;

class BackupSystemTest : public ::testing::Test {
protected:
    std::string testRoot = "./sandbox_backup_system";
    std::string srcDir = testRoot + "/source";
    std::string dstDir = testRoot + "/restore";
    std::string backupFile = testRoot + "/backup.dat";

    void SetUp() override {
        // 清理环境
        if (std::filesystem::exists(testRoot)) {
            std::filesystem::remove_all(testRoot);
        }
        std::filesystem::create_directories(srcDir);
        std::filesystem::create_directories(dstDir);
        // 创建测试文件结构
        createFile(srcDir + "/file1.txt", "Content of file 1");
        createFile(srcDir + "/file2.log", "Log data...");
        std::filesystem::create_directories(srcDir + "/subdir");
        createFile(srcDir + "/subdir/file3.bin", "Binary data \x00\x01\x02");
    }

    void TearDown() override {
        // 可选：测试后清理
        // fs::remove_all(testRoot);
    }

    void createFile(const std::string& path, const std::string& content) {
        std::ofstream out(path, std::ios::binary);
        out << content;
        out.close();
    }

    std::string readFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return "";
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    // 递归比较两个目录是否一致
    bool compareDirectories(const std::string& dir1, const std::string& dir2) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir1)) {
            std::string relPath = std::filesystem::relative(entry.path(), dir1).string();
            std::string path2 = dir2 + "/" + relPath;

            if (!std::filesystem::exists(path2)) {
                std::cerr << "Missing in restore: " << relPath << std::endl;
                return false;
            }

            if (std::filesystem::is_directory(entry)) {
                if (!std::filesystem::is_directory(path2)) return false;
            } else if (std::filesystem::is_regular_file(entry)) {
                if (readFile(entry.path().string()) != readFile(path2)) {
                    std::cerr << "Content mismatch: " << relPath << std::endl;
                    return false;
                }
            }
        }
        return true;
    }
};

// 1. 基础流程：不加密，默认压缩 (LZSS)
TEST_F(BackupSystemTest, BasicBackupRestore) {
    BackupSystem bs;
    // 默认 LZSS，无密码

    // 执行备份
    ASSERT_TRUE(bs.backup(srcDir, backupFile));
    ASSERT_TRUE(std::filesystem::exists(backupFile));

    // 执行还原
    ASSERT_TRUE(bs.restore(backupFile, dstDir));

    // 验证内容
    EXPECT_TRUE(compareDirectories(srcDir, dstDir));
}

// 2. 加密流程：设置密码
TEST_F(BackupSystemTest, EncryptedBackupRestore) {
    BackupSystem bs;
    bs.setPassword("MySecretPass");

    // 执行备份
    ASSERT_TRUE(bs.backup(srcDir, backupFile));
    
    // 验证文件已生成
    ASSERT_TRUE(std::filesystem::exists(backupFile));

    // 执行还原
    ASSERT_TRUE(bs.restore(backupFile, dstDir));

    // 验证内容
    EXPECT_TRUE(compareDirectories(srcDir, dstDir));
}

// 3. 错误密码测试
TEST_F(BackupSystemTest, RestoreWithWrongPassword) {
    // 先用密码 A 备份
    {
        BackupSystem bs;
        bs.setPassword("CorrectPassword");
        ASSERT_TRUE(bs.backup(srcDir, backupFile));
    }

    // 再用密码 B 还原
    {
        BackupSystem bs;
        bs.setPassword("WrongPassword");
        // 还原应该失败（解密失败或数据损坏）
        EXPECT_FALSE(bs.restore(backupFile, dstDir));
    }
}

// 4. 压缩算法切换 (Huffman)
TEST_F(BackupSystemTest, HuffmanCompression) {
    BackupSystem bs;
    // 假设 0 = Huffman (根据 CompressionAlgorithm 枚举)
    bs.setCompressionAlgorithm(0); 

    ASSERT_TRUE(bs.backup(srcDir, backupFile));
    ASSERT_TRUE(bs.restore(backupFile, dstDir));
    EXPECT_TRUE(compareDirectories(srcDir, dstDir));
}

// 5. 备份验证功能 (Verify)
TEST_F(BackupSystemTest, VerifyBackup) {
    BackupSystem bs;
    bs.setPassword("VerifyPass");

    // 备份
    ASSERT_TRUE(bs.backup(srcDir, backupFile));

    // 验证：使用正确密码
    EXPECT_TRUE(bs.verify(backupFile));

    // 验证：使用错误密码
    BackupSystem bsWrong;
    bsWrong.setPassword("Wrong");
    EXPECT_FALSE(bsWrong.verify(backupFile));

    // 验证：破坏文件
    {
        std::fstream f(backupFile, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(10); // 破坏头部数据
        f.put(0xFF); 
    }
    EXPECT_FALSE(bs.verify(backupFile));
}

// 6. 空目录或不存在的目录
TEST_F(BackupSystemTest, InvalidSource) {
    BackupSystem bs;
    // 备份不存在的目录
    EXPECT_FALSE(bs.backup("/path/to/nowhere", backupFile));
}