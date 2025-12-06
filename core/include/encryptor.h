#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace Backup {

/**
 * @brief 使用 OpenSSL 的 AES-256-CBC 加密器/解密器。
 * 使用 PBKDF2 从用户密码派生密钥。
 */
class Encryptor {
public:
    Encryptor();
    ~Encryptor();

    /**
     * @brief 从密码初始化加密密钥和 IV。
     * 必须在 encrypt() 或 decrypt() 之前调用。
     * @param password: 用户密码。
     */
    void init(const std::string& password);

    /**
     * @brief 加密数据块。
     * @param inData: 明文数据。
     * @return 密文数据（包含填充）。
     */
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& inData);

    /**
     * @brief 解密数据块。
     * @param inData: 密文数据。
     * @return 明文数据。
     */
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& inData);

private:
    // 实现结构体的前向声明（PImpl 惯用法）
    struct Impl;
    Impl* pImpl; 
};

} // namespace Backup