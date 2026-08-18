#include "AddressUtil.h"
#include "CryptoUtil.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64)
#define SHA256_SHANI_HEADER_ONLY
#include "sha256_shani.cpp"
#endif

static inline unsigned int endian(unsigned int x)
{
#if defined(__GNUC__)
	return __builtin_bswap32(x);
#else
	return (x << 24) | ((x << 8) & 0x00ff0000) | ((x >> 8) & 0x0000ff00) | (x >> 24);
#endif
}

static const unsigned int SHA256_IV[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/* Pack 33-byte compressed pubkey (02/03 || x_be) plus SHA-256 padding into
 * 16 big-endian SHA-256 words. x[] is little-endian uint32 limbs. */
static inline void packCompressedLe32(unsigned int msg[16], const unsigned int *x, unsigned int yParity)
{
	msg[0] = (x[7] >> 8) | ((yParity & 1u) ? 0x03000000u : 0x02000000u);
	msg[1] = (x[6] >> 8) | (x[7] << 24);
	msg[2] = (x[5] >> 8) | (x[6] << 24);
	msg[3] = (x[4] >> 8) | (x[5] << 24);
	msg[4] = (x[3] >> 8) | (x[4] << 24);
	msg[5] = (x[2] >> 8) | (x[3] << 24);
	msg[6] = (x[1] >> 8) | (x[2] << 24);
	msg[7] = (x[0] >> 8) | (x[1] << 24);
	msg[8] = (x[0] << 24) | 0x00800000u;
	msg[9] = 0;
	msg[10] = 0;
	msg[11] = 0;
	msg[12] = 0;
	msg[13] = 0;
	msg[14] = 0;
	msg[15] = 33 * 8;
}

static inline void limbs64To32(unsigned int x32[8], const uint64_t *xLe)
{
	x32[0] = (unsigned int)xLe[0];
	x32[1] = (unsigned int)(xLe[0] >> 32);
	x32[2] = (unsigned int)xLe[1];
	x32[3] = (unsigned int)(xLe[1] >> 32);
	x32[4] = (unsigned int)xLe[2];
	x32[5] = (unsigned int)(xLe[2] >> 32);
	x32[6] = (unsigned int)xLe[3];
	x32[7] = (unsigned int)(xLe[3] >> 32);
}

static inline void finishHash160(unsigned int msg[16], const unsigned int sha256Digest[8], unsigned int *digest)
{
	msg[0] = endian(sha256Digest[0]);
	msg[1] = endian(sha256Digest[1]);
	msg[2] = endian(sha256Digest[2]);
	msg[3] = endian(sha256Digest[3]);
	msg[4] = endian(sha256Digest[4]);
	msg[5] = endian(sha256Digest[5]);
	msg[6] = endian(sha256Digest[6]);
	msg[7] = endian(sha256Digest[7]);
	msg[8] = 0x00000080;
	msg[9] = 0;
	msg[10] = 0;
	msg[11] = 0;
	msg[12] = 0;
	msg[13] = 0;
	msg[14] = 256;
	msg[15] = 0;

	crypto::ripemd160(msg, digest);
}

static void hashCompressedSoft(const unsigned int *xLe, unsigned int yParity, unsigned int *digest)
{
	unsigned int msg[16];
	unsigned int sha256Digest[8];

	packCompressedLe32(msg, xLe, yParity);
	crypto::sha256Init(sha256Digest);
	crypto::sha256(msg, sha256Digest);
	finishHash160(msg, sha256Digest, digest);
}

#if defined(__x86_64__) || defined(_M_X64)
#if defined(__GNUC__)
__attribute__((target("sse4.1,sha")))
#endif
static void hashCompressedNi(const unsigned int *xLe, unsigned int yParity, unsigned int *digest)
{
	unsigned int msg[16];
	unsigned int sha256Digest[8];

	packCompressedLe32(msg, xLe, yParity);
	memcpy(sha256Digest, SHA256_IV, sizeof(SHA256_IV));
	sha256_process_words(sha256Digest, msg);
	finishHash160(msg, sha256Digest, digest);
}

#if defined(__GNUC__)
__attribute__((target("sse4.1,sha")))
#endif
static void hashCompressedNi2(const unsigned int *xLe0, unsigned int yParity0, unsigned int *digest0,
	const unsigned int *xLe1, unsigned int yParity1, unsigned int *digest1)
{
	unsigned int msg0[16];
	unsigned int msg1[16];
	unsigned int sha0[8];
	unsigned int sha1[8];

	packCompressedLe32(msg0, xLe0, yParity0);
	packCompressedLe32(msg1, xLe1, yParity1);
	memcpy(sha0, SHA256_IV, sizeof(SHA256_IV));
	memcpy(sha1, SHA256_IV, sizeof(SHA256_IV));
	sha256_process_words2(sha0, msg0, sha1, msg1);
	finishHash160(msg0, sha0, digest0);
	finishHash160(msg1, sha1, digest1);
}
#endif

bool Address::verifyAddress(std::string address)
{
	// Check length
	if(address.length() > 34) {
		false;
	}

	// Check encoding
	if(!Base58::isBase58(address)) {
		return false;
	}

	std::string noPrefix = address.substr(1);

	secp256k1::uint256 value = Base58::toBigInt(noPrefix);
	unsigned int words[6];
	unsigned int hash[5];
	unsigned int checksum;

	value.exportWords(words, 6, secp256k1::uint256::BigEndian);
	memcpy(hash, words, sizeof(unsigned int) * 5);
	checksum = words[5];

	return crypto::checksum(hash) == checksum;
}

std::string Address::fromPublicKey(const secp256k1::ecpoint &p, bool compressed)
{
	unsigned int xWords[8] = { 0 };
	unsigned int yWords[8] = { 0 };

	p.x.exportWords(xWords, 8, secp256k1::uint256::BigEndian);
	p.y.exportWords(yWords, 8, secp256k1::uint256::BigEndian);

	unsigned int digest[5];

	if(compressed) {
		Hash::hashPublicKeyCompressed(xWords, yWords, digest);
	} else {
		Hash::hashPublicKey(xWords, yWords, digest);
	}

	unsigned int checksum = crypto::checksum(digest);

	unsigned int addressWords[8] = { 0 };
	for(int i = 0; i < 5; i++) {
		addressWords[2 + i] = digest[i];
	}
	addressWords[7] = checksum;

	secp256k1::uint256 addressBigInt(addressWords, secp256k1::uint256::BigEndian);

	return "1" + Base58::toBase58(addressBigInt);
}

void Hash::hashPublicKey(const secp256k1::ecpoint &p, unsigned int *digest)
{
	unsigned int xWords[8];
	unsigned int yWords[8];

	p.x.exportWords(xWords, 8, secp256k1::uint256::BigEndian);
	p.y.exportWords(yWords, 8, secp256k1::uint256::BigEndian);

	hashPublicKey(xWords, yWords, digest);
}


void Hash::hashPublicKeyCompressed(const secp256k1::ecpoint &p, unsigned int *digest)
{
	hashPublicKeyCompressed(p.x.v, p.y.v[0] & 1u, digest);
}

void Hash::hashPublicKey(const unsigned int *x, const unsigned int *y, unsigned int *digest)
{
	unsigned int msg[16];
	unsigned int sha256Digest[8];

	// 0x04 || x || y
	msg[15] = (y[7] >> 8) | (y[6] << 24);
	msg[14] = (y[6] >> 8) | (y[5] << 24);
	msg[13] = (y[5] >> 8) | (y[4] << 24);
	msg[12] = (y[4] >> 8) | (y[3] << 24);
	msg[11] = (y[3] >> 8) | (y[2] << 24);
	msg[10] = (y[2] >> 8) | (y[1] << 24);
	msg[9] = (y[1] >> 8) | (y[0] << 24);
	msg[8] = (y[0] >> 8) | (x[7] << 24);
	msg[7] = (x[7] >> 8) | (x[6] << 24);
	msg[6] = (x[6] >> 8) | (x[5] << 24);
	msg[5] = (x[5] >> 8) | (x[4] << 24);
	msg[4] = (x[4] >> 8) | (x[3] << 24);
	msg[3] = (x[3] >> 8) | (x[2] << 24);
	msg[2] = (x[2] >> 8) | (x[1] << 24);
	msg[1] = (x[1] >> 8) | (x[0] << 24);
	msg[0] = (x[0] >> 8) | 0x04000000;


	crypto::sha256Init(sha256Digest);
	crypto::sha256(msg, sha256Digest);

	// Zero out the message
	for(int i = 0; i < 16; i++) {
		msg[i] = 0;
	}

	// Set first byte, padding, and length
	msg[0] = (y[7] << 24) | 0x00800000;
	msg[15] = 65 * 8;

	crypto::sha256(msg, sha256Digest);

	for(int i = 0; i < 16; i++) {
		msg[i] = 0;
	}

	// Swap to little-endian
	for(int i = 0; i < 8; i++) {
		msg[i] = endian(sha256Digest[i]);
	}

	// Message length, little endian
	msg[8] = 0x00000080;
	msg[14] = 256;
	msg[15] = 0;

	crypto::ripemd160(msg, digest);
}



void Hash::hashPublicKeyCompressed(const unsigned int *x, const unsigned int *y, unsigned int *digest)
{
	unsigned int xLe[8];

	xLe[0] = x[7];
	xLe[1] = x[6];
	xLe[2] = x[5];
	xLe[3] = x[4];
	xLe[4] = x[3];
	xLe[5] = x[2];
	xLe[6] = x[1];
	xLe[7] = x[0];

	hashPublicKeyCompressed(xLe, y[7] & 1u, digest);
}

void Hash::hashPublicKeyCompressed(const unsigned int *xLe, unsigned int yParity, unsigned int *digest)
{
#if defined(__x86_64__) || defined(_M_X64)
	if(crypto::sha256UsesHardware()) {
		hashCompressedNi(xLe, yParity, digest);
		return;
	}
#endif
	hashCompressedSoft(xLe, yParity, digest);
}

void Hash::hashPublicKeyCompressed(const uint64_t *xLe, unsigned int yParity, unsigned int *digest)
{
	unsigned int x32[8];
	limbs64To32(x32, xLe);
	hashPublicKeyCompressed(x32, yParity, digest);
}

void Hash::hashPublicKeyCompressed2(const unsigned int *xLe0, unsigned int yParity0, unsigned int *digest0,
	const unsigned int *xLe1, unsigned int yParity1, unsigned int *digest1)
{
#if defined(__x86_64__) || defined(_M_X64)
	if(crypto::sha256UsesHardware()) {
		hashCompressedNi2(xLe0, yParity0, digest0, xLe1, yParity1, digest1);
		return;
	}
#endif
	hashCompressedSoft(xLe0, yParity0, digest0);
	hashCompressedSoft(xLe1, yParity1, digest1);
}

void Hash::hashPublicKeyCompressed2(const uint64_t *xLe0, unsigned int yParity0, unsigned int *digest0,
	const uint64_t *xLe1, unsigned int yParity1, unsigned int *digest1)
{
	unsigned int x0[8];
	unsigned int x1[8];
	limbs64To32(x0, xLe0);
	limbs64To32(x1, xLe1);
	hashPublicKeyCompressed2(x0, yParity0, digest0, x1, yParity1, digest1);
}
