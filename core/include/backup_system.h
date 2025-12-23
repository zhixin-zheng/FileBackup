#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "common.h"
#include "filter.h"

namespace Backup {

/**
 * @brief 备份系统核心控制类
 * 负责协调 Traverser, Packer, Compressor, Encryptor 完成完整的备份与还原流程
 */
class BackupSystem {
public:
    BackupSystem();
    ~BackupSystem();

    /**
     * @brief 设置压缩算法
     * @param algo: 压缩算法 (HUFFMAN, LZSS, JOINED)
     */
    void setCompressionAlgorithm(int algo);

    /**
     * @brief 设置加密密码
     * @param password: 用户密码（如果为空则不加密）
     */
    void setPassword(const std::string& password);

    /**
     * @brief 设置文件过滤器
     * @param options: 过滤选项
     */
    void setFilter(const Filter& filter);

    /**
     * @brief 执行备份操作
     * 流程: 遍历 -> 打包 -> 压缩 -> 加密 -> 写入文件
     * @param srcDir: 源目录路径
     * @param dstFile: 目标备份文件路径
     * @return true 成功, false 失败
     */
    bool backup(const std::string& srcDir, const std::string& dstPath);

    /**
     * @brief 执行还原操作
     * 流程: 读取文件 -> 解密 -> 解压 -> 解包 -> 写入目录
     * @param srcFile: 备份文件路径
     * @param dstDir: 还原目标目录
     * @return true 成功, false 失败
     */
    bool restore(const std::string& srcFile, const std::string& dstDir);

    /**
     * @brief 验证备份文件（基本要求：备份验证）
     * 流程: 模拟还原流程（不写入磁盘），校验解包后的文件哈希或结构
     * 目前实现简化版：能否成功解密并解压出合法的 Tar 包结构
     * @param backupFile: 备份文件路径
     * @return true 验证通过, false 文件损坏或密码错误
     */
    bool verify(const std::string& backupFile);

private:
    int m_compressionAlgo;      // 当前选用的压缩算法
    std::string m_password;     // 加密密码
    bool m_isEncrypted;         // 是否启用加密
    Filter m_filter;            // 备份过滤器

    // 辅助函数：读写文件
    std::vector<uint8_t> readFile(const std::string& path);
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data);

    // 应用过滤器
    std::vector<FileInfo> applyFilter(const std::vector<FileInfo>& files);
};

} // namespace Backup