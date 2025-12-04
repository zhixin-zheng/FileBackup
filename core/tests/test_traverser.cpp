#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include "traverser.h"

// 每次测试前创建环境，测试后清理环境
class TraverserTest : public ::testing::Test {
protected:
    std::string testRoot = "./sandbox_test";

    void SetUp() override {
        // 1. 清理残留
        if (std::filesystem::exists(testRoot)) {
            std::filesystem::remove_all(testRoot);
        }
        std::filesystem::create_directories(testRoot);

        // 2. 创建普通文件
        createFile(testRoot + "/file_a.txt", "hello");
        
        // 3. 创建子目录和嵌套文件
        std::filesystem::create_directories(testRoot + "/subdir");
        createFile(testRoot + "/subdir/file_b.log", "world");

        // 4. 创建 MacOS 特有的垃圾文件 (验证是否被跳过)
        createFile(testRoot + "/.DS_Store", "junk data");
        createFile(testRoot + "/subdir/.DS_Store", "junk data");

        // 5. 创建软链接 (指向 file_a.txt)
        try {
            std::filesystem::create_symlink("../file_a.txt", testRoot + "/subdir/link_to_a");
        } catch (...) {
        }
    }

    void TearDown() override {
        if (std::filesystem::exists(testRoot)) {
            std::filesystem::remove_all(testRoot);
        }
    }

    void createFile(const std::string& path, const std::string& content) {
        std::ofstream out(path);
        out << content;
        out.close();
    }

    const Backup::FileInfo* findByRelPath(const std::vector<Backup::FileInfo>& files, const std::string& relPath) {
        auto it = std::find_if(files.begin(), files.end(), [&](const Backup::FileInfo& f) {
            return f.relativePath == relPath;
        });
        return (it != files.end()) ? &(*it) : nullptr;
    }
};

// 1. 测试基本的遍历数量和 .DS_Store 过滤
TEST_F(TraverserTest, BasicScanAndFilter) {
    Backup::Traverser traverser;
    auto results = traverser.traverse(testRoot);

    // 预期包含:
    // 1. file_a.txt
    // 2. subdir (目录本身)
    // 3. subdir/file_b.log
    // 4. subdir/link_to_a (如果软链创建成功)
    // 应该排除: .DS_Store, subdir/.DS_Store
    
    // 检查 .DS_Store 是否被过滤
    EXPECT_EQ(findByRelPath(results, ".DS_Store"), nullptr) << ".DS_Store should be filtered out";
    EXPECT_EQ(findByRelPath(results, "subdir/.DS_Store"), nullptr) << "Nested .DS_Store should be filtered out";

    // 检查正常文件是否存在
    EXPECT_NE(findByRelPath(results, "file_a.txt"), nullptr);
    EXPECT_NE(findByRelPath(results, "subdir/file_b.log"), nullptr);
}

// 2. 测试相对路径计算逻辑
TEST_F(TraverserTest, CheckRelativePaths) {
    Backup::Traverser traverser;
    auto results = traverser.traverse(testRoot);

    const Backup::FileInfo* fileB = findByRelPath(results, "subdir/file_b.log");
    ASSERT_NE(fileB, nullptr);
    
    // 验证你的 getFileInfo 中的 substr 逻辑是否正确去除了 rootDir 前缀
    // 期望: "subdir/file_b.log"
    EXPECT_EQ(fileB->relativePath, "subdir/file_b.log");
}

// 3. 测试文件类型识别
TEST_F(TraverserTest, CheckFileTypes) {
    Backup::Traverser traverser;
    auto results = traverser.traverse(testRoot);

    // 普通文件
    const Backup::FileInfo* fileA = findByRelPath(results, "file_a.txt");
    ASSERT_NE(fileA, nullptr);
    EXPECT_EQ(fileA->type, Backup::FileType::REGULAR);
    EXPECT_EQ(fileA->size, 5); // "hello" is 5 bytes

    // 目录
    const Backup::FileInfo* subDir = findByRelPath(results, "subdir");
    ASSERT_NE(subDir, nullptr);
    EXPECT_EQ(subDir->type, Backup::FileType::DIRECTORY);

    // 软链接
    const Backup::FileInfo* link = findByRelPath(results, "subdir/link_to_a");
    if (link) {
        EXPECT_EQ(link->type, Backup::FileType::SYMLINK);
        // 注意：lstat 获取的是链接本身的大小，不是目标文件的大小
        // 你的代码目前没有读取 linkTarget，所以只能测类型
    }
}

// 4. 测试非法路径异常处理
TEST_F(TraverserTest, ThrowsOnInvalidPath) {
    Backup::Traverser traverser;
    std::string invalidPath = "/path/to/non/existent/directory";
    
    // 验证是否抛出了 std::runtime_error
    EXPECT_THROW({
        traverser.traverse(invalidPath);
    }, std::runtime_error);
}

// 5. 测试空文件夹
TEST_F(TraverserTest, EmptyDirectory) {
    std::string emptyDir = testRoot + "/empty_folder";
    std::filesystem::create_directory(emptyDir);

    Backup::Traverser traverser;
    auto results = traverser.traverse(emptyDir);

    // 预期结果应为空
    EXPECT_EQ(results.size(), 0);
}
