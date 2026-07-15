// Sha256.cpp
#include "Sha256.h"

#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <cstdint>

#pragma comment(lib, "bcrypt.lib")

namespace rm {

std::string Sha256Hex(const std::string& input) {
    std::string out;
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) return out;

    DWORD hash_obj_size = 0, result_size = 0;
    status = BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                                (PUCHAR)&hash_obj_size, sizeof(DWORD),
                                &result_size, 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(alg, 0); return out; }

    DWORD hash_size = 32;  // SHA-256 = 32 bytes
    std::vector<uint8_t> hash_obj(hash_obj_size);
    status = BCryptCreateHash(alg, &hash, hash_obj.data(), (ULONG)hash_obj.size(),
                               nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(alg, 0); return out; }

    status = BCryptHashData(hash, (PUCHAR)input.data(), (ULONG)input.size(), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(alg, 0);
        return out;
    }

    std::vector<uint8_t> digest(hash_size);
    status = BCryptFinishHash(hash, digest.data(), hash_size, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(alg, 0);
        return out;
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);

    static const char hex[] = "0123456789abcdef";
    out.resize(hash_size * 2);
    for (DWORD i = 0; i < hash_size; ++i) {
        out[i*2]     = hex[(digest[i] >> 4) & 0xF];
        out[i*2 + 1] = hex[digest[i] & 0xF];
    }
    return out;
}

} // namespace rm