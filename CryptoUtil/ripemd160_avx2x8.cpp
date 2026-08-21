/* AVX2 8-way RIPEMD-160. Compiled with -mavx2 -mno-avx512f. */

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

#define RIPEMD_LANES 8
#include "ripemd160_wide.inc"

extern "C" {
#if defined(__GNUC__)
__attribute__((target("avx2"), noinline))
#endif
void crypto_ripemd160_avx2x8(unsigned int *msg, unsigned int *digest)
{
	ripemd160_Nx(msg, digest);
}

#if defined(__GNUC__)
__attribute__((target("avx2"), noinline))
#endif
void crypto_ripemd160_fromsha_avx2x8(unsigned int *sha, unsigned int *digest)
{
	ripemd160_fromsha_Nx(sha, digest);
}
}

#endif
