#include "encryptor.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace Backup {

// 自定义错误处理宏
#define HANDLE_OPENSSL_ERROR(msg) \
    throw std::runtime_error(std::string(msg) + ": " + std::to_string(ERR_get_error()))

// PImpl 实现
struct Encryptor::Impl {
    EVP_CIPHER_CTX* ctx;
    unsigned char key[32]; // AES-256 密钥 (32 字节)
    unsigned char iv[16];  // AES 块大小 IV (16 字节)
    bool initialized = false;

    Impl() {
        ctx = EVP_CIPHER_CTX_new();
        if (!ctx) HANDLE_OPENSSL_ERROR("创建 EVP_CIPHER_CTX 失败");
    }

    ~Impl() {
        if (ctx) EVP_CIPHER_CTX_free(ctx);
    }
};

Encryptor::Encryptor() : pImpl(new Impl()) {}

Encryptor::~Encryptor() {
    delete pImpl;
}

void Encryptor::init(const std::string& password) {
    // 1. 密钥派生 (PBKDF2)
    // 在生产环境中，盐应该是随机的并随文件存储。
    // 为了简化本项目，我们使用固定的盐。
    const unsigned char* salt = (const unsigned char*)"BackupSystemSalt"; 
    int iter = 10000; // 迭代次数

    // 派生 32 字节密钥
    if (!PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                           salt, strlen((const char*)salt), iter,
                           EVP_sha256(), 32, pImpl->key)) {
        HANDLE_OPENSSL_ERROR("PBKDF2 密钥派生失败");
    }

    // 2. IV 派生
    // 使用不同的盐派生 IV，以避免在密钥重复使用时的 Key/IV 重复模式。
    const unsigned char* iv_salt = (const unsigned char*)"BackupSystemIV";
    if (!PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                           iv_salt, strlen((const char*)iv_salt), iter,
                           EVP_sha256(), 16, pImpl->iv)) {
        HANDLE_OPENSSL_ERROR("PBKDF2 IV 派生失败");
    }

    pImpl->initialized = true;
}

std::vector<uint8_t> Encryptor::encrypt(const std::vector<uint8_t>& inData) {
    if (!pImpl->initialized) {
        throw std::runtime_error("加密器未初始化。请先调用 init()。");
    }
    if (inData.empty()) return {};

    // 1. 初始化加密上下文
    // AES-256-CBC 模式
    if (1 != EVP_EncryptInit_ex(pImpl->ctx, EVP_aes_256_cbc(), NULL, pImpl->key, pImpl->iv)) {
        HANDLE_OPENSSL_ERROR("EncryptInit 失败");
    }

    // 2. 准备输出缓冲区
    // 由于填充，密文长度最多比输入长度大一个块大小 (16)
    std::vector<uint8_t> outData(inData.size() + EVP_MAX_BLOCK_LENGTH);
    int len = 0;
    int ciphertext_len = 0;

    // 3. 加密更新 (处理数据)
    if (1 != EVP_EncryptUpdate(pImpl->ctx, outData.data(), &len, inData.data(), inData.size())) {
        HANDLE_OPENSSL_ERROR("EncryptUpdate 失败");
    }
    ciphertext_len = len;

    // 4. 加密结束 (处理填充)
    if (1 != EVP_EncryptFinal_ex(pImpl->ctx, outData.data() + len, &len)) {
        HANDLE_OPENSSL_ERROR("EncryptFinal 失败");
    }
    ciphertext_len += len;

    // 调整为实际长度
    outData.resize(ciphertext_len);
    return outData;
}

std::vector<uint8_t> Encryptor::decrypt(const std::vector<uint8_t>& inData) {
    if (!pImpl->initialized) {
        throw std::runtime_error("加密器未初始化。请先调用 init()。");
    }
    if (inData.empty()) return {};

    // 1. 初始化解密上下文
    if (1 != EVP_DecryptInit_ex(pImpl->ctx, EVP_aes_256_cbc(), NULL, pImpl->key, pImpl->iv)) {
        HANDLE_OPENSSL_ERROR("DecryptInit 失败");
    }

    // 2. 准备输出缓冲区
    // 明文通常与密文大小相同或更小（去除了填充），但分配足够的空间
    std::vector<uint8_t> outData(inData.size() + EVP_MAX_BLOCK_LENGTH);
    int len = 0;
    int plaintext_len = 0;

    // 3. 解密更新
    if (1 != EVP_DecryptUpdate(pImpl->ctx, outData.data(), &len, inData.data(), inData.size())) {
        HANDLE_OPENSSL_ERROR("DecryptUpdate 失败");
    }
    plaintext_len = len;

    // 4. 解密结束 (检查填充并完成)
    // 如果密码错误或数据损坏（填充错误），此步骤将失败
    if (1 != EVP_DecryptFinal_ex(pImpl->ctx, outData.data() + len, &len)) {
        // 可选：记录 OpenSSL 错误用于调试
        // unsigned long err = ERR_get_error();
        // char buf[256];
        // ERR_error_string_n(err, buf, sizeof(buf));
        // std::cerr << "OpenSSL 解密错误: " << buf << std::endl;
        
        throw std::runtime_error("解密失败 (请检查密码/数据完整性)");
    }
    plaintext_len += len;

    outData.resize(plaintext_len);
    return outData;
}

} // namespace Backup