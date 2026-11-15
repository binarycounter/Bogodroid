#ifndef __JAVA_SECURITY_H__
#define __JAVA_SECURITY_H__

#include "baron/baron.h"
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

void InitJNISecurityClasses(FakeJni::Jvm* vm);

namespace aes { class CipherCore; }

namespace jnivm {
namespace java {
namespace security {
    // Marker interfaces so java/security/Key and java/security/spec/AlgorithmParameterSpec appear in method signatures.
    class Key : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("java/security/Key")
        virtual ~Key() {}
    };
    namespace spec {
        class AlgorithmParameterSpec : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/security/spec/AlgorithmParameterSpec")
            virtual ~AlgorithmParameterSpec() {}
        };
    }
    // java.security.MessageDigest backed by hashes.h (MD5, SHA-1, SHA-224/256/384/512).
    class MessageDigest : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("java/security/MessageDigest")
        std::string algorithm;
        std::vector<uint8_t> data;

        MessageDigest() {}
        static std::shared_ptr<MessageDigest> getInstance(std::shared_ptr<FakeJni::JString> algorithm);
        void update(std::shared_ptr<FakeJni::JByteArray> input);
        void updateRange(std::shared_ptr<FakeJni::JByteArray> input, int offset, int length);
        std::shared_ptr<FakeJni::JByteArray> digest();
        std::shared_ptr<FakeJni::JByteArray> digestInput(std::shared_ptr<FakeJni::JByteArray> input);
        void reset();
        int getDigestLength();
        std::shared_ptr<FakeJni::JString> getAlgorithm();
    };

    // java.security.SecureRandom reading /dev/urandom.
    class SecureRandom : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("java/security/SecureRandom")
        SecureRandom() {}
        void nextBytes(std::shared_ptr<FakeJni::JByteArray> bytes);
        std::shared_ptr<FakeJni::JByteArray> generateSeed(int count);
        void setSeed(long seed);
        int nextInt();
    };
}
}

namespace javax_crypto {
    class SecretKeySpec : public java::security::Key {
    public:
        DEFINE_CLASS_NAME("javax/crypto/spec/SecretKeySpec", java::security::Key)
        std::vector<uint8_t> key;
        std::string algorithm;
        SecretKeySpec(std::shared_ptr<FakeJni::JByteArray> key, std::shared_ptr<FakeJni::JString> algorithm);
        std::shared_ptr<FakeJni::JByteArray> getEncoded();
        std::shared_ptr<FakeJni::JString> getAlgorithm();
    };

    class IvParameterSpec : public java::security::spec::AlgorithmParameterSpec {
    public:
        DEFINE_CLASS_NAME("javax/crypto/spec/IvParameterSpec", java::security::spec::AlgorithmParameterSpec)
        std::vector<uint8_t> iv;
        IvParameterSpec(std::shared_ptr<FakeJni::JByteArray> iv);
        std::shared_ptr<FakeJni::JByteArray> getIV();
    };

    // javax.crypto.Cipher: AES in ECB / CBC / CTR with PKCS5/PKCS7/no padding.
    class Cipher : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("javax/crypto/Cipher")
        inline static int ENCRYPT_MODE = 1;
        inline static int DECRYPT_MODE = 2;
        std::string transformation;
        std::shared_ptr<aes::CipherCore> core;
        std::vector<uint8_t> iv;
        std::vector<uint8_t> keyBytes;
        bool ready = false;

        Cipher();
        static std::shared_ptr<Cipher> getInstance(std::shared_ptr<FakeJni::JString> transformation);
        void init(int mode, std::shared_ptr<java::security::Key> key);
        void initWithParams(int mode, std::shared_ptr<java::security::Key> key, std::shared_ptr<java::security::spec::AlgorithmParameterSpec> params);
        std::shared_ptr<FakeJni::JByteArray> update(std::shared_ptr<FakeJni::JByteArray> input);
        std::shared_ptr<FakeJni::JByteArray> updateRange(std::shared_ptr<FakeJni::JByteArray> input, int offset, int length);
        std::shared_ptr<FakeJni::JByteArray> doFinal();
        std::shared_ptr<FakeJni::JByteArray> doFinalInput(std::shared_ptr<FakeJni::JByteArray> input);
        int getBlockSize();
        std::shared_ptr<FakeJni::JByteArray> getIV();
        std::shared_ptr<FakeJni::JString> getAlgorithm();
    };

    // javax.crypto.Mac: HMAC over the digests in hashes.h.
    class Mac : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("javax/crypto/Mac")
        std::string algorithm;   // e.g. HmacSHA256
        std::string hashName;    // e.g. SHA-256
        size_t blockSize = 64;
        std::vector<uint8_t> key;
        std::vector<uint8_t> data;
        Mac() {}
        static std::shared_ptr<Mac> getInstance(std::shared_ptr<FakeJni::JString> algorithm);
        void init(std::shared_ptr<java::security::Key> key);
        void update(std::shared_ptr<FakeJni::JByteArray> input);
        void updateRange(std::shared_ptr<FakeJni::JByteArray> input, int offset, int length);
        std::shared_ptr<FakeJni::JByteArray> doFinal();
        std::shared_ptr<FakeJni::JByteArray> doFinalInput(std::shared_ptr<FakeJni::JByteArray> input);
        void reset();
        int getMacLength();
        std::shared_ptr<FakeJni::JString> getAlgorithm();
    };
}
}

#endif
