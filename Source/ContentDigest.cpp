#include "ContentDigest.h"
#include <juce_cryptography/juce_cryptography.h>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

namespace mixworks
{
juce::String contentDigest(juce::InputStream& input)
{
    const auto end = input.getTotalLength();
    const auto complete = [&] { return input.isExhausted() && (end < 0 || input.getPosition() == end); };
#if JUCE_WINDOWS
    struct NativeHash
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        ~NativeHash() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    } native;
    const auto begin = input.getPosition();
    if (BCryptOpenAlgorithmProvider(&native.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0
        && BCryptCreateHash(native.algorithm, &native.hash, nullptr, 0, nullptr, 0, 0) >= 0)
    {
        constexpr int block = 256 * 1024;
        juce::HeapBlock<unsigned char> buffer(block);
        bool valid = true;
        while (!input.isExhausted())
        {
            const int count = input.read(buffer.getData(), block);
            if (count <= 0) { if (!complete()) return {}; break; }
            if (BCryptHashData(native.hash, buffer.getData(), (ULONG) count, 0) < 0) { valid = false; break; }
        }
        unsigned char digest[32] {};
        if (valid && !complete()) return {};
        if (valid && BCryptFinishHash(native.hash, digest, sizeof(digest), 0) >= 0)
            return juce::String::toHexString(digest, sizeof(digest), 0);
        // Native-provider failure must never publish a partial digest. Rewind
        // before falling back; non-seekable failed streams remain unavailable.
        if (!input.setPosition(begin)) return {};
    }
#endif
    const auto digest = juce::SHA256(input).toHexString();
    return complete() ? digest : juce::String();
}
}
