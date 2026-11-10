// Small dependency-free AES (128/192/256) block cipher with ECB/CBC/CTR helpers, HMAC, for the javax.crypto stubs.
#ifndef __AES_H__
#define __AES_H__

#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>

namespace aes {

struct Tables {
    uint8_t sbox[256], inv[256];
    Tables() {
        // Build the S-box from the multiplicative inverse in GF(2^8) plus the affine transform.
        uint8_t p = 1, q = 1;
        do {
            p = p ^ (uint8_t)(p << 1) ^ ((p & 0x80) ? 0x1B : 0);
            q ^= q << 1; q ^= q << 2; q ^= q << 4;
            if (q & 0x80) q ^= 0x09;
            uint8_t x = q ^ (uint8_t)((q << 1) | (q >> 7)) ^ (uint8_t)((q << 2) | (q >> 6)) ^ (uint8_t)((q << 3) | (q >> 5)) ^ (uint8_t)((q << 4) | (q >> 4));
            sbox[p] = x ^ 0x63;
        } while (p != 1);
        sbox[0] = 0x63;
        for (int i = 0; i < 256; i++) inv[sbox[i]] = (uint8_t)i;
    }
};
inline const Tables& tables() { static Tables t; return t; }

inline uint8_t xtime(uint8_t x) { return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1B : 0)); }
inline uint8_t mul(uint8_t a, uint8_t b) {
    uint8_t r = 0;
    while (b) { if (b & 1) r ^= a; a = xtime(a); b >>= 1; }
    return r;
}

class Key {
public:
    int rounds = 0;
    uint8_t rk[15][16];

    explicit Key(const std::vector<uint8_t>& key) {
        size_t n = key.size();
        if (n != 16 && n != 24 && n != 32) throw std::runtime_error("java.security.InvalidKeyException: Invalid AES key length: " + std::to_string(n) + " bytes");
        int nk = (int)n / 4;
        rounds = nk + 6;
        const uint8_t* S = tables().sbox;
        std::vector<uint8_t> w(16 * (rounds + 1));
        memcpy(w.data(), key.data(), n);
        uint8_t rcon = 1;
        for (int i = nk; i < 4 * (rounds + 1); i++) {
            uint8_t t[4];
            memcpy(t, &w[(i - 1) * 4], 4);
            if (i % nk == 0) {
                uint8_t tmp = t[0];
                t[0] = S[t[1]] ^ rcon; t[1] = S[t[2]]; t[2] = S[t[3]]; t[3] = S[tmp];
                rcon = xtime(rcon);
            } else if (nk > 6 && i % nk == 4) {
                for (int j = 0; j < 4; j++) t[j] = S[t[j]];
            }
            for (int j = 0; j < 4; j++) w[i * 4 + j] = w[(i - nk) * 4 + j] ^ t[j];
        }
        for (int r = 0; r <= rounds; r++) memcpy(rk[r], &w[r * 16], 16);
    }

    void encrypt(const uint8_t in[16], uint8_t out[16]) const {
        const uint8_t* S = tables().sbox;
        uint8_t s[16];
        for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[0][i];
        for (int r = 1; r <= rounds; r++) {
            uint8_t t[16];
            for (int i = 0; i < 16; i++) t[i] = S[s[i]];
            // ShiftRows (state is column-major: index = col*4 + row)
            uint8_t u[16];
            for (int c = 0; c < 4; c++) for (int row = 0; row < 4; row++) u[c*4 + row] = t[((c + row) % 4)*4 + row];
            if (r != rounds) {
                for (int c = 0; c < 4; c++) {
                    uint8_t* a = &u[c*4];
                    uint8_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
                    a[0] = xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3;
                    a[1] = a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3;
                    a[2] = a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3);
                    a[3] = (xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
                }
            }
            for (int i = 0; i < 16; i++) s[i] = u[i] ^ rk[r][i];
        }
        memcpy(out, s, 16);
    }

    void decrypt(const uint8_t in[16], uint8_t out[16]) const {
        const uint8_t* IS = tables().inv;
        uint8_t s[16];
        for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[rounds][i];
        for (int r = rounds - 1; r >= 0; r--) {
            // InvShiftRows
            uint8_t u[16];
            for (int c = 0; c < 4; c++) for (int row = 0; row < 4; row++) u[((c + row) % 4)*4 + row] = s[c*4 + row];
            for (int i = 0; i < 16; i++) s[i] = IS[u[i]] ^ rk[r][i];
            if (r != 0) {
                for (int c = 0; c < 4; c++) {
                    uint8_t* a = &s[c*4];
                    uint8_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
                    a[0] = mul(a0,14) ^ mul(a1,11) ^ mul(a2,13) ^ mul(a3,9);
                    a[1] = mul(a0,9)  ^ mul(a1,14) ^ mul(a2,11) ^ mul(a3,13);
                    a[2] = mul(a0,13) ^ mul(a1,9)  ^ mul(a2,14) ^ mul(a3,11);
                    a[3] = mul(a0,11) ^ mul(a1,13) ^ mul(a2,9)  ^ mul(a3,14);
                }
            }
        }
        memcpy(out, s, 16);
    }
};


// Streaming cipher: AES in ECB / CBC / CTR with PKCS#5/7 or no padding. Mirrors javax.crypto.Cipher semantics.
class CipherCore {
public:
    enum Mode { ECB, CBC, CTR };
    Mode mode = ECB;
    bool pad = true;
    bool encrypting = true;
    uint8_t iv[16] = {0};        // CBC: previous ciphertext block; CTR: counter block
    std::vector<uint8_t> pending; // input not yet processed
    uint8_t ks[16];               // CTR keystream block
    int ksPos = 16;
    Key* key = nullptr;
    ~CipherCore() { delete key; }

    void init(const std::vector<uint8_t>& k, const uint8_t* ivIn) {
        delete key;
        key = new Key(k);
        if (ivIn) memcpy(iv, ivIn, 16); else memset(iv, 0, 16);
        pending.clear();
        ksPos = 16;
    }

    std::vector<uint8_t> update(const uint8_t* data, size_t len) {
        pending.insert(pending.end(), data, data + len);
        std::vector<uint8_t> out;
        if (mode == CTR) {
            out.resize(pending.size());
            for (size_t i = 0; i < pending.size(); i++) out[i] = pending[i] ^ nextKs();
            pending.clear();
            return out;
        }
        size_t blocks = pending.size() / 16;
        // when decrypting with padding the final block must wait for finish() so the padding can be removed
        if (!encrypting && pad && blocks > 0 && pending.size() % 16 == 0) blocks--;
        for (size_t b = 0; b < blocks; b++) processBlock(&pending[b * 16], out);
        pending.erase(pending.begin(), pending.begin() + blocks * 16);
        return out;
    }

    std::vector<uint8_t> finish() {
        std::vector<uint8_t> out;
        if (mode == CTR) return out;
        if (encrypting) {
            if (pad) {
                size_t padLen = 16 - pending.size() % 16;
                pending.insert(pending.end(), padLen, (uint8_t)padLen);
            } else if (pending.size() % 16)
                throw std::runtime_error("javax.crypto.IllegalBlockSizeException: data not block size aligned");
            for (size_t b = 0; b < pending.size() / 16; b++) processBlock(&pending[b * 16], out);
        } else {
            if (pending.size() % 16 || (pad && pending.empty()))
                throw std::runtime_error("javax.crypto.IllegalBlockSizeException: last block incomplete in decryption");
            for (size_t b = 0; b < pending.size() / 16; b++) processBlock(&pending[b * 16], out);
            if (pad) {
                uint8_t n = out.empty() ? 0 : out.back();
                if (n < 1 || n > 16 || n > out.size())
                    throw std::runtime_error("javax.crypto.BadPaddingException: Given final block not properly padded");
                for (size_t i = 0; i < n; i++)
                    if (out[out.size() - 1 - i] != n) throw std::runtime_error("javax.crypto.BadPaddingException: Given final block not properly padded");
                out.resize(out.size() - n);
            }
        }
        pending.clear();
        return out;
    }

private:
    uint8_t nextKs() {
        if (ksPos == 16) {
            key->encrypt(iv, ks);
            for (int i = 15; i >= 0; i--) if (++iv[i]) break; // big-endian counter increment
            ksPos = 0;
        }
        return ks[ksPos++];
    }

    void processBlock(const uint8_t* in, std::vector<uint8_t>& out) {
        uint8_t b[16];
        if (mode == ECB) {
            if (encrypting) key->encrypt(in, b); else key->decrypt(in, b);
        } else { // CBC
            if (encrypting) {
                uint8_t x[16];
                for (int i = 0; i < 16; i++) x[i] = in[i] ^ iv[i];
                key->encrypt(x, b);
                memcpy(iv, b, 16);
            } else {
                key->decrypt(in, b);
                for (int i = 0; i < 16; i++) b[i] ^= iv[i];
                memcpy(iv, in, 16);
            }
        }
        out.insert(out.end(), b, b + 16);
    }
};

} // namespace aes

#endif
