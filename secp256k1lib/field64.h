#ifndef _SECP256K1_FIELD64_H
#define _SECP256K1_FIELD64_H

/* 64-bit secp256k1 field arithmetic (P = 2^256 - 0x1000003D1).
 * Used on hosts with 128-bit integers. Operands are little-endian. */

#include <stdint.h>
#include <string.h>

#if defined(__GNUC__)
#define FE_INLINE inline __attribute__((always_inline))
#else
#define FE_INLINE inline
#endif

struct fe {
	uint64_t n[4];
};

static const uint64_t FE_P0 = 0xFFFFFFFEFFFFFC2FULL;
static const uint64_t FE_C  = 0x1000003D1ULL;

static FE_INLINE void fe_load(fe &r, const secp256k1::uint256 &a)
{
	r.n[0] = (uint64_t)a.v[0] | ((uint64_t)a.v[1] << 32);
	r.n[1] = (uint64_t)a.v[2] | ((uint64_t)a.v[3] << 32);
	r.n[2] = (uint64_t)a.v[4] | ((uint64_t)a.v[5] << 32);
	r.n[3] = (uint64_t)a.v[6] | ((uint64_t)a.v[7] << 32);
}

static FE_INLINE void fe_store(secp256k1::uint256 &r, const fe &a)
{
	r.v[0] = (unsigned int)a.n[0];
	r.v[1] = (unsigned int)(a.n[0] >> 32);
	r.v[2] = (unsigned int)a.n[1];
	r.v[3] = (unsigned int)(a.n[1] >> 32);
	r.v[4] = (unsigned int)a.n[2];
	r.v[5] = (unsigned int)(a.n[2] >> 32);
	r.v[6] = (unsigned int)a.n[3];
	r.v[7] = (unsigned int)(a.n[3] >> 32);
}

static FE_INLINE void fe_set1(fe &r)
{
	r.n[0] = 1;
	r.n[1] = 0;
	r.n[2] = 0;
	r.n[3] = 0;
}

static FE_INLINE bool fe_eq(const fe &a, const fe &b)
{
	return a.n[0] == b.n[0] && a.n[1] == b.n[1] && a.n[2] == b.n[2] && a.n[3] == b.n[3];
}

static FE_INLINE bool fe_is_inf(const fe &x, const fe &y)
{
	return x.n[0] == ~0ULL && x.n[1] == ~0ULL && x.n[2] == ~0ULL && x.n[3] == ~0ULL &&
	       y.n[0] == ~0ULL && y.n[1] == ~0ULL && y.n[2] == ~0ULL && y.n[3] == ~0ULL;
}

static FE_INLINE bool fe_ge_p(const fe &a)
{
	return a.n[3] == ~0ULL && a.n[2] == ~0ULL && a.n[1] == ~0ULL && a.n[0] >= FE_P0;
}

static FE_INLINE void fe_add_c(fe &r)
{
	unsigned __int128 t = (unsigned __int128)r.n[0] + FE_C;
	r.n[0] = (uint64_t)t;
	t = (unsigned __int128)r.n[1] + (t >> 64);
	r.n[1] = (uint64_t)t;
	t = (unsigned __int128)r.n[2] + (t >> 64);
	r.n[2] = (uint64_t)t;
	t = (unsigned __int128)r.n[3] + (t >> 64);
	r.n[3] = (uint64_t)t;
}

static FE_INLINE void fe_sub_c(fe &r)
{
	unsigned __int128 t = (unsigned __int128)r.n[0] - FE_C;
	r.n[0] = (uint64_t)t;
	t = (unsigned __int128)r.n[1] - ((t >> 64) & 1);
	r.n[1] = (uint64_t)t;
	t = (unsigned __int128)r.n[2] - ((t >> 64) & 1);
	r.n[2] = (uint64_t)t;
	t = (unsigned __int128)r.n[3] - ((t >> 64) & 1);
	r.n[3] = (uint64_t)t;
}

static FE_INLINE void fe_add(fe &r, const fe &a, const fe &b)
{
	unsigned __int128 t = (unsigned __int128)a.n[0] + b.n[0];
	r.n[0] = (uint64_t)t;
	t = (unsigned __int128)a.n[1] + b.n[1] + (t >> 64);
	r.n[1] = (uint64_t)t;
	t = (unsigned __int128)a.n[2] + b.n[2] + (t >> 64);
	r.n[2] = (uint64_t)t;
	t = (unsigned __int128)a.n[3] + b.n[3] + (t >> 64);
	r.n[3] = (uint64_t)t;
	uint64_t carry = (uint64_t)(t >> 64);

	if(carry || fe_ge_p(r)) {
		fe_add_c(r);
	}
}

static FE_INLINE void fe_sub(fe &r, const fe &a, const fe &b)
{
	unsigned __int128 t = (unsigned __int128)a.n[0] - b.n[0];
	r.n[0] = (uint64_t)t;
	t = (unsigned __int128)a.n[1] - b.n[1] - ((t >> 64) & 1);
	r.n[1] = (uint64_t)t;
	t = (unsigned __int128)a.n[2] - b.n[2] - ((t >> 64) & 1);
	r.n[2] = (uint64_t)t;
	t = (unsigned __int128)a.n[3] - b.n[3] - ((t >> 64) & 1);
	r.n[3] = (uint64_t)t;
	uint64_t borrow = (uint64_t)((t >> 64) & 1);

	if(borrow) {
		fe_sub_c(r);
	}
}

static FE_INLINE void fe_mul_wide(uint64_t z[8], const uint64_t a[4], const uint64_t b[4])
{
	const uint64_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
	const uint64_t b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3];
	unsigned __int128 m, c;
	uint64_t t1, t2, t3, t4, t5, t6;

	m = (unsigned __int128)a0 * b0;
	z[0] = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a0 * b1 + c;
	t1 = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a0 * b2 + c;
	t2 = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a0 * b3 + c;
	t3 = (uint64_t)m;
	t4 = (uint64_t)(m >> 64);

	m = (unsigned __int128)a1 * b0 + t1;
	z[1] = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a1 * b1 + t2 + c;
	t2 = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a1 * b2 + t3 + c;
	t3 = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a1 * b3 + t4 + c;
	t4 = (uint64_t)m;
	t5 = (uint64_t)(m >> 64);

	m = (unsigned __int128)a2 * b0 + t2;
	z[2] = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a2 * b1 + t3 + c;
	t3 = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a2 * b2 + t4 + c;
	t4 = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a2 * b3 + t5 + c;
	t5 = (uint64_t)m;
	t6 = (uint64_t)(m >> 64);

	m = (unsigned __int128)a3 * b0 + t3;
	z[3] = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a3 * b1 + t4 + c;
	z[4] = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a3 * b2 + t5 + c;
	z[5] = (uint64_t)m;
	c = m >> 64;

	m = (unsigned __int128)a3 * b3 + t6 + c;
	z[6] = (uint64_t)m;
	z[7] = (uint64_t)(m >> 64);
}

static FE_INLINE void fe_reduce512(fe &r, const uint64_t p[8])
{
	/* lo + hi * 0x1000003D1, then fold remaining carry the same way. */
	unsigned __int128 c;

	c = (unsigned __int128)p[0] + (unsigned __int128)p[4] * FE_C;
	r.n[0] = (uint64_t)c;
	c >>= 64;

	c += (unsigned __int128)p[1] + (unsigned __int128)p[5] * FE_C;
	r.n[1] = (uint64_t)c;
	c >>= 64;

	c += (unsigned __int128)p[2] + (unsigned __int128)p[6] * FE_C;
	r.n[2] = (uint64_t)c;
	c >>= 64;

	c += (unsigned __int128)p[3] + (unsigned __int128)p[7] * FE_C;
	r.n[3] = (uint64_t)c;
	c >>= 64;

	while(c) {
		unsigned __int128 t = (unsigned __int128)r.n[0] + c * FE_C;
		r.n[0] = (uint64_t)t;
		t >>= 64;
		t += r.n[1];
		r.n[1] = (uint64_t)t;
		t >>= 64;
		t += r.n[2];
		r.n[2] = (uint64_t)t;
		t >>= 64;
		t += r.n[3];
		r.n[3] = (uint64_t)t;
		c = t >> 64;
	}

	if(fe_ge_p(r)) {
		fe_add_c(r);
	}
}

static FE_INLINE void fe_mul(fe &r, const fe &a, const fe &b)
{
	uint64_t p[8];
	fe_mul_wide(p, a.n, b.n);
	fe_reduce512(r, p);
}

/* Add x*y (or 2*x*y) into a 192-bit accumulator (t: low 128, h: high 64). */
static FE_INLINE void fe_acc_mul(unsigned __int128 &t, uint64_t &h, uint64_t x, uint64_t y)
{
	unsigned __int128 p = (unsigned __int128)x * y;
	unsigned __int128 s = t + p;
	h += (s < t);
	t = s;
}

static FE_INLINE void fe_acc_mul2(unsigned __int128 &t, uint64_t &h, uint64_t x, uint64_t y)
{
	unsigned __int128 p = (unsigned __int128)x * y;
	uint64_t e = (uint64_t)(p >> 127);
	p <<= 1;
	unsigned __int128 s = t + p;
	h += e + (s < t);
	t = s;
}

static FE_INLINE void fe_acc_extract(uint64_t &z, unsigned __int128 &t, uint64_t &h)
{
	z = (uint64_t)t;
	t = (t >> 64) | ((unsigned __int128)h << 64);
	h = 0;
}

/* Schoolbook square: diagonals + 2*cross. Loads limbs first so fe_sqr(r, r) is safe. */
static FE_INLINE void fe_sqr_wide(uint64_t z[8], const uint64_t a[4])
{
	const uint64_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
	unsigned __int128 t;
	uint64_t h = 0;

	t = (unsigned __int128)a0 * a0;
	fe_acc_extract(z[0], t, h);

	fe_acc_mul2(t, h, a0, a1);
	fe_acc_extract(z[1], t, h);

	fe_acc_mul2(t, h, a0, a2);
	fe_acc_mul(t, h, a1, a1);
	fe_acc_extract(z[2], t, h);

	fe_acc_mul2(t, h, a0, a3);
	fe_acc_mul2(t, h, a1, a2);
	fe_acc_extract(z[3], t, h);

	fe_acc_mul2(t, h, a1, a3);
	fe_acc_mul(t, h, a2, a2);
	fe_acc_extract(z[4], t, h);

	fe_acc_mul2(t, h, a2, a3);
	fe_acc_extract(z[5], t, h);

	fe_acc_mul(t, h, a3, a3);
	fe_acc_extract(z[6], t, h);

	z[7] = (uint64_t)t;
}

static FE_INLINE void fe_sqr(fe &r, const fe &a)
{
	uint64_t p[8];
	fe_sqr_wide(p, a.n);
	fe_reduce512(r, p);
}

/* Fermat inverse a^(P-2) via Peter Dettman's addition chain (libsecp256k1). */
static inline void fe_inv(fe &r, const fe &a)
{
	fe x2, x3, x6, x9, x11, x22, x44, x88, x176, x220, x223, t1;
	int j;

	fe_sqr(x2, a);
	fe_mul(x2, x2, a);

	fe_sqr(x3, x2);
	fe_mul(x3, x3, a);

	x6 = x3;
	for(j = 0; j < 3; j++) {
		fe_sqr(x6, x6);
	}
	fe_mul(x6, x6, x3);

	x9 = x6;
	for(j = 0; j < 3; j++) {
		fe_sqr(x9, x9);
	}
	fe_mul(x9, x9, x3);

	x11 = x9;
	for(j = 0; j < 2; j++) {
		fe_sqr(x11, x11);
	}
	fe_mul(x11, x11, x2);

	x22 = x11;
	for(j = 0; j < 11; j++) {
		fe_sqr(x22, x22);
	}
	fe_mul(x22, x22, x11);

	x44 = x22;
	for(j = 0; j < 22; j++) {
		fe_sqr(x44, x44);
	}
	fe_mul(x44, x44, x22);

	x88 = x44;
	for(j = 0; j < 44; j++) {
		fe_sqr(x88, x88);
	}
	fe_mul(x88, x88, x44);

	x176 = x88;
	for(j = 0; j < 88; j++) {
		fe_sqr(x176, x176);
	}
	fe_mul(x176, x176, x88);

	x220 = x176;
	for(j = 0; j < 44; j++) {
		fe_sqr(x220, x220);
	}
	fe_mul(x220, x220, x44);

	x223 = x220;
	for(j = 0; j < 3; j++) {
		fe_sqr(x223, x223);
	}
	fe_mul(x223, x223, x3);

	t1 = x223;
	for(j = 0; j < 23; j++) {
		fe_sqr(t1, t1);
	}
	fe_mul(t1, t1, x22);
	for(j = 0; j < 5; j++) {
		fe_sqr(t1, t1);
	}
	fe_mul(t1, t1, a);
	for(j = 0; j < 3; j++) {
		fe_sqr(t1, t1);
	}
	fe_mul(t1, t1, x2);
	for(j = 0; j < 2; j++) {
		fe_sqr(t1, t1);
	}
	fe_mul(r, a, t1);
}

#undef FE_INLINE

#endif
