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
	bool ripemd160UsesAvx2();

	void sha256Init(unsigned int *digest);
	void sha256(unsigned int *msg, unsigned int *digest);
	void sha2562(unsigned int *msg0, unsigned int *digest0, unsigned int *msg1, unsigned int *digest1);
	bool sha256UsesHardware();

	unsigned int checksum(const unsigned int *hash);
};

#endif