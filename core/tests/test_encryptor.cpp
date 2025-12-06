#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include "../include/encryptor.h"

using namespace Backup;

class EncryptorTest : public ::testing::Test {
protected:
    Encryptor encryptor;

    // 辅助函数：生成随机数据
    std::vector<uint8_t> generateRandomData(size_t size) {
        std::vector<uint8_t> data(size);
        std::mt19937 gen(42); // 固定种子以保证可重复性
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& byte : data) {
            byte = static_cast<uint8_t>(dis(gen));
        }
        return data;
    }

    // 辅助函数：将字符串转换为 vector
    std::vector<uint8_t> stringToVector(const std::string& str) {
        return std::vector<uint8_t>(str.begin(), str.end());
    }
};

// 1. 基本加密解密流程测试
TEST_F(EncryptorTest, BasicRoundTrip) {
    std::string password = "StrongPassword123!";
    std::string originalText = "Hello, OpenSSL Encryption World!";
    auto inputData = stringToVector(originalText);

    // 初始化
    ASSERT_NO_THROW(encryptor.init(password));

    // 加密
    std::vector<uint8_t> encrypted = encryptor.encrypt(inputData);
    
    // 密文不应为空，且长度通常大于明文（因为填充）
    ASSERT_FALSE(encrypted.empty());
    ASSERT_GE(encrypted.size(), inputData.size());

    // 解密
    // 注意：解密通常需要一个新的上下文或者重新 init，
    // 但根据您的 Encryptor 实现，同一个实例可以连续调用 encrypt/decrypt
    std::vector<uint8_t> decrypted = encryptor.decrypt(encrypted);

    // 验证
    EXPECT_EQ(inputData, decrypted) << "Decrypted data does not match original.";
}

// 2. 空数据测试
TEST_F(EncryptorTest, EmptyData) {
    std::string password = "123";
    encryptor.init(password);

    std::vector<uint8_t> emptyInput;
    auto encrypted = encryptor.encrypt(emptyInput);
    EXPECT_TRUE(encrypted.empty()) << "Encrypted empty data should be empty";

    auto decrypted = encryptor.decrypt(encrypted);
    EXPECT_TRUE(decrypted.empty()) << "Decrypted empty data should be empty";
}

// 3. 大数据量测试 (例如 1MB)
TEST_F(EncryptorTest, LargeDataRoundTrip) {
    std::string password = "LargeDataPass";
    encryptor.init(password);

    auto largeData = generateRandomData(1024 * 1024); // 1MB
    
    auto encrypted = encryptor.encrypt(largeData);
    auto decrypted = encryptor.decrypt(encrypted);

    EXPECT_EQ(largeData, decrypted);
}

// 4. 错误密码解密测试
TEST_F(EncryptorTest, WrongPassword) {
    std::string correctPass = "CorrectHorseBatteryStaple";
    std::string wrongPass = "WrongPassword";
    auto inputData = stringToVector("Secret Data");

    // 用正确密码加密
    encryptor.init(correctPass);
    auto encrypted = encryptor.encrypt(inputData);

    // 用错误密码解密
    // 这里需要一个新的 Encryptor 实例，或者重新 init
    Encryptor encryptorBad;
    encryptorBad.init(wrongPass);

    // OpenSSL 解密时，如果密码错误，通常会导致填充检查失败 (Bad Decrypt) 并抛出异常。
    // 但在极少数情况下（约 1/256），解密出的乱码可能碰巧符合 PKCS7 填充规则，导致不抛出异常。
    // 因此，我们应该检查：要么抛出异常，要么解密结果与原数据不一致。
    try {
        auto decryptedBad = encryptorBad.decrypt(encrypted);
        // 如果没抛出异常，解密结果绝对不能和原数据一样
        EXPECT_NE(decryptedBad, inputData) << "Decrypted data with wrong password should not match original";
    } catch (const std::runtime_error&) {
        // 预期行为：捕获到了填充错误异常
    } catch (...) {
        FAIL() << "Caught unexpected exception type";
    }
}

// 5. 未初始化异常测试
TEST_F(EncryptorTest, ThrowIfNotInitialized) {
    Encryptor uninitEncryptor;
    std::vector<uint8_t> data = {1, 2, 3};

    EXPECT_THROW({
        uninitEncryptor.encrypt(data);
    }, std::runtime_error);

    EXPECT_THROW({
        uninitEncryptor.decrypt(data);
    }, std::runtime_error);
}

// 6. 数据完整性/篡改测试
TEST_F(EncryptorTest, TamperedCiphertext) {
    std::string password = "IntegrityTest";
    encryptor.init(password);
    
    auto input = stringToVector("Do not touch my ciphertext!");
    auto encrypted = encryptor.encrypt(input);

    // 篡改密文的一个字节（避免修改开头可能导致 IV 问题，修改中间数据）
    if (encrypted.size() > 10) {
        encrypted[encrypted.size() / 2] ^= 0xFF; // 翻转某些位
    }

    // 解密被篡改的数据应该失败（填充错误）或得到错误结果
    // 您的代码在 DecryptFinal 中检查了错误，通常会抛出异常
    EXPECT_THROW({
        encryptor.decrypt(encrypted);
    }, std::runtime_error);
}

// 7. 确定性测试 (验证固定盐和 PBKDF2 的行为)
// 您的实现使用了固定的盐，所以对于相同的密码，生成的 Key 和 IV 应该是相同的。
// 这意味着相同的明文应该产生完全相同的密文。
TEST_F(EncryptorTest, DeterministicEncryption) {
    std::string password = "FixedSaltTest";
    auto input = stringToVector("Consistency Check");

    Encryptor e1;
    e1.init(password);
    auto c1 = e1.encrypt(input);

    Encryptor e2;
    e2.init(password);
    auto c2 = e2.encrypt(input);

    // 由于使用了固定盐和固定 IV 生成逻辑，密文应该完全一致
    EXPECT_EQ(c1, c2) << "Encryption should be deterministic with fixed salt/IV derivation logic.";
}