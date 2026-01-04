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
    bool compareDirectories(const std::string& dir1, const std::string& wrapped_dir2) {
        std::string dir2 = wrapped_dir2 + "/" + std::filesystem::path(dir1).filename().string();
        std::cerr << "[Compare] Comparing " << dir1 << " with " << dir2 << std::endl;
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
        EXPECT_THROW(bs.restore(backupFile, dstDir), std::runtime_error);
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
    EXPECT_THROW(bsWrong.verify(backupFile), std::runtime_error);

    // 验证：破坏文件
    {
        std::fstream f(backupFile, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(10); // 破坏头部数据
        f.put(0xFF); 
    }
    EXPECT_THROW(bs.verify(backupFile), std::runtime_error);
}

// 6. 空目录或不存在的目录
TEST_F(BackupSystemTest, InvalidSource) {
    BackupSystem bs;
    // 备份不存在的目录
    EXPECT_THROW(bs.backup("/path/to/nowhere", backupFile), std::runtime_error);
}

// 7. [NEW] 过滤器测试：后缀名与大小限制
TEST_F(BackupSystemTest, BackupWithFilter) {
    BackupSystem bs;

    // --- 准备额外的测试文件 ---
    // 1. 应该被后缀过滤器保留的文件
    createFile(srcDir + "/extra.txt", "keep this text");
    // 2. 应该被后缀过滤器忽略的文件
    createFile(srcDir + "/ignore.jpg", "fake image data");
    // 3. 应该被大小过滤器忽略的大文件 (10KB)
    std::string bigData(10240, 'A'); 
    createFile(srcDir + "/large_doc.txt", bigData);

    Filter opts;
    opts.enabled = true;
    
    // 规则A: 只保留 .txt, .log
    opts.suffixes = {".txt", ".log"}; 
    
    // 规则B: 最大文件大小 5000 字节 (过滤掉 large_doc.txt)
    opts.maxSize = 5000; 

    bs.setFilter(opts);

    ASSERT_TRUE(bs.backup(srcDir, backupFile));
    ASSERT_TRUE(bs.restore(backupFile, dstDir));

    // --- 验证结果 ---
    // 1. 验证应保留的文件 (符合后缀且足够小)
    std::string real_dstDir = dstDir + "/" + std::filesystem::path(srcDir).filename().string();
    EXPECT_TRUE(std::filesystem::exists(real_dstDir + "/file1.txt"));       // 来自 SetUp
    EXPECT_TRUE(std::filesystem::exists(real_dstDir + "/file2.log"));       // 来自 SetUp
    EXPECT_TRUE(std::filesystem::exists(real_dstDir + "/extra.txt"));       // 新增的小 .txt

    // 2. 验证应被过滤的文件
    EXPECT_FALSE(std::filesystem::exists(real_dstDir + "/subdir/file3.bin")); // 后缀 .bin 不在白名单
    EXPECT_FALSE(std::filesystem::exists(real_dstDir + "/ignore.jpg"));       // 后缀 .jpg 不在白名单
    EXPECT_FALSE(std::filesystem::exists(real_dstDir + "/large_doc.txt"));    // 也是 .txt 但超过了 maxSize (10KB > 5KB)

    std::cout << "[Test Info] Filter test passed: Only valid files were restored." << std::endl;
}

// 8. [NEW] 过滤器测试：文件名正则匹配
TEST_F(BackupSystemTest, BackupWithNameRegex) {
    BackupSystem bs;

    // --- 准备测试文件 ---
    createFile(srcDir + "/report_2023.txt", "data");
    createFile(srcDir + "/report_2024.txt", "data");
    createFile(srcDir + "/draft_notes.txt", "data");
    createFile(srcDir + "/image.png", "data");

    // --- 配置过滤器 ---
    Filter opts;
    opts.enabled = true;
    
    // 规则: 只保留文件名以 "report_" 开头的文件
    // 注意: std::regex_match 需要匹配整个字符串，所以要加 .*
    opts.nameRegex = "^report*"; 

    bs.setFilter(opts);

    // --- 执行 ---
    ASSERT_TRUE(bs.backup(srcDir, backupFile));
    ASSERT_TRUE(bs.restore(backupFile, dstDir));

    // --- 验证 ---
    std::string real_dstDir = dstDir + "/" + std::filesystem::path(srcDir).filename().string();
    EXPECT_TRUE(std::filesystem::exists(real_dstDir + "/report_2023.txt"));
    EXPECT_TRUE(std::filesystem::exists(real_dstDir + "/report_2024.txt"));
    
    // 原有的 file1.txt, file2.log 也不符合 "report_.*"，会被过滤
    EXPECT_FALSE(std::filesystem::exists(real_dstDir + "/file1.txt"));
    EXPECT_FALSE(std::filesystem::exists(real_dstDir + "/draft_notes.txt"));
    EXPECT_FALSE(std::filesystem::exists(real_dstDir + "/image.png"));

    std::cout << "[Test Info] Regex filter test passed." << std::endl;
}

TEST_F(BackupSystemTest, BackupWithNameKeywords) {
    BackupSystem bs;

    // --- 准备测试文件 ---
    createFile(srcDir + "/project_alpha_v1.code", "data");
    createFile(srcDir + "/project_beta_v2.code", "data");
    createFile(srcDir + "/notes_alpha.txt", "data");
    createFile(srcDir + "/vacation.jpg", "image");
    
    // 特殊字符测试文件 (包含括号和加号)
    createFile(srcDir + "/calc(v1+2).cpp", "math"); 

    // --- 配置过滤器 ---
    Filter opts;
    opts.enabled = true;
    
    // 场景：用户输入关键词 "alpha" 和 "(v1+2)"
    // 系统应该自动将其转换为正则类似 ".*(alpha|\(v1\+2\)).*"
    // 从而匹配到 "project_alpha_v1.code", "notes_alpha.txt", "calc(v1+2).cpp"
    opts.nameKeywords = {"alpha", "(v1+2)"}; 

    bs.setFilter(opts);

    // --- 执行 ---
    ASSERT_TRUE(bs.backup(srcDir, backupFile));
    ASSERT_TRUE(bs.restore(backupFile, dstDir));

    // --- 验证保留的文件 ---
    std::string real_dstDir = dstDir + "/" + std::filesystem::path(srcDir).filename().string();
    EXPECT_TRUE(std::filesystem::exists(real_dstDir + "/project_alpha_v1.code")); // 匹配 "alpha"
    EXPECT_TRUE(std::filesystem::exists(real_dstDir + "/notes_alpha.txt"));       // 匹配 "alpha"
    EXPECT_TRUE(std::filesystem::exists(real_dstDir + "/calc(v1+2).cpp"));        // 匹配 "(v1+2)" (特殊字符应被转义)

    // --- 验证被过滤的文件 ---
    EXPECT_FALSE(std::filesystem::exists(real_dstDir + "/project_beta_v2.code")); // 不包含关键字
    EXPECT_FALSE(std::filesystem::exists(real_dstDir + "/vacation.jpg"));         // 不包含关键字
    EXPECT_FALSE(std::filesystem::exists(real_dstDir + "/file1.txt"));            // 原有的文件也不包含

    std::cout << "[Test Info] Keyword to Regex conversion test passed." << std::endl;
}