#ifndef _CRYPTO_UTIL_H
#define _CRYPTO_UTIL_H

namespace crypto {

	class Rng {
		unsigned int _state[16];
		unsigned int _counter;

		void reseed();

	public:
		Rng();
		void get(unsigned char *buf, int len);
	};


	void ripemd160(unsigned int *msg, unsigned int *digest);

	/* 4 independent 64-byte blocks. msg[lane][0..15] are little-endian
	 * 32-bit words (same layout as ripemd160). digest[lane][0..4] are
	 * 5 big-endian uint32 words (same as ripemd160). Uses AVX2 when
	 * available, otherwise 4 scalar hashes. */
	void ripemd160x4(unsigned int msg[4][16], unsigned int digest[4][5]);
	void ripemd160x8(unsigned int msg[8][16], unsigned int digest[8][5]);
	void ripemd160x16(unsigned int msg[16][16], unsigned int digest[16][5]);
	/* HASH160 second half: sha[lane][0..7] are SHA-256 state words
	 * (same layout as crypto::sha256 output). Padding is implicit. */
	void ripemd160FromSha256x8(unsigned int sha[8][8], unsigned int digest[8][5]);
	void ripemd160FromSha256x16(unsigned int sha[16][8], unsigned int digest[16][5]);
	bool ripemd160UsesAvx2();
	bool ripemd160UsesAvx512();

	void sha256Init(unsigned int *digest);
	void sha256(unsigned int *msg, unsigned int *digest);
	void sha2562(unsigned int *msg0, unsigned int *digest0, unsigned int *msg1, unsigned int *digest1);
	void sha2564(unsigned int *msg0, unsigned int *digest0, unsigned int *msg1, unsigned int *digest1,
		unsigned int *msg2, unsigned int *digest2, unsigned int *msg3, unsigned int *digest3);
	/* Like sha2564, but each digest starts from the SHA-256 IV. */
	void sha2564FromIv(unsigned int *msg0, unsigned int *digest0, unsigned int *msg1, unsigned int *digest1,
		unsigned int *msg2, unsigned int *digest2, unsigned int *msg3, unsigned int *digest3);
	bool sha256UsesHardware();

	unsigned int checksum(const unsigned int *hash);
};

#endif