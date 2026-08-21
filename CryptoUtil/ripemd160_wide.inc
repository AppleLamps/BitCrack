/* Parameterized N-way RIPEMD-160. Include after defining RIPEMD_LANES as 8 or 16.
 * Compiled in dedicated TUs so AVX2 and AVX-512 stay isolated from SHA-NI. */

#if RIPEMD_LANES == 8
typedef __m256i rpv;
#define RP_ADD _mm256_add_epi32
#define RP_XOR _mm256_xor_si256
#define RP_OR  _mm256_or_si256
#define RP_AND _mm256_and_si256
#define RP_ANDNOT _mm256_andnot_si256
#define RP_SLLI _mm256_slli_epi32
#define RP_SRLI _mm256_srli_epi32
#define RP_SET1 _mm256_set1_epi32
#define RP_ZERO _mm256_setzero_si256()
#define RP_STORE(ptr, v) _mm256_store_si256((__m256i *)(ptr), (v))
#define RP_ALIGN 32
static RIPEMD_INLINE rpv rp_rotl(rpv x, int n)
{
	return RP_OR(RP_SLLI(x, n), RP_SRLI(x, 32 - n));
}
static RIPEMD_INLINE rpv rp_bswap(rpv x)
{
	const rpv mask = _mm256_setr_epi8(
		3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
		3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
	return _mm256_shuffle_epi8(x, mask);
}
static RIPEMD_INLINE rpv rp_load_word(const unsigned int *base, int stride, int word)
{
	return _mm256_setr_epi32(
		(int)base[0 * stride + word],
		(int)base[1 * stride + word],
		(int)base[2 * stride + word],
		(int)base[3 * stride + word],
		(int)base[4 * stride + word],
		(int)base[5 * stride + word],
		(int)base[6 * stride + word],
		(int)base[7 * stride + word]);
}
#elif RIPEMD_LANES == 16
typedef __m512i rpv;
#define RP_ADD _mm512_add_epi32
#define RP_XOR _mm512_xor_si512
#define RP_OR  _mm512_or_si512
#define RP_AND _mm512_and_si512
#define RP_ANDNOT _mm512_andnot_si512
#define RP_SET1 _mm512_set1_epi32
#define RP_ZERO _mm512_setzero_si512()
#define RP_STORE(ptr, v) _mm512_store_si512((__m512i *)(ptr), (v))
#define RP_ALIGN 64
static RIPEMD_INLINE rpv rp_rotl(rpv x, int n)
{
	return _mm512_rol_epi32(x, n);
}
static RIPEMD_INLINE rpv rp_bswap(rpv x)
{
	const __m128i m128 = _mm_setr_epi8(
		3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
	return _mm512_shuffle_epi8(x, _mm512_broadcast_i32x4(m128));
}
static RIPEMD_INLINE rpv rp_load_word(const unsigned int *base, int stride, int word)
{
	return _mm512_setr_epi32(
		(int)base[0 * stride + word],
		(int)base[1 * stride + word],
		(int)base[2 * stride + word],
		(int)base[3 * stride + word],
		(int)base[4 * stride + word],
		(int)base[5 * stride + word],
		(int)base[6 * stride + word],
		(int)base[7 * stride + word],
		(int)base[8 * stride + word],
		(int)base[9 * stride + word],
		(int)base[10 * stride + word],
		(int)base[11 * stride + word],
		(int)base[12 * stride + word],
		(int)base[13 * stride + word],
		(int)base[14 * stride + word],
		(int)base[15 * stride + word]);
}
#else
#error "RIPEMD_LANES must be 8 or 16"
#endif

static const unsigned int _IV_W[5] = {
	0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0
};

static const unsigned int _K0 = 0x5a827999;
static const unsigned int _K1 = 0x6ed9eba1;
static const unsigned int _K2 = 0x8f1bbcdc;
static const unsigned int _K3 = 0xa953fd4e;
static const unsigned int _K4 = 0x7a6d76e9;
static const unsigned int _K5 = 0x6d703ef3;
static const unsigned int _K6 = 0x5c4dd124;
static const unsigned int _K7 = 0x50a28be6;

static RIPEMD_INLINE rpv F(rpv x, rpv y, rpv z)
{
	return RP_XOR(RP_XOR(x, y), z);
}
static RIPEMD_INLINE rpv G(rpv x, rpv y, rpv z)
{
	return RP_OR(RP_AND(x, y), RP_ANDNOT(x, z));
}
static RIPEMD_INLINE rpv H(rpv x, rpv y, rpv z)
{
	return RP_XOR(RP_OR(x, RP_XOR(y, RP_SET1(-1))), z);
}
static RIPEMD_INLINE rpv I(rpv x, rpv y, rpv z)
{
	return RP_OR(RP_AND(x, z), RP_ANDNOT(z, y));
}
static RIPEMD_INLINE rpv J(rpv x, rpv y, rpv z)
{
	return RP_XOR(x, RP_OR(y, RP_XOR(z, RP_SET1(-1))));
}

static RIPEMD_INLINE void FF(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(F(b, c, d), x));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}
static RIPEMD_INLINE void GG(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(G(b, c, d), RP_ADD(x, RP_SET1((int)_K0))));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}
static RIPEMD_INLINE void HH(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(H(b, c, d), RP_ADD(x, RP_SET1((int)_K1))));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}
static RIPEMD_INLINE void II(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(I(b, c, d), RP_ADD(x, RP_SET1((int)_K2))));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}
static RIPEMD_INLINE void JJ(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(J(b, c, d), RP_ADD(x, RP_SET1((int)_K3))));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}
static RIPEMD_INLINE void FFF(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(F(b, c, d), x));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}
static RIPEMD_INLINE void GGG(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(G(b, c, d), RP_ADD(x, RP_SET1((int)_K4))));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}
static RIPEMD_INLINE void HHH(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(H(b, c, d), RP_ADD(x, RP_SET1((int)_K5))));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}
static RIPEMD_INLINE void III(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(I(b, c, d), RP_ADD(x, RP_SET1((int)_K6))));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}
static RIPEMD_INLINE void JJJ(rpv &a, rpv &b, rpv &c, rpv &d, rpv &e, rpv x, int s)
{
	a = RP_ADD(a, RP_ADD(J(b, c, d), RP_ADD(x, RP_SET1((int)_K7))));
	a = RP_ADD(rp_rotl(a, s), e);
	c = rp_rotl(c, 10);
}

static RIPEMD_INLINE void rp_finish(rpv iv0, rpv iv1, rpv iv2, rpv iv3, rpv iv4,
	rpv a1, rpv b1, rpv c1, rpv d1, rpv e1,
	rpv a2, rpv b2, rpv c2, rpv d2, rpv e2,
	unsigned int *digest)
{
	rpv o0 = rp_bswap(RP_ADD(RP_ADD(iv1, c1), d2));
	rpv o1 = rp_bswap(RP_ADD(RP_ADD(iv2, d1), e2));
	rpv o2 = rp_bswap(RP_ADD(RP_ADD(iv3, e1), a2));
	rpv o3 = rp_bswap(RP_ADD(RP_ADD(iv4, a1), b2));
	rpv o4 = rp_bswap(RP_ADD(RP_ADD(iv0, b1), c2));

	alignas(RP_ALIGN) unsigned int tmp[RIPEMD_LANES];
	RP_STORE(tmp, o0);
	for(int lane = 0; lane < RIPEMD_LANES; lane++) digest[lane * 5 + 0] = tmp[lane];
	RP_STORE(tmp, o1);
	for(int lane = 0; lane < RIPEMD_LANES; lane++) digest[lane * 5 + 1] = tmp[lane];
	RP_STORE(tmp, o2);
	for(int lane = 0; lane < RIPEMD_LANES; lane++) digest[lane * 5 + 2] = tmp[lane];
	RP_STORE(tmp, o3);
	for(int lane = 0; lane < RIPEMD_LANES; lane++) digest[lane * 5 + 3] = tmp[lane];
	RP_STORE(tmp, o4);
	for(int lane = 0; lane < RIPEMD_LANES; lane++) digest[lane * 5 + 4] = tmp[lane];
}

static void ripemd160_core(rpv x[16], unsigned int *digest)
{
	const rpv iv0 = RP_SET1((int)_IV_W[0]);
	const rpv iv1 = RP_SET1((int)_IV_W[1]);
	const rpv iv2 = RP_SET1((int)_IV_W[2]);
	const rpv iv3 = RP_SET1((int)_IV_W[3]);
	const rpv iv4 = RP_SET1((int)_IV_W[4]);

	rpv a1 = iv0, b1 = iv1, c1 = iv2, d1 = iv3, e1 = iv4;
	rpv a2 = iv0, b2 = iv1, c2 = iv2, d2 = iv3, e2 = iv4;

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

	rp_finish(iv0, iv1, iv2, iv3, iv4, a1, b1, c1, d1, e1, a2, b2, c2, d2, e2, digest);
}

static void ripemd160_Nx(const unsigned int *msg, unsigned int *digest)
{
	rpv x[16];
	for(int i = 0; i < 16; i++) {
		x[i] = rp_load_word(msg, 16, i);
	}
	ripemd160_core(x, digest);
}

static void ripemd160_fromsha_Nx(const unsigned int *sha, unsigned int *digest)
{
	rpv x[16];
	for(int i = 0; i < 8; i++) {
		x[i] = rp_bswap(rp_load_word(sha, 8, i));
	}
	x[8] = RP_SET1(0x00000080);
	x[9] = RP_ZERO;
	x[10] = RP_ZERO;
	x[11] = RP_ZERO;
	x[12] = RP_ZERO;
	x[13] = RP_ZERO;
	x[14] = RP_SET1(256);
	x[15] = RP_ZERO;
	ripemd160_core(x, digest);
}

#undef RP_ADD
#undef RP_XOR
#undef RP_OR
#undef RP_AND
#undef RP_ANDNOT
#undef RP_SET1
#undef RP_ZERO
#undef RP_STORE
#undef RP_ALIGN
#if RIPEMD_LANES == 8
#undef RP_SLLI
#undef RP_SRLI
#endif
