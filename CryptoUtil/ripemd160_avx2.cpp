/* AVX2 4-way RIPEMD-160. Hashes 4 independent 64-byte (16 LE uint32)
 * messages. Output is 5 BE uint32 words per lane, matching crypto::ripemd160.
 * Compiled with -mavx2 -mno-avx512f; runtime CPUID in ripemd160.cpp decides
 * whether to call this. */

#if defined(__x86_64__) || defined(_M_X64)

#include <stdint.h>

#if defined(__GNUC__)
#include <x86intrin.h>
#endif

#if defined(_MSC_VER)
#include <immintrin.h>
#endif

#if defined(__GNUC__)
#define RIPEMD_INLINE inline __attribute__((always_inline))
#else
#define RIPEMD_INLINE inline
#endif

typedef __m128i v4;

static const unsigned int _IV[5] = {
	0x67452301,
	0xefcdab89,
	0x98badcfe,
	0x10325476,
	0xc3d2e1f0
};

static const unsigned int _K0 = 0x5a827999;
static const unsigned int _K1 = 0x6ed9eba1;
static const unsigned int _K2 = 0x8f1bbcdc;
static const unsigned int _K3 = 0xa953fd4e;

static const unsigned int _K4 = 0x7a6d76e9;
static const unsigned int _K5 = 0x6d703ef3;
static const unsigned int _K6 = 0x5c4dd124;
static const unsigned int _K7 = 0x50a28be6;

static RIPEMD_INLINE v4 vrotl(v4 x, int n)
{
	return _mm_or_si128(_mm_slli_epi32(x, n), _mm_srli_epi32(x, 32 - n));
}

static RIPEMD_INLINE v4 F(v4 x, v4 y, v4 z)
{
	return _mm_xor_si128(_mm_xor_si128(x, y), z);
}

static RIPEMD_INLINE v4 G(v4 x, v4 y, v4 z)
{
	return _mm_or_si128(_mm_and_si128(x, y), _mm_andnot_si128(x, z));
}

static RIPEMD_INLINE v4 H(v4 x, v4 y, v4 z)
{
	return _mm_xor_si128(_mm_or_si128(x, _mm_xor_si128(y, _mm_set1_epi32(-1))), z);
}

static RIPEMD_INLINE v4 I(v4 x, v4 y, v4 z)
{
	return _mm_or_si128(_mm_and_si128(x, z), _mm_andnot_si128(z, y));
}

static RIPEMD_INLINE v4 J(v4 x, v4 y, v4 z)
{
	return _mm_xor_si128(x, _mm_or_si128(y, _mm_xor_si128(z, _mm_set1_epi32(-1))));
}

static RIPEMD_INLINE void FF(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(F(b, c, d), x));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE void GG(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(G(b, c, d), _mm_add_epi32(x, _mm_set1_epi32((int)_K0))));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE void HH(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(H(b, c, d), _mm_add_epi32(x, _mm_set1_epi32((int)_K1))));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE void II(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(I(b, c, d), _mm_add_epi32(x, _mm_set1_epi32((int)_K2))));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE void JJ(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(J(b, c, d), _mm_add_epi32(x, _mm_set1_epi32((int)_K3))));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE void FFF(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(F(b, c, d), x));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE void GGG(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(G(b, c, d), _mm_add_epi32(x, _mm_set1_epi32((int)_K4))));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE void HHH(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(H(b, c, d), _mm_add_epi32(x, _mm_set1_epi32((int)_K5))));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE void III(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(I(b, c, d), _mm_add_epi32(x, _mm_set1_epi32((int)_K6))));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE void JJJ(v4 &a, v4 &b, v4 &c, v4 &d, v4 &e, v4 x, int s)
{
	a = _mm_add_epi32(a, _mm_add_epi32(J(b, c, d), _mm_add_epi32(x, _mm_set1_epi32((int)_K7))));
	a = _mm_add_epi32(vrotl(a, s), e);
	c = vrotl(c, 10);
}

static RIPEMD_INLINE v4 vendian(v4 x)
{
	return _mm_shuffle_epi8(x, _mm_setr_epi8(
		3, 2, 1, 0,
		7, 6, 5, 4,
		11, 10, 9, 8,
		15, 14, 13, 12));
}

#if defined(__GNUC__)
__attribute__((target("avx2"), noinline))
#endif
static void ripemd160_4x(const unsigned int *msg, unsigned int *digest)
{
	v4 x[16];
	for(int i = 0; i < 16; i++) {
		x[i] = _mm_setr_epi32(
			(int)msg[i],
			(int)msg[16 + i],
			(int)msg[32 + i],
			(int)msg[48 + i]);
	}

	const v4 iv0 = _mm_set1_epi32((int)_IV[0]);
	const v4 iv1 = _mm_set1_epi32((int)_IV[1]);
	const v4 iv2 = _mm_set1_epi32((int)_IV[2]);
	const v4 iv3 = _mm_set1_epi32((int)_IV[3]);
	const v4 iv4 = _mm_set1_epi32((int)_IV[4]);

	v4 a1 = iv0;
	v4 b1 = iv1;
	v4 c1 = iv2;
	v4 d1 = iv3;
	v4 e1 = iv4;

	v4 a2 = iv0;
	v4 b2 = iv1;
	v4 c2 = iv2;
	v4 d2 = iv3;
	v4 e2 = iv4;

	/* round 1 */
	FF(a1, b1, c1, d1, e1, x[0], 11);
	FF(e1, a1, b1, c1, d1, x[1], 14);
	FF(d1, e1, a1, b1, c1, x[2], 15);
	FF(c1, d1, e1, a1, b1, x[3], 12);
	FF(b1, c1, d1, e1, a1, x[4], 5);
	FF(a1, b1, c1, d1, e1, x[5], 8);
	FF(e1, a1, b1, c1, d1, x[6], 7);
	FF(d1, e1, a1, b1, c1, x[7], 9);
	FF(c1, d1, e1, a1, b1, x[8], 11);
	FF(b1, c1, d1, e1, a1, x[9], 13);
	FF(a1, b1, c1, d1, e1, x[10], 14);
	FF(e1, a1, b1, c1, d1, x[11], 15);
	FF(d1, e1, a1, b1, c1, x[12], 6);
	FF(c1, d1, e1, a1, b1, x[13], 7);
	FF(b1, c1, d1, e1, a1, x[14], 9);
	FF(a1, b1, c1, d1, e1, x[15], 8);

	/* round 2 */
	GG(e1, a1, b1, c1, d1, x[7], 7);
	GG(d1, e1, a1, b1, c1, x[4], 6);
	GG(c1, d1, e1, a1, b1, x[13], 8);
	GG(b1, c1, d1, e1, a1, x[1], 13);
	GG(a1, b1, c1, d1, e1, x[10], 11);
	GG(e1, a1, b1, c1, d1, x[6], 9);
	GG(d1, e1, a1, b1, c1, x[15], 7);
	GG(c1, d1, e1, a1, b1, x[3], 15);
	GG(b1, c1, d1, e1, a1, x[12], 7);
	GG(a1, b1, c1, d1, e1, x[0], 12);
	GG(e1, a1, b1, c1, d1, x[9], 15);
	GG(d1, e1, a1, b1, c1, x[5], 9);
	GG(c1, d1, e1, a1, b1, x[2], 11);
	GG(b1, c1, d1, e1, a1, x[14], 7);
	GG(a1, b1, c1, d1, e1, x[11], 13);
	GG(e1, a1, b1, c1, d1, x[8], 12);

	/* round 3 */
	HH(d1, e1, a1, b1, c1, x[3], 11);
	HH(c1, d1, e1, a1, b1, x[10], 13);
	HH(b1, c1, d1, e1, a1, x[14], 6);
	HH(a1, b1, c1, d1, e1, x[4], 7);
	HH(e1, a1, b1, c1, d1, x[9], 14);
	HH(d1, e1, a1, b1, c1, x[15], 9);
	HH(c1, d1, e1, a1, b1, x[8], 13);
	HH(b1, c1, d1, e1, a1, x[1], 15);
	HH(a1, b1, c1, d1, e1, x[2], 14);
	HH(e1, a1, b1, c1, d1, x[7], 8);
	HH(d1, e1, a1, b1, c1, x[0], 13);
	HH(c1, d1, e1, a1, b1, x[6], 6);
	HH(b1, c1, d1, e1, a1, x[13], 5);
	HH(a1, b1, c1, d1, e1, x[11], 12);
	HH(e1, a1, b1, c1, d1, x[5], 7);
	HH(d1, e1, a1, b1, c1, x[12], 5);

	/* round 4 */
	II(c1, d1, e1, a1, b1, x[1], 11);
	II(b1, c1, d1, e1, a1, x[9], 12);
	II(a1, b1, c1, d1, e1, x[11], 14);
	II(e1, a1, b1, c1, d1, x[10], 15);
	II(d1, e1, a1, b1, c1, x[0], 14);
	II(c1, d1, e1, a1, b1, x[8], 15);
	II(b1, c1, d1, e1, a1, x[12], 9);
	II(a1, b1, c1, d1, e1, x[4], 8);
	II(e1, a1, b1, c1, d1, x[13], 9);
	II(d1, e1, a1, b1, c1, x[3], 14);
	II(c1, d1, e1, a1, b1, x[7], 5);
	II(b1, c1, d1, e1, a1, x[15], 6);
	II(a1, b1, c1, d1, e1, x[14], 8);
	II(e1, a1, b1, c1, d1, x[5], 6);
	II(d1, e1, a1, b1, c1, x[6], 5);
	II(c1, d1, e1, a1, b1, x[2], 12);

	/* round 5 */
	JJ(b1, c1, d1, e1, a1, x[4], 9);
	JJ(a1, b1, c1, d1, e1, x[0], 15);
	JJ(e1, a1, b1, c1, d1, x[5], 5);
	JJ(d1, e1, a1, b1, c1, x[9], 11);
	JJ(c1, d1, e1, a1, b1, x[7], 6);
	JJ(b1, c1, d1, e1, a1, x[12], 8);
	JJ(a1, b1, c1, d1, e1, x[2], 13);
	JJ(e1, a1, b1, c1, d1, x[10], 12);
	JJ(d1, e1, a1, b1, c1, x[14], 5);
	JJ(c1, d1, e1, a1, b1, x[1], 12);
	JJ(b1, c1, d1, e1, a1, x[3], 13);
	JJ(a1, b1, c1, d1, e1, x[8], 14);
	JJ(e1, a1, b1, c1, d1, x[11], 11);
	JJ(d1, e1, a1, b1, c1, x[6], 8);
	JJ(c1, d1, e1, a1, b1, x[15], 5);
	JJ(b1, c1, d1, e1, a1, x[13], 6);


	/* parallel round 1 */
	JJJ(a2, b2, c2, d2, e2, x[5], 8);
	JJJ(e2, a2, b2, c2, d2, x[14], 9);
	JJJ(d2, e2, a2, b2, c2, x[7], 9);
	JJJ(c2, d2, e2, a2, b2, x[0], 11);
	JJJ(b2, c2, d2, e2, a2, x[9], 13);
	JJJ(a2, b2, c2, d2, e2, x[2], 15);
	JJJ(e2, a2, b2, c2, d2, x[11], 15);
	JJJ(d2, e2, a2, b2, c2, x[4], 5);
	JJJ(c2, d2, e2, a2, b2, x[13], 7);
	JJJ(b2, c2, d2, e2, a2, x[6], 7);
	JJJ(a2, b2, c2, d2, e2, x[15], 8);
	JJJ(e2, a2, b2, c2, d2, x[8], 11);
	JJJ(d2, e2, a2, b2, c2, x[1], 14);
	JJJ(c2, d2, e2, a2, b2, x[10], 14);
	JJJ(b2, c2, d2, e2, a2, x[3], 12);
	JJJ(a2, b2, c2, d2, e2, x[12], 6);

	/* parallel round 2 */
	III(e2, a2, b2, c2, d2, x[6], 9);
	III(d2, e2, a2, b2, c2, x[11], 13);
	III(c2, d2, e2, a2, b2, x[3], 15);
	III(b2, c2, d2, e2, a2, x[7], 7);
	III(a2, b2, c2, d2, e2, x[0], 12);
	III(e2, a2, b2, c2, d2, x[13], 8);
	III(d2, e2, a2, b2, c2, x[5], 9);
	III(c2, d2, e2, a2, b2, x[10], 11);
	III(b2, c2, d2, e2, a2, x[14], 7);
	III(a2, b2, c2, d2, e2, x[15], 7);
	III(e2, a2, b2, c2, d2, x[8], 12);
	III(d2, e2, a2, b2, c2, x[12], 7);
	III(c2, d2, e2, a2, b2, x[4], 6);
	III(b2, c2, d2, e2, a2, x[9], 15);
	III(a2, b2, c2, d2, e2, x[1], 13);
	III(e2, a2, b2, c2, d2, x[2], 11);

	/* parallel round 3 */
	HHH(d2, e2, a2, b2, c2, x[15], 9);
	HHH(c2, d2, e2, a2, b2, x[5], 7);
	HHH(b2, c2, d2, e2, a2, x[1], 15);
	HHH(a2, b2, c2, d2, e2, x[3], 11);
	HHH(e2, a2, b2, c2, d2, x[7], 8);
	HHH(d2, e2, a2, b2, c2, x[14], 6);
	HHH(c2, d2, e2, a2, b2, x[6], 6);
	HHH(b2, c2, d2, e2, a2, x[9], 14);
	HHH(a2, b2, c2, d2, e2, x[11], 12);
	HHH(e2, a2, b2, c2, d2, x[8], 13);
	HHH(d2, e2, a2, b2, c2, x[12], 5);
	HHH(c2, d2, e2, a2, b2, x[2], 14);
	HHH(b2, c2, d2, e2, a2, x[10], 13);
	HHH(a2, b2, c2, d2, e2, x[0], 13);
	HHH(e2, a2, b2, c2, d2, x[4], 7);
	HHH(d2, e2, a2, b2, c2, x[13], 5);

	/* parallel round 4 */
	GGG(c2, d2, e2, a2, b2, x[8], 15);
	GGG(b2, c2, d2, e2, a2, x[6], 5);
	GGG(a2, b2, c2, d2, e2, x[4], 8);
	GGG(e2, a2, b2, c2, d2, x[1], 11);
	GGG(d2, e2, a2, b2, c2, x[3], 14);
	GGG(c2, d2, e2, a2, b2, x[11], 14);
	GGG(b2, c2, d2, e2, a2, x[15], 6);
	GGG(a2, b2, c2, d2, e2, x[0], 14);
	GGG(e2, a2, b2, c2, d2, x[5], 6);
	GGG(d2, e2, a2, b2, c2, x[12], 9);
	GGG(c2, d2, e2, a2, b2, x[2], 12);
	GGG(b2, c2, d2, e2, a2, x[13], 9);
	GGG(a2, b2, c2, d2, e2, x[9], 12);
	GGG(e2, a2, b2, c2, d2, x[7], 5);
	GGG(d2, e2, a2, b2, c2, x[10], 15);
	GGG(c2, d2, e2, a2, b2, x[14], 8);

	/* parallel round 5 */
	FFF(b2, c2, d2, e2, a2, x[12], 8);
	FFF(a2, b2, c2, d2, e2, x[15], 5);
	FFF(e2, a2, b2, c2, d2, x[10], 12);
	FFF(d2, e2, a2, b2, c2, x[4], 9);
	FFF(c2, d2, e2, a2, b2, x[1], 12);
	FFF(b2, c2, d2, e2, a2, x[5], 5);
	FFF(a2, b2, c2, d2, e2, x[8], 14);
	FFF(e2, a2, b2, c2, d2, x[7], 6);
	FFF(d2, e2, a2, b2, c2, x[6], 8);
	FFF(c2, d2, e2, a2, b2, x[2], 13);
	FFF(b2, c2, d2, e2, a2, x[13], 6);
	FFF(a2, b2, c2, d2, e2, x[14], 5);
	FFF(e2, a2, b2, c2, d2, x[0], 15);
	FFF(d2, e2, a2, b2, c2, x[3], 13);
	FFF(c2, d2, e2, a2, b2, x[9], 11);
	FFF(b2, c2, d2, e2, a2, x[11], 11);


	v4 d0 = vendian(_mm_add_epi32(_mm_add_epi32(iv1, c1), d2));
	v4 d1o = vendian(_mm_add_epi32(_mm_add_epi32(iv2, d1), e2));
	v4 d2o = vendian(_mm_add_epi32(_mm_add_epi32(iv3, e1), a2));
	v4 d3o = vendian(_mm_add_epi32(_mm_add_epi32(iv4, a1), b2));
	v4 d4o = vendian(_mm_add_epi32(_mm_add_epi32(iv0, b1), c2));

	alignas(16) unsigned int tmp[4];
	_mm_store_si128((__m128i *)tmp, d0);
	digest[0] = tmp[0]; digest[5] = tmp[1]; digest[10] = tmp[2]; digest[15] = tmp[3];
	_mm_store_si128((__m128i *)tmp, d1o);
	digest[1] = tmp[0]; digest[6] = tmp[1]; digest[11] = tmp[2]; digest[16] = tmp[3];
	_mm_store_si128((__m128i *)tmp, d2o);
	digest[2] = tmp[0]; digest[7] = tmp[1]; digest[12] = tmp[2]; digest[17] = tmp[3];
	_mm_store_si128((__m128i *)tmp, d3o);
	digest[3] = tmp[0]; digest[8] = tmp[1]; digest[13] = tmp[2]; digest[18] = tmp[3];
	_mm_store_si128((__m128i *)tmp, d4o);
	digest[4] = tmp[0]; digest[9] = tmp[1]; digest[14] = tmp[2]; digest[19] = tmp[3];
}

extern "C" {
#if defined(__GNUC__)
__attribute__((target("avx2")))
#endif
void crypto_ripemd160_avx2(unsigned int *msg, unsigned int *digest)
{
	ripemd160_4x(msg, digest);
}
}

#endif
