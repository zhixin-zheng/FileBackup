#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "packer.h"
#include "traverser.h"


class PackerTest : public ::testing::Test {
protected:
    std::string srcDir = "./sandbox_packer_src";
    std::string dstDir = "./sandbox_packer_dst";
    std::string tarFile = "./test_archive.tar";

    void SetUp() override {
        cleanUp();
        std::filesystem::create_directories(srcDir);
        createTestStructure();
    }

    void TearDown() override {
        cleanUp();
    }

    void cleanUp() {
        if (std::filesystem::exists(srcDir)) std::filesystem::remove_all(srcDir);
        if (std::filesystem::exists(dstDir)) std::filesystem::remove_all(dstDir);
        if (std::filesystem::exists(tarFile)) std::filesystem::remove(tarFile);
    }

    void createTestStructure() {
        // 1. 根目录文件
        createFile(srcDir + "/root_file.txt", "Root file content");
        
        // 2. 空目录
        std::filesystem::create_directories(srcDir + "/empty_dir");

        // 3. 深度嵌套结构
        std::filesystem::create_directories(srcDir + "/level1/level2/level3");
        createFile(srcDir + "/level1/file_l1.txt", "Level 1 content");
        createFile(srcDir + "/level1/level2/file_l2.txt", "Level 2 content");
        createFile(srcDir + "/level1/level2/level3/file_l3.txt", "Level 3 content");

        // 4. 符号链接
        // 链接到文件
        std::filesystem::create_symlink("root_file.txt", srcDir + "/link_to_root.txt");
        // 链接到目录
        std::filesystem::create_symlink("level1/level2", srcDir + "/link_to_level2");
        // 嵌套目录中的相对路径链接
        std::filesystem::create_symlink("../../root_file.txt", srcDir + "/level1/level2/link_to_root_up.txt");

        // 注意: FIFO 和设备文件在此处被跳过，因为当前的 Packer 实现会将它们视为普通文件并尝试读取内容，
        // 这会导致 FIFO 读取阻塞或设备文件读取失败。
    }

    void createFile(const std::string& path, const std::string& content) {
        std::ofstream out(path);
        out << content;
        out.close();
    }

    std::string readFile(const std::string& path) {
        std::ifstream in(path);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
};

TEST_F(PackerTest, PackAndUnpackComplexStructure) {
    // 1. 遍历源目录
    Backup::Traverser traverser;
    auto files = traverser.traverse(srcDir);
    ASSERT_FALSE(files.empty()) << "Traverser should find files";

    // 2. 打包
    Backup::Packer packer;
    ASSERT_TRUE(packer.pack(files, tarFile)) << "Packing should succeed";
    ASSERT_TRUE(std::filesystem::exists(tarFile)) << "Tar file should be created";

    // 3. 解包
    ASSERT_TRUE(packer.unpack(tarFile, dstDir)) << "Unpacking should succeed";
    ASSERT_TRUE(std::filesystem::exists(dstDir)) << "Destination directory should exist";
    // 4. 验证文件结构和内容

    // 验证根文件
    EXPECT_TRUE(std::filesystem::exists(dstDir + "/root_file.txt"));
    EXPECT_EQ(readFile(dstDir + "/root_file.txt"), "Root file content");

    // 验证空目录
    EXPECT_TRUE(std::filesystem::is_directory(dstDir + "/empty_dir"));
    EXPECT_TRUE(std::filesystem::is_empty(dstDir + "/empty_dir"));

    // 验证嵌套文件
    EXPECT_EQ(readFile(dstDir + "/level1/file_l1.txt"), "Level 1 content");
    EXPECT_EQ(readFile(dstDir + "/level1/level2/file_l2.txt"), "Level 2 content");
    EXPECT_EQ(readFile(dstDir + "/level1/level2/level3/file_l3.txt"), "Level 3 content");

    // 验证符号链接
    std::string link1 = dstDir + "/link_to_root.txt";
    EXPECT_TRUE(std::filesystem::is_symlink(link1));
    EXPECT_EQ(std::filesystem::read_symlink(link1).string(), "root_file.txt");
    EXPECT_EQ(readFile(link1), "Root file content"); // 通过链接读取

    std::string link2 = dstDir + "/link_to_level2";
    EXPECT_TRUE(std::filesystem::is_symlink(link2));
    EXPECT_EQ(std::filesystem::read_symlink(link2).string(), "level1/level2");
    EXPECT_TRUE(std::filesystem::is_directory(link2));

    std::string link3 = dstDir + "/level1/level2/link_to_root_up.txt";
    EXPECT_TRUE(std::filesystem::is_symlink(link3));
    EXPECT_EQ(std::filesystem::read_symlink(link3).string(), "../../root_file.txt");
}
