#include "java_security.h"
#include "hashes.h"
#include "aes.h"
#include "logging.h"
#include <fstream>
#include <stdexcept>

using namespace jnivm::java::security;
using namespace jnivm::javax_crypto;

static std::shared_ptr<FakeJni::JByteArray> toJava(const std::vector<uint8_t>& v)
{
    return std::make_shared<FakeJni::JByteArray>(std::vector<jbyte>(v.begin(), v.end()));
}

static std::vector<uint8_t> fromJava(const std::shared_ptr<FakeJni::JByteArray>& a)
{
    if (!a) throw std::runtime_error("java.lang.NullPointerException");
    const uint8_t* p = reinterpret_cast<const uint8_t*>(a->getArray());
    return std::vector<uint8_t>(p, p + a->getSize());
}

static void fillRandom(uint8_t* out, size_t n)
{
    std::ifstream f("/dev/urandom", std::ios::binary);
    f.read(reinterpret_cast<char*>(out), n);
    if (!f || (size_t)f.gcount() != n)
        throw std::runtime_error("java.security.ProviderException: cannot read /dev/urandom");
}

///// MessageDigest

std::shared_ptr<MessageDigest> MessageDigest::getInstance(std::shared_ptr<FakeJni::JString> algorithm)
{
    std::string name = algorithm ? algorithm->c_str() : "";
    std::vector<uint8_t> probe;
    if (!hashes::digest(name, {}, probe)) {
        verbose("JBRIDGE", "MessageDigest.getInstance(%s): unsupported algorithm", name.c_str());
        throw std::runtime_error("java.security.NoSuchAlgorithmException: " + name + " MessageDigest not available");
    }
    verbose("JBRIDGE", "MessageDigest.getInstance(%s)", name.c_str());
    auto md = std::make_shared<MessageDigest>();
    md->algorithm = name;
    return md;
}

void MessageDigest::update(std::shared_ptr<FakeJni::JByteArray> input)
{
    if (!input) return;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(input->getArray());
    data.insert(data.end(), p, p + input->getSize());
}

void MessageDigest::updateRange(std::shared_ptr<FakeJni::JByteArray> input, int offset, int length)
{
    if (!input) return;
    if (offset < 0 || length < 0 || (size_t)offset + (size_t)length > (size_t)input->getSize())
        throw std::runtime_error("java.lang.IllegalArgumentException: Bad offset/length");
    const uint8_t* p = reinterpret_cast<const uint8_t*>(input->getArray());
    data.insert(data.end(), p + offset, p + offset + length);
}

std::shared_ptr<FakeJni::JByteArray> MessageDigest::digest()
{
    std::vector<uint8_t> out;
    hashes::digest(algorithm, data, out);
    data.clear(); // like Java: digest() resets the object
    return std::make_shared<FakeJni::JByteArray>(std::vector<jbyte>(out.begin(), out.end()));
}

std::shared_ptr<FakeJni::JByteArray> MessageDigest::digestInput(std::shared_ptr<FakeJni::JByteArray> input)
{
    update(input);
    return digest();
}

void MessageDigest::reset() { data.clear(); }

int MessageDigest::getDigestLength()
{
    std::vector<uint8_t> out;
    hashes::digest(algorithm, {}, out);
    return (int)out.size();
}

std::shared_ptr<FakeJni::JString> MessageDigest::getAlgorithm()
{
    return std::make_shared<FakeJni::JString>(algorithm);
}

///// SecureRandom

void SecureRandom::nextBytes(std::shared_ptr<FakeJni::JByteArray> bytes)
{
    if (!bytes) throw std::runtime_error("java.lang.NullPointerException");
    fillRandom(reinterpret_cast<uint8_t*>(bytes->getArray()), bytes->getSize());
}

std::shared_ptr<FakeJni::JByteArray> SecureRandom::generateSeed(int count)
{
    std::vector<jbyte> v(count > 0 ? count : 0);
    if (!v.empty()) fillRandom(reinterpret_cast<uint8_t*>(v.data()), v.size());
    return std::make_shared<FakeJni::JByteArray>(v);
}

void SecureRandom::setSeed(long) {} // supplements the entropy in Java; nothing to do

int SecureRandom::nextInt()
{
    int v;
    fillRandom(reinterpret_cast<uint8_t*>(&v), sizeof(v));
    return v;
}

///// SecretKeySpec / IvParameterSpec

SecretKeySpec::SecretKeySpec(std::shared_ptr<FakeJni::JByteArray> k, std::shared_ptr<FakeJni::JString> alg)
{
    key = fromJava(k);
    algorithm = alg ? alg->c_str() : "";
}
std::shared_ptr<FakeJni::JByteArray> SecretKeySpec::getEncoded() { return toJava(key); }
std::shared_ptr<FakeJni::JString> SecretKeySpec::getAlgorithm() { return std::make_shared<FakeJni::JString>(algorithm); }

IvParameterSpec::IvParameterSpec(std::shared_ptr<FakeJni::JByteArray> v) { iv = fromJava(v); }
std::shared_ptr<FakeJni::JByteArray> IvParameterSpec::getIV() { return toJava(iv); }

///// Cipher

Cipher::Cipher() {}

static std::string upper(std::string s) { for (auto& c : s) c = (char)toupper((unsigned char)c); return s; }

std::shared_ptr<Cipher> Cipher::getInstance(std::shared_ptr<FakeJni::JString> t)
{
    std::string name = t ? t->c_str() : "";
    // "AES", "AES/MODE/PADDING"; Java's default for plain "AES" is ECB with PKCS5 padding.
    std::vector<std::string> parts;
    size_t from = 0;
    for (;;) {
        size_t slash = name.find('/', from);
        parts.push_back(upper(name.substr(from, slash == std::string::npos ? std::string::npos : slash - from)));
        if (slash == std::string::npos) break;
        from = slash + 1;
    }
    std::string mode = parts.size() > 1 ? parts[1] : "ECB", padding = parts.size() > 2 ? parts[2] : "PKCS5PADDING";
    auto core = std::make_shared<aes::CipherCore>();
    bool ok = parts[0] == "AES" && parts.size() != 2 && parts.size() <= 3;
    if (mode == "ECB") core->mode = aes::CipherCore::ECB;
    else if (mode == "CBC") core->mode = aes::CipherCore::CBC;
    else if (mode == "CTR") core->mode = aes::CipherCore::CTR;
    else ok = false;
    if (padding == "PKCS5PADDING" || padding == "PKCS7PADDING") core->pad = true;
    else if (padding == "NOPADDING") core->pad = false;
    else ok = false;
    if (core->mode == aes::CipherCore::CTR) core->pad = false;
    if (!ok) {
        verbose("JBRIDGE", "Cipher.getInstance(%s): unsupported transformation", name.c_str());
        throw std::runtime_error("java.security.NoSuchAlgorithmException: Cannot find any provider supporting " + name);
    }
    verbose("JBRIDGE", "Cipher.getInstance(%s)", name.c_str());
    auto c = std::make_shared<Cipher>();
    c->transformation = name;
    c->core = core;
    return c;
}

void Cipher::initWithParams(int mode, std::shared_ptr<Key> key, std::shared_ptr<spec::AlgorithmParameterSpec> params)
{
    auto sk = std::dynamic_pointer_cast<SecretKeySpec>(key);
    if (!sk) throw std::runtime_error("java.security.InvalidKeyException: No installed provider supports this key");
    if (mode != ENCRYPT_MODE && mode != DECRYPT_MODE)
        throw std::runtime_error("java.lang.UnsupportedOperationException: only ENCRYPT_MODE and DECRYPT_MODE are supported");
    core->encrypting = mode == ENCRYPT_MODE;
    std::vector<uint8_t> ivBytes;
    if (auto ivs = std::dynamic_pointer_cast<IvParameterSpec>(params)) {
        ivBytes = ivs->iv;
        if (ivBytes.size() != 16) throw std::runtime_error("java.security.InvalidAlgorithmParameterException: Wrong IV length: must be 16 bytes long");
    } else if (core->mode != aes::CipherCore::ECB) {
        if (!core->encrypting) throw std::runtime_error("java.security.InvalidKeyException: Parameters missing");
        ivBytes.resize(16); // like Java, encryption without an IV picks a random one (see getIV())
        std::ifstream f("/dev/urandom", std::ios::binary);
        f.read(reinterpret_cast<char*>(ivBytes.data()), 16);
    }
    iv = ivBytes;
    keyBytes = sk->key;
    core->init(keyBytes, ivBytes.empty() ? nullptr : ivBytes.data());
    ready = true;
}

void Cipher::init(int mode, std::shared_ptr<Key> key) { initWithParams(mode, key, nullptr); }

std::shared_ptr<FakeJni::JByteArray> Cipher::update(std::shared_ptr<FakeJni::JByteArray> input)
{
    if (!ready) throw std::runtime_error("java.lang.IllegalStateException: Cipher not initialized");
    auto in = fromJava(input);
    return toJava(core->update(in.data(), in.size()));
}

std::shared_ptr<FakeJni::JByteArray> Cipher::updateRange(std::shared_ptr<FakeJni::JByteArray> input, int offset, int length)
{
    if (!ready) throw std::runtime_error("java.lang.IllegalStateException: Cipher not initialized");
    auto in = fromJava(input);
    if (offset < 0 || length < 0 || (size_t)offset + (size_t)length > in.size())
        throw std::runtime_error("java.lang.IllegalArgumentException: Bad arguments");
    return toJava(core->update(in.data() + offset, length));
}

std::shared_ptr<FakeJni::JByteArray> Cipher::doFinal()
{
    if (!ready) throw std::runtime_error("java.lang.IllegalStateException: Cipher not initialized");
    auto out = core->finish();
    // Like Java, doFinal() leaves the cipher ready to use again with the same key and IV.
    core->init(keyBytes, iv.empty() ? nullptr : iv.data());
    return toJava(out);
}

std::shared_ptr<FakeJni::JByteArray> Cipher::doFinalInput(std::shared_ptr<FakeJni::JByteArray> input)
{
    auto first = update(input);
    auto rest = doFinal();
    std::vector<uint8_t> all = fromJava(first), tail = fromJava(rest);
    all.insert(all.end(), tail.begin(), tail.end());
    return toJava(all);
}

int Cipher::getBlockSize() { return 16; }
std::shared_ptr<FakeJni::JByteArray> Cipher::getIV() { return iv.empty() ? nullptr : toJava(iv); }
std::shared_ptr<FakeJni::JString> Cipher::getAlgorithm() { return std::make_shared<FakeJni::JString>(transformation); }

///// Mac (HMAC)

std::shared_ptr<Mac> Mac::getInstance(std::shared_ptr<FakeJni::JString> a)
{
    std::string name = a ? a->c_str() : "";
    std::string u = upper(name);
    auto m = std::make_shared<Mac>();
    m->algorithm = name;
    if (u == "HMACMD5") m->hashName = "MD5";
    else if (u == "HMACSHA1") m->hashName = "SHA-1";
    else if (u == "HMACSHA224") m->hashName = "SHA-224";
    else if (u == "HMACSHA256") m->hashName = "SHA-256";
    else if (u == "HMACSHA384") { m->hashName = "SHA-384"; m->blockSize = 128; }
    else if (u == "HMACSHA512") { m->hashName = "SHA-512"; m->blockSize = 128; }
    else throw std::runtime_error("java.security.NoSuchAlgorithmException: Algorithm " + name + " not available");
    verbose("JBRIDGE", "Mac.getInstance(%s)", name.c_str());
    return m;
}

void Mac::init(std::shared_ptr<Key> k)
{
    auto sk = std::dynamic_pointer_cast<SecretKeySpec>(k);
    if (!sk) throw std::runtime_error("java.security.InvalidKeyException: Unsupported key");
    key = sk->key;
    data.clear();
}

void Mac::update(std::shared_ptr<FakeJni::JByteArray> input)
{
    auto v = fromJava(input);
    data.insert(data.end(), v.begin(), v.end());
}

void Mac::updateRange(std::shared_ptr<FakeJni::JByteArray> input, int offset, int length)
{
    auto v = fromJava(input);
    if (offset < 0 || length < 0 || (size_t)offset + (size_t)length > v.size())
        throw std::runtime_error("java.lang.IllegalArgumentException: Bad arguments");
    data.insert(data.end(), v.begin() + offset, v.begin() + offset + length);
}

std::shared_ptr<FakeJni::JByteArray> Mac::doFinal()
{
    std::vector<uint8_t> k = key;
    if (k.size() > blockSize) hashes::digest(hashName, std::vector<uint8_t>(k), k);
    k.resize(blockSize, 0);
    std::vector<uint8_t> inner(blockSize), outer(blockSize);
    for (size_t i = 0; i < blockSize; i++) { inner[i] = k[i] ^ 0x36; outer[i] = k[i] ^ 0x5c; }
    inner.insert(inner.end(), data.begin(), data.end());
    std::vector<uint8_t> ih, result;
    hashes::digest(hashName, inner, ih);
    outer.insert(outer.end(), ih.begin(), ih.end());
    hashes::digest(hashName, outer, result);
    data.clear(); // like Java: the Mac is reset (same key) after doFinal
    return toJava(result);
}

std::shared_ptr<FakeJni::JByteArray> Mac::doFinalInput(std::shared_ptr<FakeJni::JByteArray> input)
{
    update(input);
    return doFinal();
}

void Mac::reset() { data.clear(); }

int Mac::getMacLength()
{
    std::vector<uint8_t> out;
    hashes::digest(hashName, {}, out);
    return (int)out.size();
}

std::shared_ptr<FakeJni::JString> Mac::getAlgorithm() { return std::make_shared<FakeJni::JString>(algorithm); }

// Descriptors

BEGIN_NATIVE_DESCRIPTOR(jnivm::java::security::MessageDigest) { FakeJni::Constructor<MessageDigest> {} },
{ FakeJni::Function<&MessageDigest::getInstance> {}, "getInstance", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&MessageDigest::update> {}, "update", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&MessageDigest::updateRange> {}, "update", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&MessageDigest::digest> {}, "digest", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&MessageDigest::digestInput> {}, "digest", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&MessageDigest::reset> {}, "reset", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&MessageDigest::getDigestLength> {}, "getDigestLength", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&MessageDigest::getAlgorithm> {}, "getAlgorithm", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::java::security::SecureRandom) { FakeJni::Constructor<SecureRandom> {} },
{ FakeJni::Function<&SecureRandom::nextBytes> {}, "nextBytes", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&SecureRandom::generateSeed> {}, "generateSeed", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&SecureRandom::setSeed> {}, "setSeed", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&SecureRandom::nextInt> {}, "nextInt", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::java::security::Key) { FakeJni::Constructor<Key> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::java::security::spec::AlgorithmParameterSpec) { FakeJni::Constructor<jnivm::java::security::spec::AlgorithmParameterSpec> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::javax_crypto::SecretKeySpec) { FakeJni::Constructor<SecretKeySpec, std::shared_ptr<FakeJni::JByteArray>, std::shared_ptr<FakeJni::JString>> {} },
{ FakeJni::Function<&SecretKeySpec::getEncoded> {}, "getEncoded", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&SecretKeySpec::getAlgorithm> {}, "getAlgorithm", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::javax_crypto::IvParameterSpec) { FakeJni::Constructor<IvParameterSpec, std::shared_ptr<FakeJni::JByteArray>> {} },
{ FakeJni::Function<&IvParameterSpec::getIV> {}, "getIV", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::javax_crypto::Cipher) { FakeJni::Constructor<Cipher> {} },
{ FakeJni::Field<&Cipher::ENCRYPT_MODE> {}, "ENCRYPT_MODE", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Cipher::DECRYPT_MODE> {}, "DECRYPT_MODE", FakeJni::JFieldID::STATIC },
{ FakeJni::Function<&Cipher::getInstance> {}, "getInstance", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&Cipher::init> {}, "init", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Cipher::initWithParams> {}, "init", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Cipher::update> {}, "update", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Cipher::updateRange> {}, "update", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Cipher::doFinal> {}, "doFinal", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Cipher::doFinalInput> {}, "doFinal", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Cipher::getBlockSize> {}, "getBlockSize", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Cipher::getIV> {}, "getIV", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Cipher::getAlgorithm> {}, "getAlgorithm", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::javax_crypto::Mac) { FakeJni::Constructor<Mac> {} },
{ FakeJni::Function<&Mac::getInstance> {}, "getInstance", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&Mac::init> {}, "init", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Mac::update> {}, "update", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Mac::updateRange> {}, "update", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Mac::doFinal> {}, "doFinal", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Mac::doFinalInput> {}, "doFinal", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Mac::reset> {}, "reset", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Mac::getMacLength> {}, "getMacLength", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Mac::getAlgorithm> {}, "getAlgorithm", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

void InitJNISecurityClasses(FakeJni::Jvm* vm)
{
    vm->registerClass<Key>();
    vm->registerClass<jnivm::java::security::spec::AlgorithmParameterSpec>();
    vm->registerClass<SecretKeySpec>();
    vm->registerClass<IvParameterSpec>();
    vm->registerClass<Cipher>();
    vm->registerClass<Mac>();
    vm->registerClass<MessageDigest>();
    vm->registerClass<SecureRandom>();
}
