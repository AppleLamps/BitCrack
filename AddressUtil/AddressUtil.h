#ifndef _ADDRESS_UTIL_H
#define _ADDRESS_UTIL_H

#include "secp256k1.h"

namespace Address {
	std::string fromPublicKey(const secp256k1::ecpoint &p, bool compressed = false);
	bool verifyAddress(std::string address);
};

namespace Base58 {
	std::string toBase58(const secp256k1::uint256 &x);
	secp256k1::uint256 toBigInt(const std::string &s);
	void getMinMaxFromPrefix(const std::string &prefix, secp256k1::uint256 &minValueOut, secp256k1::uint256 &maxValueOut);

	void toHash160(const std::string &s, unsigned int hash[5]);

	bool isBase58(std::string s);
};



namespace Hash {


	void hashPublicKey(const secp256k1::ecpoint &p, unsigned int *digest);
	void hashPublicKeyCompressed(const secp256k1::ecpoint &p, unsigned int *digest);

	void hashPublicKey(const unsigned int *x, const unsigned int *y, unsigned int *digest);
	void hashPublicKeyCompressed(const unsigned int *x, const unsigned int *y, unsigned int *digest);

	/* xLe: 8 little-endian uint32 limbs (v[0] = least significant). yParity: 0 or 1. */
	void hashPublicKeyCompressed(const unsigned int *xLe, unsigned int yParity, unsigned int *digest);
	/* xLe: 4 little-endian uint64 limbs. yParity: 0 or 1. */
	void hashPublicKeyCompressed(const uint64_t *xLe, unsigned int yParity, unsigned int *digest);

	void hashPublicKeyCompressed2(const unsigned int *xLe0, unsigned int yParity0, unsigned int *digest0,
		const unsigned int *xLe1, unsigned int yParity1, unsigned int *digest1);
	void hashPublicKeyCompressed2(const uint64_t *xLe0, unsigned int yParity0, unsigned int *digest0,
		const uint64_t *xLe1, unsigned int yParity1, unsigned int *digest1);

};


#endif