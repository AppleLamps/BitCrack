#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include"CryptoUtil.h"

#include "secp256k1.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#if defined(__SIZEOF_INT128__)
#include "field64.h"

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
#endif


using namespace secp256k1;

static uint256 _ONE(1);
static uint256 _ZERO;
static crypto::Rng _rng;

static void bulkInversionModP(std::vector<uint256> &in);
static void bulkInversionModPRange(std::vector<uint256> &in, size_t begin, size_t end);

static inline void addc(unsigned int a, unsigned int b, unsigned int carryIn, unsigned int &sum, int &carryOut)
{
	uint64_t sum64 = (uint64_t)a + b + carryIn;

	sum = (unsigned int)sum64;
	carryOut = (int)(sum64 >> 32) & 1;
}


static inline void subc(unsigned int a, unsigned int b, unsigned int borrowIn, unsigned int &diff, int &borrowOut)
{
	uint64_t diff64 = (uint64_t)a - b - borrowIn;

	diff = (unsigned int)diff64;
	borrowOut = (int)((diff64 >> 32) & 1);
}



static bool lessThanEqualTo(const unsigned int *a, const unsigned int *b, int len)
{
	for(int i = len - 1; i >= 0; i--) {
		if(a[i] < b[i]) {
			// is greater than
			return true;
		} else if(a[i] > b[i]) {
			// is less than
			return false;
		}
	}

	// is equal
	return true;
}

static bool greaterThanEqualTo(const unsigned int *a, const unsigned int *b, int len)
{
	for(int i = len - 1; i >= 0; i--) {
		if(a[i] > b[i]) {
			// is greater than
			return true;
		} else if(a[i] < b[i]) {
			// is less than
			return false;
		}
	}

	// is equal
	return true;
}

static int add(const unsigned int *a, const unsigned int *b, unsigned int *c, int len)
{
	int carry = 0;

	for(int i = 0; i < len; i++) {
		addc(a[i], b[i], carry, c[i], carry);
	}

	return carry;
}

static int sub(const unsigned int *a, const unsigned int *b, unsigned int *c, int len)
{
	int borrow = 0;

	for(int i = 0; i < len; i++) {
		subc(a[i], b[i], borrow, c[i], borrow);
	}

	return borrow & 1;
}

static void multiply(const unsigned int *x, int xLen, const unsigned int *y, int yLen, unsigned int *z)
{
	for(int i = 0; i < xLen + yLen; i++) {
		z[i] = 0;
	}

	int i, j;
	for(i = 0; i < xLen; i++) {

		unsigned int high = 0;

		for(j = 0; j < yLen; j++) {

			uint64_t product = (uint64_t)x[i] * y[j];

			product = product + z[i + j] + high;
			z[i + j] = (unsigned int)product;
			high = product >> 32;
		}

		z[i + yLen] = high;
	}
}

static void multiply256(const unsigned int a[8], const unsigned int b[8], unsigned int z[16])
{
#if defined(__SIZEOF_INT128__)
	uint64_t x[4];
	uint64_t y[4];
	uint64_t r[8];

	x[0] = (uint64_t)a[0] | ((uint64_t)a[1] << 32);
	x[1] = (uint64_t)a[2] | ((uint64_t)a[3] << 32);
	x[2] = (uint64_t)a[4] | ((uint64_t)a[5] << 32);
	x[3] = (uint64_t)a[6] | ((uint64_t)a[7] << 32);

	y[0] = (uint64_t)b[0] | ((uint64_t)b[1] << 32);
	y[1] = (uint64_t)b[2] | ((uint64_t)b[3] << 32);
	y[2] = (uint64_t)b[4] | ((uint64_t)b[5] << 32);
	y[3] = (uint64_t)b[6] | ((uint64_t)b[7] << 32);

	r[0] = r[1] = r[2] = r[3] = r[4] = r[5] = r[6] = r[7] = 0;

	for(int i = 0; i < 4; i++) {
		unsigned __int128 carry = 0;
		for(int j = 0; j < 4; j++) {
			unsigned __int128 t = (unsigned __int128)x[i] * y[j] + r[i + j] + carry;
			r[i + j] = (uint64_t)t;
			carry = t >> 64;
		}
		r[i + 4] = (uint64_t)carry;
	}

	for(int i = 0; i < 8; i++) {
		z[2 * i] = (unsigned int)r[i];
		z[2 * i + 1] = (unsigned int)(r[i] >> 32);
	}
#else
	multiply(a, 8, b, 8, z);
#endif
}

static uint256 rightShift(const uint256 &x, int count)
{
	uint256 r;

	count &= 0x1f;

	for(int i = 0; i < 7; i++) {
		r.v[i] = (x.v[i] >> count) | (x.v[i + 1] << (32 - count));
	}
	r.v[7] = x.v[7] >> count;

	return r;
}

uint256 uint256::mul(const uint256 &x) const
{
	unsigned int product[16] = { 0 };

	multiply256(this->v, x.v, product);

	return uint256(product);
}

uint256 uint256::mul(uint64_t i) const
{
    unsigned int product[16] = {0};
    unsigned int x[2];

    x[0] = (unsigned int)i;
    x[1] = (unsigned int)(i >> 32);

    multiply(x, 2, this->v, 8, product);

    return uint256(product);
}

uint256 uint256::mul(int i) const
{
	unsigned int product[16] = { 0 };

	multiply((unsigned int *)&i, 1, this->v, 8, product);

	return uint256(product);
}

uint256 uint256::mul(uint32_t i) const
{
    unsigned int product[16] = {0};

    multiply((unsigned int *)&i, 1, this->v, 8, product);

    return uint256(product);
}

uint256 uint256::div(uint32_t val) const
{
	uint256 t = *this;
	uint256 quotient;

	// Shift divisor left until MSB is 1
	uint32_t kWords[8] = { 0 };
	kWords[7] = val;

	int shiftCount = 7 * 32;

	while((kWords[7] & 0x80000000) == 0) {
		kWords[7] <<= 1;
		shiftCount++;
	}

	uint256 k(kWords);
	// while t >= divisor
	while(t.cmp(uint256(val)) >= 0) {

		// while t < k
		while(t.cmp(k) < 0) {
			// k = k / 2
			k = rightShift(k, 1);
			shiftCount--;
		}
		// t = t - k
		::sub(t.v, k.v, t.v, 8);

		quotient = quotient.add(uint256(2).pow(shiftCount));
	}

	return quotient;
}


uint256 uint256::mod(uint32_t val) const
{
	uint256 quotient = this->div(val);

	uint256 product = quotient.mul(val);

	uint256 result;

	::sub(this->v, product.v, result.v, 8);

	return result;
}

uint256 uint256::add(int val) const
{
	uint256 result(val);

	::add(this->v, result.v, result.v, 8);

	return result;
}

uint256 uint256::add(unsigned int val) const
{
	uint256 result(val);

	::add(this->v, result.v, result.v, 8);

	return result;
}

uint256 uint256::add(uint64_t val) const
{
	uint256 result(val);

	::add(this->v, result.v, result.v, 8);

	return result;
}

uint256 uint256::sub(int val) const
{
	uint256 result(val);

	::sub(this->v, result.v, result.v, 8);

	return result;
}

uint256 uint256::add(const uint256 &val) const
{
	uint256 result;

	::add(this->v, val.v, result.v, 8);

	return result;
}

uint256 uint256::sub(const uint256 &val) const
{
    uint256 result;

    ::sub(this->v, val.v, result.v, 8);

    return result;
}

static bool isOne(const uint256 &x)
{
	if(x.v[0] != 1) {
		return false;
	}

	for(int i = 1; i < 8; i++) {
		if(x.v[i] != 0) {
			return false;
		}
	}

	return true;
}

static uint256 divBy2(const uint256 &x)
{
	uint256 r;

	for(int i = 0; i < 7; i++) {
		r.v[i] = (x.v[i] >> 1) | (x.v[i + 1] << 31);
	}
	r.v[7] = x.v[7] >> 1;

	return r;
}


static bool isEven(const uint256 &x)
{
	return (x.v[0] & 1) == 0;
}

ecpoint secp256k1::pointAtInfinity()
{
	uint256 x(_POINT_AT_INFINITY_WORDS);

	return ecpoint(x, x);
}

ecpoint secp256k1::G()
{
	uint256 x(_GX_WORDS);
	uint256 y(_GY_WORDS);

	return ecpoint(x, y);
}

uint256 secp256k1::invModP(const uint256 &x)
{
#if defined(__SIZEOF_INT128__)
	fe a, r;
	fe_load(a, x);
	fe_inv(r, a);
	uint256 output;
	fe_store(output, r);
	return output;
#else
	uint256 u = x;
	uint256 v = P;
	uint256 x1 = _ONE;
	uint256 x2 = _ZERO;

	// Signed part of the 256-bit words
	int x1Signed = 0;
	int x2Signed = 0;

	while(!isOne(u) && !isOne(v)) {

		while(isEven(u)) {

			u = divBy2(u);

			if(isEven(x1)) {
				x1 = divBy2(x1);

				// Shift right (signed bit is preserved)
				x1.v[7] |= ((unsigned int)x1Signed & 0x01) << 31;

				x1Signed >>= 1;
			} else {
				int carry = add(x1.v, P.v, x1.v, 8);

				x1 = divBy2(x1);

				x1Signed += carry;

				x1.v[7] |= ((unsigned int)x1Signed & 0x01) << 31;

				x1Signed >>= 1;
			}

		}

		while(isEven(v)) {

			v = divBy2(v);

			if(isEven(x2)) {

				x2 = divBy2(x2);

				x2.v[7] |= ((unsigned int)x2Signed & 0x01) << 31;

				x2Signed >>= 1;
			} else {
				int carry = add(x2.v, P.v, x2.v, 8);

				x2 = divBy2(x2);

				x2Signed += carry;

				x2.v[7] |= ((unsigned int)x2Signed & 0x01) << 31;

				x2Signed >>= 1;
			}
		}

		if(lessThanEqualTo(v.v, u.v, 8)) {
			sub(u.v, v.v, u.v, 8);

			// x1 = x1 - x2
			int borrow = sub(x1.v, x2.v, x1.v, 8);
			x1Signed -= x2Signed;
			x1Signed -= borrow;
		} else {
			sub(v.v, u.v, v.v, 8);
			int borrow = sub(x2.v, x1.v, x2.v, 8);
			x2Signed -= x1Signed;
			x2Signed -= borrow;
		}
	}

	uint256 output;

	if(isOne(u)) {
	
		while(x1Signed < 0) {
			x1Signed += add(x1.v, P.v, x1.v, 8);
		}
	
		while(x1Signed > 0) {
			x1Signed -= sub(x1.v, P.v, x1.v, 8);
		}
	
		for(int i = 0; i < 8; i++) {
			output.v[i] = x1.v[i];
		}
	
	} else {
	
		while(x2Signed < 0) {
			x2Signed += add(x2.v, P.v, x2.v,  8);
		}
	
		while(x2Signed > 0) {
			x2Signed -= sub(x2.v, P.v, x2.v, 8);
		}
	
		for(int i = 0; i < 8; i++) {
			output.v[i] = x2.v[i];
		}
	}

	return output;
#endif
}



uint256 secp256k1::addModP(const uint256 &a, const uint256 &b)
{
#if defined(__SIZEOF_INT128__)
	fe fa, fb, fr;
	fe_load(fa, a);
	fe_load(fb, b);
	fe_add(fr, fa, fb);
	uint256 sum;
	fe_store(sum, fr);
	return sum;
#else
	uint256 sum;

	int overflow = add(a.v, b.v, sum.v, 8);

	// mod P
	if(overflow || greaterThanEqualTo(sum.v, P.v, 8)) {
		sub(sum.v, P.v, sum.v, 8);
	}

	return sum;
#endif
}

uint256 secp256k1::addModN(const uint256 &a, const uint256 &b)
{
	uint256 sum;

	int overflow = add(a.v, b.v, sum.v, 8);

	// mod P
	if(overflow || greaterThanEqualTo(sum.v, N.v, 8)) {
		sub(sum.v, N.v, sum.v, 8);
	}

	return sum;
}

uint256 secp256k1::subModN(const uint256 &a, const uint256 &b)
{
	uint256 diff;

	if(sub(a.v, b.v, diff.v, 8)) {
		add(diff.v, N.v, diff.v, 8);
	}

	return diff;
}

uint256 secp256k1::subModP(const uint256 &a, const uint256 &b)
{
#if defined(__SIZEOF_INT128__)
	fe fa, fb, fr;
	fe_load(fa, a);
	fe_load(fb, b);
	fe_sub(fr, fa, fb);
	uint256 diff;
	fe_store(diff, fr);
	return diff;
#else
	uint256 diff;

	if(sub(a.v, b.v, diff.v, 8)) {
		add(diff.v, P.v, diff.v, 8);
	}

	return diff;
#endif
}



uint256 secp256k1::negModP(const uint256 &x)
{
	return subModP(P, x);
}

uint256 secp256k1::negModN(const uint256 &x)
{
	return subModN(N, x);
}

uint256 secp256k1::multiplyModP(const uint256 &a, const uint256 &b)
{
#if defined(__SIZEOF_INT128__)
	fe fa, fb, fr;
	fe_load(fa, a);
	fe_load(fb, b);
	fe_mul(fr, fa, fb);
	uint256 result;
	fe_store(result, fr);
	return result;
#else
	unsigned int product[16];

	multiply256(a.v, b.v, product);

	unsigned int tmp[10] = { 0 };
	unsigned int tmp2[10] = { 0 };
	unsigned int s = 977;

	//multiply by high 8 words by 2^32 + 977
	for(int i = 0; i < 8; i++) {
		tmp2[1 + i] = product[8 + i];
	}

	multiply(&s, 1, &product[8], 8, &tmp[0]);
	add(tmp, tmp2, tmp, 10);

	// clear top 8 words of product
	for(int i = 8; i < 16; i++) {
		product[i] = 0;
	}

	//add to product
	add(&product[0], tmp, &product[0], 10);


	//multiply high 2 words by 2^32 + 977
	for(int i = 0; i < 8; i++) {
		tmp2[1 + i] = product[8 + i];
	}
	//multiply(&s, 1, &product[8], 2, &tmp[1]);
	multiply(&s, 1, &product[8], 8, &tmp[0]);
	add(tmp, tmp2, tmp, 10);



	// add to low 8 words
	int overflow = add(&product[0], &tmp[0], &product[0], 8);

	if(overflow || greaterThanEqualTo(&product[0], P.v, 8)) {
		sub(&product[0], P.v, &product[0], 8);
	}

	uint256 result;

	for(int i = 0; i < 8; i++) {
		result.v[i] = product[i];
	}

	return result;
#endif
}


static void reduceModN(const unsigned int *x, unsigned int *r)
{
	unsigned int barrettN[] = { 0x2fc9bec0, 0x402da173, 0x50b75fc4, 0x45512319, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 00000001 };
	unsigned int product[25] = { 0 };

	// Multiply by barrett constant
	multiply(barrettN, 9, x, 16, product);

	// divide by 2^512
	for(int i = 0; i < 9; i++) {
		product[i] = product[16 + i];
	}

	unsigned int product2[16] = { 0 };

	// Multiply by N
	multiply(product, 8, N.v, 8, product2);

	// Take the difference
	unsigned int diff[16] = { 0 };
	sub(x, product2, diff, 16);

	if((diff[8] & 1) || greaterThanEqualTo(diff, N.v, 8)) {
		sub(diff, N.v, diff, 8);
	}

	for(int i = 0; i < 8; i++) {
		r[i] = diff[i];
	}
}

uint256 secp256k1::multiplyModN(const uint256 &a, const uint256 &b)
{
	unsigned int product[16];

	multiply256(a.v, b.v, product);

	uint256 r;

	bool gt = false;
	for(int i = 0; i < 8; i++) {
		if(product[8 + i] != 0) {
			gt = true;
			break;
		}
	}

	if(gt) {
		reduceModN(product, r.v);
	} else if(greaterThanEqualTo(product, N.v, 8)) {
		sub(product, N.v, r.v, 8);
	} else {
		for(int i = 0; i < 8; i++) {
			r.v[i] = product[i];
		}
	}

	return r;
}

std::string secp256k1::uint256::toString(int base)
{
	std::string s = "";

	for(int i = 7; i >= 0; i--) {
		char hex[9] = { 0 };

		sprintf(hex, "%.8X", this->v[i]);
		s += std::string(hex);
	}

	return s;
}


uint256 secp256k1::generatePrivateKey()
{
	uint256 k;

	_rng.get((unsigned char *)k.v, 32);

	return k;
}

bool secp256k1::isPointAtInfinity(const ecpoint &p)
{

	for(int i = 0; i < 8; i++) {
		if(p.x.v[i] != 0xffffffff) {
			return false;
		}
	}

	for(int i = 0; i < 8; i++) {
		if(p.y.v[i] != 0xffffffff) {
			return false;
		}
	}

	return true;
}

ecpoint secp256k1::doublePoint(const ecpoint &p)
{
	// 1 / 2y
	uint256 yInv = invModP(addModP(p.y, p.y));

	// s = 3x^2 / 2y
	uint256 x3 = multiplyModP(p.x, p.x);
	uint256 s = multiplyModP(addModP(addModP(x3, x3), x3), yInv);

	//rx = s^2 - 2x
	uint256 rx = subModP(subModP(multiplyModP(s, s), p.x), p.x);

	//ry = s * (px - rx) - py
	uint256 ry = subModP(multiplyModP(s, subModP(p.x, rx)), p.y);

	ecpoint result;
	result.x = rx;
	result.y = ry;

	return result;
}

ecpoint secp256k1::addPoints(const ecpoint &p1, const ecpoint &p2)
{
	if(p1 == p2) {
		return doublePoint(p1);
	}

	if(p1.x == p2.x) {
		return pointAtInfinity();
	}

	if(isPointAtInfinity(p1)) {
		return p2;
	}

	if(isPointAtInfinity(p2)) {
		return p1;
	}

	uint256 rise = subModP(p1.y, p2.y);
	uint256 run = subModP(p1.x, p2.x);

	uint256 s = multiplyModP(rise, invModP(run));

	//rx = (s*s - px - qx) % _p;
	uint256 rx = subModP(subModP(multiplyModP(s, s), p1.x), p2.x);

	//ry = (s * (px - rx) - py) % _p;
	uint256 ry = subModP(multiplyModP(s, subModP(p1.x, rx)), p1.y);

	ecpoint sum;
	sum.x = rx;
	sum.y = ry;

	return sum;
}

#if defined(__SIZEOF_INT128__)
static void bulkInversionFeRange(std::vector<fe> &in, size_t begin, size_t end)
{
	if(end <= begin) {
		return;
	}

	size_t count = end - begin;
	std::vector<fe> products(count);
	fe total;
	fe_set1(total);

	for(size_t i = 0; i < count; i++) {
		fe_mul(total, total, in[begin + i]);
		products[i] = total;
	}

	fe inverse;
	fe_inv(inverse, total);

	for(size_t i = count; i-- > 0; ) {
		if(i > 0) {
			fe newValue;
			fe_mul(newValue, products[i - 1], inverse);
			fe_mul(inverse, inverse, in[begin + i]);
			in[begin + i] = newValue;
		} else {
			in[begin + i] = inverse;
		}
	}
}
#endif

void secp256k1::addPointsBulk(std::vector<ecpoint> &points, const ecpoint &q, int threads)
{
	size_t n = points.size();
	if(n == 0) {
		return;
	}

	if(isPointAtInfinity(q)) {
		return;
	}

	if(threads < 1) {
		threads = 1;
	}

#if defined(__SIZEOF_INT128__)
	std::vector<fe> run(n);
	std::vector<unsigned char> op(n);

	fe qx, qy;
	fe_load(qx, q.x);
	fe_load(qy, q.y);

#ifdef _OPENMP
	#pragma omp parallel for schedule(static) num_threads(threads)
	for(long long i = 0; i < (long long)n; i++) {
#else
	for(size_t i = 0; i < n; i++) {
#endif
		fe px, py;
		fe_load(px, points[i].x);
		fe_load(py, points[i].y);

		if(fe_is_inf(px, py)) {
			op[i] = 1;
			fe_set1(run[i]);
		} else if(fe_eq(px, qx)) {
			if(fe_eq(py, qy)) {
				op[i] = 2;
				fe_add(run[i], py, py);
			} else {
				op[i] = 3;
				fe_set1(run[i]);
			}
		} else {
			op[i] = 0;
			fe_sub(run[i], px, qx);
		}
	}

#ifdef _OPENMP
	if(threads > 1 && n > 1) {
		#pragma omp parallel num_threads(threads)
		{
			int tid = omp_get_thread_num();
			int nt = omp_get_num_threads();
			size_t chunk = (n + (size_t)nt - 1) / (size_t)nt;
			size_t begin = (size_t)tid * chunk;
			size_t end = begin + chunk;
			if(begin > n) {
				begin = n;
			}
			if(end > n) {
				end = n;
			}
			bulkInversionFeRange(run, begin, end);
		}
	} else {
		bulkInversionFeRange(run, 0, n);
	}
#else
	bulkInversionFeRange(run, 0, n);
	(void)threads;
#endif

#ifdef _OPENMP
	#pragma omp parallel for schedule(static) num_threads(threads)
	for(long long i = 0; i < (long long)n; i++) {
#else
	for(size_t i = 0; i < n; i++) {
#endif
		if(op[i] == 1) {
			points[i] = q;
			continue;
		}

		if(op[i] == 3) {
			points[i] = pointAtInfinity();
			continue;
		}

		fe px, py, s, rx, ry, tmp;
		fe_load(px, points[i].x);
		fe_load(py, points[i].y);

		if(op[i] == 2) {
			fe_sqr(tmp, px);
			fe_add(s, tmp, tmp);
			fe_add(s, s, tmp);
			fe_mul(s, s, run[i]);
			fe_sqr(tmp, s);
			fe_sub(rx, tmp, px);
			fe_sub(rx, rx, px);
			fe_sub(tmp, px, rx);
			fe_mul(ry, s, tmp);
			fe_sub(ry, ry, py);
			fe_store(points[i].x, rx);
			fe_store(points[i].y, ry);
			continue;
		}

		fe_sub(tmp, py, qy);
		fe_mul(s, tmp, run[i]);
		fe_sqr(tmp, s);
		fe_sub(rx, tmp, px);
		fe_sub(rx, rx, qx);
		fe_sub(tmp, px, rx);
		fe_mul(ry, s, tmp);
		fe_sub(ry, ry, py);
		fe_store(points[i].x, rx);
		fe_store(points[i].y, ry);
	}
#else
	std::vector<uint256> run(n);
	std::vector<unsigned char> op(n);

#ifdef _OPENMP
	#pragma omp parallel for schedule(static) num_threads(threads)
	for(long long i = 0; i < (long long)n; i++) {
#else
	for(size_t i = 0; i < n; i++) {
#endif
		if(isPointAtInfinity(points[i])) {
			op[i] = 1;
			run[i] = uint256(1);
		} else if(points[i].x == q.x) {
			if(points[i].y == q.y) {
				op[i] = 2;
				run[i] = addModP(points[i].y, points[i].y);
			} else {
				op[i] = 3;
				run[i] = uint256(1);
			}
		} else {
			op[i] = 0;
			run[i] = subModP(points[i].x, q.x);
		}
	}

#ifdef _OPENMP
	if(threads > 1 && n > 1) {
		#pragma omp parallel num_threads(threads)
		{
			int tid = omp_get_thread_num();
			int nt = omp_get_num_threads();
			size_t chunk = (n + (size_t)nt - 1) / (size_t)nt;
			size_t begin = (size_t)tid * chunk;
			size_t end = begin + chunk;
			if(begin > n) {
				begin = n;
			}
			if(end > n) {
				end = n;
			}
			bulkInversionModPRange(run, begin, end);
		}
	} else {
		bulkInversionModP(run);
	}
#else
	bulkInversionModP(run);
	(void)threads;
#endif

#ifdef _OPENMP
	#pragma omp parallel for schedule(static) num_threads(threads)
	for(long long i = 0; i < (long long)n; i++) {
#else
	for(size_t i = 0; i < n; i++) {
#endif
		if(op[i] == 1) {
			points[i] = q;
			continue;
		}

		if(op[i] == 3) {
			points[i] = pointAtInfinity();
			continue;
		}

		if(op[i] == 2) {
			uint256 x3 = multiplyModP(points[i].x, points[i].x);
			uint256 s = multiplyModP(addModP(addModP(x3, x3), x3), run[i]);
			uint256 rx = subModP(subModP(multiplyModP(s, s), points[i].x), points[i].x);
			uint256 ry = subModP(multiplyModP(s, subModP(points[i].x, rx)), points[i].y);
			points[i].x = rx;
			points[i].y = ry;
			continue;
		}

		uint256 rise = subModP(points[i].y, q.y);
		uint256 s = multiplyModP(rise, run[i]);
		uint256 rx = subModP(subModP(multiplyModP(s, s), points[i].x), q.x);
		uint256 ry = subModP(multiplyModP(s, subModP(points[i].x, rx)), points[i].y);
		points[i].x = rx;
		points[i].y = ry;
	}
#endif
}

void secp256k1::addPointsBulkXY(uint64_t *x, uint64_t *y, size_t n, const uint64_t qx[4], const uint64_t qy[4], int threads)
{
	if(n == 0) {
		return;
	}

	if(threads < 1) {
		threads = 1;
	}

#if defined(__SIZEOF_INT128__)
	fe qxf, qyf;
	fe_load_u64(qxf, qx);
	fe_load_u64(qyf, qy);

	std::vector<fe> run(n);
	std::vector<unsigned char> op(n);

#ifdef _OPENMP
	#pragma omp parallel for schedule(static) num_threads(threads)
	for(long long i = 0; i < (long long)n; i++) {
#else
	for(size_t i = 0; i < n; i++) {
#endif
		const uint64_t *xi = x + (size_t)i * 4;
		const uint64_t *yi = y + (size_t)i * 4;
		fe px, py;
#if defined(__GNUC__)
		if((size_t)i + 8 < n) {
			__builtin_prefetch(x + ((size_t)i + 8) * 4, 0, 3);
		}
#endif
		fe_load_u64(px, xi);
		fe_load_u64(py, yi);
		if(fe_eq(px, qxf)) {
			if(fe_eq(py, qyf)) {
				op[i] = 2;
				fe_add(run[i], py, py);
			} else {
				op[i] = 3;
				fe_set1(run[i]);
			}
		} else {
			op[i] = 0;
			fe_sub(run[i], px, qxf);
		}
	}

#ifdef _OPENMP
	if(threads > 1 && n > 1) {
		#pragma omp parallel num_threads(threads)
		{
			int tid = omp_get_thread_num();
			int nt = omp_get_num_threads();
			size_t chunk = (n + (size_t)nt - 1) / (size_t)nt;
			size_t begin = (size_t)tid * chunk;
			size_t end = begin + chunk;
			if(begin > n) {
				begin = n;
			}
			if(end > n) {
				end = n;
			}
			bulkInversionFeRange(run, begin, end);
		}
	} else {
		bulkInversionFeRange(run, 0, n);
	}
#else
	bulkInversionFeRange(run, 0, n);
	(void)threads;
#endif

#ifdef _OPENMP
	#pragma omp parallel for schedule(static) num_threads(threads)
	for(long long i = 0; i < (long long)n; i++) {
#else
	for(size_t i = 0; i < n; i++) {
#endif
		uint64_t *xi = x + (size_t)i * 4;
		uint64_t *yi = y + (size_t)i * 4;
#if defined(__GNUC__)
		if((size_t)i + 8 < n) {
			__builtin_prefetch(x + ((size_t)i + 8) * 4, 0, 3);
			__builtin_prefetch(y + ((size_t)i + 8) * 4, 0, 3);
		}
#endif
		if(op[i] == 3) {
			xi[0] = xi[1] = xi[2] = xi[3] = ~0ULL;
			yi[0] = yi[1] = yi[2] = yi[3] = ~0ULL;
			continue;
		}

		fe px, py, s, rx, ry, tmp;
		fe_load_u64(px, xi);
		fe_load_u64(py, yi);

		if(op[i] == 2) {
			fe_sqr(tmp, px);
			fe_add(s, tmp, tmp);
			fe_add(s, s, tmp);
			fe_mul(s, s, run[i]);
			fe_sqr(tmp, s);
			fe_sub(rx, tmp, px);
			fe_sub(rx, rx, px);
			fe_sub(tmp, px, rx);
			fe_mul(ry, s, tmp);
			fe_sub(ry, ry, py);
			fe_store_u64(xi, rx);
			fe_store_u64(yi, ry);
			continue;
		}

		fe_sub(tmp, py, qyf);
		fe_mul(s, tmp, run[i]);
		fe_sqr(tmp, s);
		fe_sub(rx, tmp, px);
		fe_sub(rx, rx, qxf);
		fe_sub(tmp, px, rx);
		fe_mul(ry, s, tmp);
		fe_sub(ry, ry, py);
		fe_store_u64(xi, rx);
		fe_store_u64(yi, ry);
	}
#else
	std::vector<ecpoint> points(n);
	uint256 qx256, qy256;

	qx256.v[0] = (unsigned int)qx[0];
	qx256.v[1] = (unsigned int)(qx[0] >> 32);
	qx256.v[2] = (unsigned int)qx[1];
	qx256.v[3] = (unsigned int)(qx[1] >> 32);
	qx256.v[4] = (unsigned int)qx[2];
	qx256.v[5] = (unsigned int)(qx[2] >> 32);
	qx256.v[6] = (unsigned int)qx[3];
	qx256.v[7] = (unsigned int)(qx[3] >> 32);

	qy256.v[0] = (unsigned int)qy[0];
	qy256.v[1] = (unsigned int)(qy[0] >> 32);
	qy256.v[2] = (unsigned int)qy[1];
	qy256.v[3] = (unsigned int)(qy[1] >> 32);
	qy256.v[4] = (unsigned int)qy[2];
	qy256.v[5] = (unsigned int)(qy[2] >> 32);
	qy256.v[6] = (unsigned int)qy[3];
	qy256.v[7] = (unsigned int)(qy[3] >> 32);

	ecpoint q(qx256, qy256);

	for(size_t i = 0; i < n; i++) {
		uint256 px, py;
		const uint64_t *xl = x + i * 4;
		const uint64_t *yl = y + i * 4;

		px.v[0] = (unsigned int)xl[0];
		px.v[1] = (unsigned int)(xl[0] >> 32);
		px.v[2] = (unsigned int)xl[1];
		px.v[3] = (unsigned int)(xl[1] >> 32);
		px.v[4] = (unsigned int)xl[2];
		px.v[5] = (unsigned int)(xl[2] >> 32);
		px.v[6] = (unsigned int)xl[3];
		px.v[7] = (unsigned int)(xl[3] >> 32);

		py.v[0] = (unsigned int)yl[0];
		py.v[1] = (unsigned int)(yl[0] >> 32);
		py.v[2] = (unsigned int)yl[1];
		py.v[3] = (unsigned int)(yl[1] >> 32);
		py.v[4] = (unsigned int)yl[2];
		py.v[5] = (unsigned int)(yl[2] >> 32);
		py.v[6] = (unsigned int)yl[3];
		py.v[7] = (unsigned int)(yl[3] >> 32);

		points[i] = ecpoint(px, py);
	}

	addPointsBulk(points, q, threads);

	for(size_t i = 0; i < n; i++) {
		uint64_t *xl = x + i * 4;
		uint64_t *yl = y + i * 4;
		xl[0] = (uint64_t)points[i].x.v[0] | ((uint64_t)points[i].x.v[1] << 32);
		xl[1] = (uint64_t)points[i].x.v[2] | ((uint64_t)points[i].x.v[3] << 32);
		xl[2] = (uint64_t)points[i].x.v[4] | ((uint64_t)points[i].x.v[5] << 32);
		xl[3] = (uint64_t)points[i].x.v[6] | ((uint64_t)points[i].x.v[7] << 32);
		yl[0] = (uint64_t)points[i].y.v[0] | ((uint64_t)points[i].y.v[1] << 32);
		yl[1] = (uint64_t)points[i].y.v[2] | ((uint64_t)points[i].y.v[3] << 32);
		yl[2] = (uint64_t)points[i].y.v[4] | ((uint64_t)points[i].y.v[5] << 32);
		yl[3] = (uint64_t)points[i].y.v[6] | ((uint64_t)points[i].y.v[7] << 32);
	}
#endif
}

ecpoint secp256k1::multiplyPoint(const uint256 &k, const ecpoint &p)
{
	ecpoint sum = pointAtInfinity();
	ecpoint d = p;

	for(int i = 0; i < 256; i++) {
		unsigned int mask = 1 << (i % 32);

		if(k.v[i / 32] & mask) {
			sum = addPoints(sum, d);
		}

		d = doublePoint(d);
	}

	return sum;
}

uint256 generatePrivateKey()
{
	uint256 k;

	for(int i = 0; i < 8; i++) {
		k.v[i] = ((unsigned int)rand() | ((unsigned int)rand()) << 17);
	}

	return k;
}

bool secp256k1::pointExists(const ecpoint &p)
{
	uint256 y = multiplyModP(p.y, p.y);

	uint256 x = addModP(multiplyModP(multiplyModP(p.x, p.x), p.x), uint256(7));

	return y == x;
}

static void bulkInversionModPRange(std::vector<uint256> &in, size_t begin, size_t end)
{
	if(end <= begin) {
		return;
	}

	size_t count = end - begin;
#if defined(__SIZEOF_INT128__)
	std::vector<fe> values(count);
	for(size_t i = 0; i < count; i++) {
		fe_load(values[i], in[begin + i]);
	}
	bulkInversionFeRange(values, 0, count);
	for(size_t i = 0; i < count; i++) {
		fe_store(in[begin + i], values[i]);
	}
#else
	std::vector<uint256> products(count);
	uint256 total(1);

	for(size_t i = 0; i < count; i++) {
		total = secp256k1::multiplyModP(total, in[begin + i]);
		products[i] = total;
	}

	uint256 inverse = secp256k1::invModP(total);

	for(size_t i = count; i-- > 0; ) {
		if(i > 0) {
			uint256 newValue = secp256k1::multiplyModP(products[i - 1], inverse);
			inverse = multiplyModP(inverse, in[begin + i]);
			in[begin + i] = newValue;
		} else {
			in[begin + i] = inverse;
		}
	}
#endif
}

static void bulkInversionModP(std::vector<uint256> &in)
{
	bulkInversionModPRange(in, 0, in.size());
}

void secp256k1::generateKeyPairsBulk(unsigned int count, const ecpoint &basePoint, std::vector<uint256> &privKeysOut, std::vector<ecpoint> &pubKeysOut)
{
	privKeysOut.clear();

	for(unsigned int i = 0; i < count; i++) {
		privKeysOut.push_back(generatePrivateKey());
	}

	generateKeyPairsBulk(basePoint, privKeysOut, pubKeysOut);
}

void secp256k1::generateKeyPairsBulk(const ecpoint &basePoint, std::vector<uint256> &privKeys, std::vector<ecpoint> &pubKeysOut)
{
	unsigned int count = (unsigned int)privKeys.size();

	//privKeysOut.clear();
	pubKeysOut.clear();

	// generate a table of points G, 2G, 4G, 8G...(2^255)G
	std::vector<ecpoint> table;

	table.push_back(basePoint);
	for(int i = 1; i < 256; i++) {

		ecpoint p = doublePoint(table[i-1]);
		if(!pointExists(p)) {
			throw std::string("Point does not exist!");
		}
		table.push_back(p);
	}

	for(unsigned int i = 0; i < count; i++) {
		pubKeysOut.push_back(ecpoint());
	}

	for(int i = 0; i < 256; i++) {

		std::vector<uint256> runList;

		// calculate (Px - Qx)
		for(unsigned int j = 0; j < count; j++) {
			uint256 run;
			uint256 k = privKeys[j];

			if(k.bit(i)) {
				if(isPointAtInfinity(pubKeysOut[j])) {
					run = uint256(2);
				} else {
					run = subModP(pubKeysOut[j].x, table[i].x);
				}
			} else {
				run = uint256(2);
			}

			runList.push_back(run);
		}

		// calculate 1/(Px - Qx)
		bulkInversionModP(runList);

		// complete the addition
		for(unsigned int j = 0; j < count; j++) {
			uint256 rise;
			uint256 k = privKeys[j];

			if(k.bit(i)) {
				if(isPointAtInfinity(pubKeysOut[j])) {
					pubKeysOut[j] = table[i];
				} else {
					rise = subModP(pubKeysOut[j].y, table[i].y);

					// s = (Py - Qy)/(Px - Qx)
					uint256 s = multiplyModP(rise, runList[j]);

					//rx = (s*s - px - qx) % _p;
					uint256 rx = subModP(subModP(multiplyModP(s, s), pubKeysOut[j].x), table[i].x);

					uint256 ry = subModP(multiplyModP(s, subModP(pubKeysOut[j].x, rx)), pubKeysOut[j].y);

					pubKeysOut[j] = ecpoint(rx, ry);
				}
			}
		}
	}
}

/**
 * Parses a public key. Expected format is 04<64 hex digits for X><64 hex digits for Y>
 */
secp256k1::ecpoint secp256k1::parsePublicKey(const std::string &pubKeyString)
{
	if(pubKeyString.length() != 130) {
		throw std::string("Invalid public key");
	}

	if(pubKeyString[0] != '0' || pubKeyString[1] != '4') {
		throw std::string("Invalid public key");
	}

	std::string xString = pubKeyString.substr(2, 64);
	std::string yString = pubKeyString.substr(66, 64);

	uint256 x(xString);
	uint256 y(yString);

	ecpoint p(x, y);

	if(!pointExists(p)) {
		throw std::string("Invalid public key");
	}

	return p;
}
