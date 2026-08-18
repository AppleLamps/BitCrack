/* Intel SHA-NI SHA-256 compression. Public-domain implementation by
 * Jeffrey Walton, based on Intel / Sean Gulley (miTLS). Adapted to
 * BitCrack's 16-word big-endian SHA-256 message layout.
 *
 * Inlined into AddressUtil/hash.cpp (SHA256_SHANI_HEADER_ONLY) so the
 * compressed HASH160 path can absorb SHA-NI when profitable. Keep
 * target("sse4.1,sha"); do not add noinline. */

#ifndef SHA256_SHANI_INCLUDED
#define SHA256_SHANI_INCLUDED

#if defined(__x86_64__) || defined(_M_X64)

#include <stdint.h>

#if defined(__GNUC__)
#include <x86intrin.h>
#define SHA256_SHANI_TARGET __attribute__((target("sse4.1,sha")))
#else
#define SHA256_SHANI_TARGET
#endif

#if defined(_MSC_VER)
#include <immintrin.h>
#endif

/* Process one 64-byte block. msg[] holds SHA-256 words (host uint32),
 * matching crypto::sha256()'s existing layout — not a raw byte string. */
SHA256_SHANI_TARGET
static inline void sha256_process_words(uint32_t state[8], const uint32_t w[16])
{
	__m128i STATE0, STATE1;
	__m128i MSG, TMP;
	__m128i MSG0, MSG1, MSG2, MSG3;
	__m128i ABEF_SAVE, CDGH_SAVE;

	TMP = _mm_loadu_si128((const __m128i *)&state[0]);
	STATE1 = _mm_loadu_si128((const __m128i *)&state[4]);

	TMP = _mm_shuffle_epi32(TMP, 0xB1);
	STATE1 = _mm_shuffle_epi32(STATE1, 0x1B);
	STATE0 = _mm_alignr_epi8(TMP, STATE1, 8);
	STATE1 = _mm_blend_epi16(STATE1, TMP, 0xF0);

	ABEF_SAVE = STATE0;
	CDGH_SAVE = STATE1;

	MSG0 = _mm_loadu_si128((const __m128i *)(w + 0));
	MSG = _mm_add_epi32(MSG0, _mm_set_epi64x(0xE9B5DBA5B5C0FBCFULL, 0x71374491428A2F98ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);

	MSG1 = _mm_loadu_si128((const __m128i *)(w + 4));
	MSG = _mm_add_epi32(MSG1, _mm_set_epi64x(0xAB1C5ED5923F82A4ULL, 0x59F111F13956C25BULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);

	MSG2 = _mm_loadu_si128((const __m128i *)(w + 8));
	MSG = _mm_add_epi32(MSG2, _mm_set_epi64x(0x550C7DC3243185BEULL, 0x12835B01D807AA98ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);

	MSG3 = _mm_loadu_si128((const __m128i *)(w + 12));
	MSG = _mm_add_epi32(MSG3, _mm_set_epi64x(0xC19BF1749BDC06A7ULL, 0x80DEB1FE72BE5D74ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG3, MSG2, 4);
	MSG0 = _mm_add_epi32(MSG0, TMP);
	MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);

	MSG = _mm_add_epi32(MSG0, _mm_set_epi64x(0x240CA1CC0FC19DC6ULL, 0xEFBE4786E49B69C1ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG0, MSG3, 4);
	MSG1 = _mm_add_epi32(MSG1, TMP);
	MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);

	MSG = _mm_add_epi32(MSG1, _mm_set_epi64x(0x76F988DA5CB0A9DCULL, 0x4A7484AA2DE92C6FULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG1, MSG0, 4);
	MSG2 = _mm_add_epi32(MSG2, TMP);
	MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);

	MSG = _mm_add_epi32(MSG2, _mm_set_epi64x(0xBF597FC7B00327C8ULL, 0xA831C66D983E5152ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG2, MSG1, 4);
	MSG3 = _mm_add_epi32(MSG3, TMP);
	MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);

	MSG = _mm_add_epi32(MSG3, _mm_set_epi64x(0x1429296706CA6351ULL, 0xD5A79147C6E00BF3ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG3, MSG2, 4);
	MSG0 = _mm_add_epi32(MSG0, TMP);
	MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);

	MSG = _mm_add_epi32(MSG0, _mm_set_epi64x(0x53380D134D2C6DFCULL, 0x2E1B213827B70A85ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG0, MSG3, 4);
	MSG1 = _mm_add_epi32(MSG1, TMP);
	MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);

	MSG = _mm_add_epi32(MSG1, _mm_set_epi64x(0x92722C8581C2C92EULL, 0x766A0ABB650A7354ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG1, MSG0, 4);
	MSG2 = _mm_add_epi32(MSG2, TMP);
	MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);

	MSG = _mm_add_epi32(MSG2, _mm_set_epi64x(0xC76C51A3C24B8B70ULL, 0xA81A664BA2BFE8A1ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG2, MSG1, 4);
	MSG3 = _mm_add_epi32(MSG3, TMP);
	MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);

	MSG = _mm_add_epi32(MSG3, _mm_set_epi64x(0x106AA070F40E3585ULL, 0xD6990624D192E819ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG3, MSG2, 4);
	MSG0 = _mm_add_epi32(MSG0, TMP);
	MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);

	MSG = _mm_add_epi32(MSG0, _mm_set_epi64x(0x34B0BCB52748774CULL, 0x1E376C0819A4C116ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG0, MSG3, 4);
	MSG1 = _mm_add_epi32(MSG1, TMP);
	MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
	MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);

	MSG = _mm_add_epi32(MSG1, _mm_set_epi64x(0x682E6FF35B9CCA4FULL, 0x4ED8AA4A391C0CB3ULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG1, MSG0, 4);
	MSG2 = _mm_add_epi32(MSG2, TMP);
	MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);

	MSG = _mm_add_epi32(MSG2, _mm_set_epi64x(0x8CC7020884C87814ULL, 0x78A5636F748F82EEULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	TMP = _mm_alignr_epi8(MSG2, MSG1, 4);
	MSG3 = _mm_add_epi32(MSG3, TMP);
	MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);

	MSG = _mm_add_epi32(MSG3, _mm_set_epi64x(0xC67178F2BEF9A3F7ULL, 0xA4506CEB90BEFFFAULL));
	STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
	MSG = _mm_shuffle_epi32(MSG, 0x0E);
	STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);

	STATE0 = _mm_add_epi32(STATE0, ABEF_SAVE);
	STATE1 = _mm_add_epi32(STATE1, CDGH_SAVE);

	TMP = _mm_shuffle_epi32(STATE0, 0x1B);
	STATE1 = _mm_shuffle_epi32(STATE1, 0xB1);
	STATE0 = _mm_blend_epi16(TMP, STATE1, 0xF0);
	STATE1 = _mm_alignr_epi8(STATE1, TMP, 8);

	_mm_storeu_si128((__m128i *)&state[0], STATE0);
	_mm_storeu_si128((__m128i *)&state[4], STATE1);
}

/* Two independent blocks, interleaved so SHA-NI latency overlaps. */
SHA256_SHANI_TARGET
static inline void sha256_process_words2(uint32_t stateA[8], const uint32_t wA[16],
	uint32_t stateB[8], const uint32_t wB[16])
{
	__m128i STATE0A, STATE1A, STATE0B, STATE1B;
	__m128i MSGA, MSGB, TMPA, TMPB;
	__m128i MSG0A, MSG1A, MSG2A, MSG3A;
	__m128i MSG0B, MSG1B, MSG2B, MSG3B;
	__m128i ABEF_A, CDGH_A, ABEF_B, CDGH_B;

	TMPA = _mm_loadu_si128((const __m128i *)&stateA[0]);
	STATE1A = _mm_loadu_si128((const __m128i *)&stateA[4]);
	TMPB = _mm_loadu_si128((const __m128i *)&stateB[0]);
	STATE1B = _mm_loadu_si128((const __m128i *)&stateB[4]);

	TMPA = _mm_shuffle_epi32(TMPA, 0xB1);
	TMPB = _mm_shuffle_epi32(TMPB, 0xB1);
	STATE1A = _mm_shuffle_epi32(STATE1A, 0x1B);
	STATE1B = _mm_shuffle_epi32(STATE1B, 0x1B);
	STATE0A = _mm_alignr_epi8(TMPA, STATE1A, 8);
	STATE0B = _mm_alignr_epi8(TMPB, STATE1B, 8);
	STATE1A = _mm_blend_epi16(STATE1A, TMPA, 0xF0);
	STATE1B = _mm_blend_epi16(STATE1B, TMPB, 0xF0);

	ABEF_A = STATE0A;
	CDGH_A = STATE1A;
	ABEF_B = STATE0B;
	CDGH_B = STATE1B;

	MSG0A = _mm_loadu_si128((const __m128i *)(wA + 0));
	MSG0B = _mm_loadu_si128((const __m128i *)(wB + 0));
	MSGA = _mm_add_epi32(MSG0A, _mm_set_epi64x(0xE9B5DBA5B5C0FBCFULL, 0x71374491428A2F98ULL));
	MSGB = _mm_add_epi32(MSG0B, _mm_set_epi64x(0xE9B5DBA5B5C0FBCFULL, 0x71374491428A2F98ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);

	MSG1A = _mm_loadu_si128((const __m128i *)(wA + 4));
	MSG1B = _mm_loadu_si128((const __m128i *)(wB + 4));
	MSGA = _mm_add_epi32(MSG1A, _mm_set_epi64x(0xAB1C5ED5923F82A4ULL, 0x59F111F13956C25BULL));
	MSGB = _mm_add_epi32(MSG1B, _mm_set_epi64x(0xAB1C5ED5923F82A4ULL, 0x59F111F13956C25BULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG0A = _mm_sha256msg1_epu32(MSG0A, MSG1A);
	MSG0B = _mm_sha256msg1_epu32(MSG0B, MSG1B);

	MSG2A = _mm_loadu_si128((const __m128i *)(wA + 8));
	MSG2B = _mm_loadu_si128((const __m128i *)(wB + 8));
	MSGA = _mm_add_epi32(MSG2A, _mm_set_epi64x(0x550C7DC3243185BEULL, 0x12835B01D807AA98ULL));
	MSGB = _mm_add_epi32(MSG2B, _mm_set_epi64x(0x550C7DC3243185BEULL, 0x12835B01D807AA98ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG1A = _mm_sha256msg1_epu32(MSG1A, MSG2A);
	MSG1B = _mm_sha256msg1_epu32(MSG1B, MSG2B);

	MSG3A = _mm_loadu_si128((const __m128i *)(wA + 12));
	MSG3B = _mm_loadu_si128((const __m128i *)(wB + 12));
	MSGA = _mm_add_epi32(MSG3A, _mm_set_epi64x(0xC19BF1749BDC06A7ULL, 0x80DEB1FE72BE5D74ULL));
	MSGB = _mm_add_epi32(MSG3B, _mm_set_epi64x(0xC19BF1749BDC06A7ULL, 0x80DEB1FE72BE5D74ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG3A, MSG2A, 4);
	TMPB = _mm_alignr_epi8(MSG3B, MSG2B, 4);
	MSG0A = _mm_add_epi32(MSG0A, TMPA);
	MSG0B = _mm_add_epi32(MSG0B, TMPB);
	MSG0A = _mm_sha256msg2_epu32(MSG0A, MSG3A);
	MSG0B = _mm_sha256msg2_epu32(MSG0B, MSG3B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG2A = _mm_sha256msg1_epu32(MSG2A, MSG3A);
	MSG2B = _mm_sha256msg1_epu32(MSG2B, MSG3B);

	MSGA = _mm_add_epi32(MSG0A, _mm_set_epi64x(0x240CA1CC0FC19DC6ULL, 0xEFBE4786E49B69C1ULL));
	MSGB = _mm_add_epi32(MSG0B, _mm_set_epi64x(0x240CA1CC0FC19DC6ULL, 0xEFBE4786E49B69C1ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG0A, MSG3A, 4);
	TMPB = _mm_alignr_epi8(MSG0B, MSG3B, 4);
	MSG1A = _mm_add_epi32(MSG1A, TMPA);
	MSG1B = _mm_add_epi32(MSG1B, TMPB);
	MSG1A = _mm_sha256msg2_epu32(MSG1A, MSG0A);
	MSG1B = _mm_sha256msg2_epu32(MSG1B, MSG0B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG3A = _mm_sha256msg1_epu32(MSG3A, MSG0A);
	MSG3B = _mm_sha256msg1_epu32(MSG3B, MSG0B);

	MSGA = _mm_add_epi32(MSG1A, _mm_set_epi64x(0x76F988DA5CB0A9DCULL, 0x4A7484AA2DE92C6FULL));
	MSGB = _mm_add_epi32(MSG1B, _mm_set_epi64x(0x76F988DA5CB0A9DCULL, 0x4A7484AA2DE92C6FULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG1A, MSG0A, 4);
	TMPB = _mm_alignr_epi8(MSG1B, MSG0B, 4);
	MSG2A = _mm_add_epi32(MSG2A, TMPA);
	MSG2B = _mm_add_epi32(MSG2B, TMPB);
	MSG2A = _mm_sha256msg2_epu32(MSG2A, MSG1A);
	MSG2B = _mm_sha256msg2_epu32(MSG2B, MSG1B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG0A = _mm_sha256msg1_epu32(MSG0A, MSG1A);
	MSG0B = _mm_sha256msg1_epu32(MSG0B, MSG1B);

	MSGA = _mm_add_epi32(MSG2A, _mm_set_epi64x(0xBF597FC7B00327C8ULL, 0xA831C66D983E5152ULL));
	MSGB = _mm_add_epi32(MSG2B, _mm_set_epi64x(0xBF597FC7B00327C8ULL, 0xA831C66D983E5152ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG2A, MSG1A, 4);
	TMPB = _mm_alignr_epi8(MSG2B, MSG1B, 4);
	MSG3A = _mm_add_epi32(MSG3A, TMPA);
	MSG3B = _mm_add_epi32(MSG3B, TMPB);
	MSG3A = _mm_sha256msg2_epu32(MSG3A, MSG2A);
	MSG3B = _mm_sha256msg2_epu32(MSG3B, MSG2B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG1A = _mm_sha256msg1_epu32(MSG1A, MSG2A);
	MSG1B = _mm_sha256msg1_epu32(MSG1B, MSG2B);

	MSGA = _mm_add_epi32(MSG3A, _mm_set_epi64x(0x1429296706CA6351ULL, 0xD5A79147C6E00BF3ULL));
	MSGB = _mm_add_epi32(MSG3B, _mm_set_epi64x(0x1429296706CA6351ULL, 0xD5A79147C6E00BF3ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG3A, MSG2A, 4);
	TMPB = _mm_alignr_epi8(MSG3B, MSG2B, 4);
	MSG0A = _mm_add_epi32(MSG0A, TMPA);
	MSG0B = _mm_add_epi32(MSG0B, TMPB);
	MSG0A = _mm_sha256msg2_epu32(MSG0A, MSG3A);
	MSG0B = _mm_sha256msg2_epu32(MSG0B, MSG3B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG2A = _mm_sha256msg1_epu32(MSG2A, MSG3A);
	MSG2B = _mm_sha256msg1_epu32(MSG2B, MSG3B);

	MSGA = _mm_add_epi32(MSG0A, _mm_set_epi64x(0x53380D134D2C6DFCULL, 0x2E1B213827B70A85ULL));
	MSGB = _mm_add_epi32(MSG0B, _mm_set_epi64x(0x53380D134D2C6DFCULL, 0x2E1B213827B70A85ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG0A, MSG3A, 4);
	TMPB = _mm_alignr_epi8(MSG0B, MSG3B, 4);
	MSG1A = _mm_add_epi32(MSG1A, TMPA);
	MSG1B = _mm_add_epi32(MSG1B, TMPB);
	MSG1A = _mm_sha256msg2_epu32(MSG1A, MSG0A);
	MSG1B = _mm_sha256msg2_epu32(MSG1B, MSG0B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG3A = _mm_sha256msg1_epu32(MSG3A, MSG0A);
	MSG3B = _mm_sha256msg1_epu32(MSG3B, MSG0B);

	MSGA = _mm_add_epi32(MSG1A, _mm_set_epi64x(0x92722C8581C2C92EULL, 0x766A0ABB650A7354ULL));
	MSGB = _mm_add_epi32(MSG1B, _mm_set_epi64x(0x92722C8581C2C92EULL, 0x766A0ABB650A7354ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG1A, MSG0A, 4);
	TMPB = _mm_alignr_epi8(MSG1B, MSG0B, 4);
	MSG2A = _mm_add_epi32(MSG2A, TMPA);
	MSG2B = _mm_add_epi32(MSG2B, TMPB);
	MSG2A = _mm_sha256msg2_epu32(MSG2A, MSG1A);
	MSG2B = _mm_sha256msg2_epu32(MSG2B, MSG1B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG0A = _mm_sha256msg1_epu32(MSG0A, MSG1A);
	MSG0B = _mm_sha256msg1_epu32(MSG0B, MSG1B);

	MSGA = _mm_add_epi32(MSG2A, _mm_set_epi64x(0xC76C51A3C24B8B70ULL, 0xA81A664BA2BFE8A1ULL));
	MSGB = _mm_add_epi32(MSG2B, _mm_set_epi64x(0xC76C51A3C24B8B70ULL, 0xA81A664BA2BFE8A1ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG2A, MSG1A, 4);
	TMPB = _mm_alignr_epi8(MSG2B, MSG1B, 4);
	MSG3A = _mm_add_epi32(MSG3A, TMPA);
	MSG3B = _mm_add_epi32(MSG3B, TMPB);
	MSG3A = _mm_sha256msg2_epu32(MSG3A, MSG2A);
	MSG3B = _mm_sha256msg2_epu32(MSG3B, MSG2B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG1A = _mm_sha256msg1_epu32(MSG1A, MSG2A);
	MSG1B = _mm_sha256msg1_epu32(MSG1B, MSG2B);

	MSGA = _mm_add_epi32(MSG3A, _mm_set_epi64x(0x106AA070F40E3585ULL, 0xD6990624D192E819ULL));
	MSGB = _mm_add_epi32(MSG3B, _mm_set_epi64x(0x106AA070F40E3585ULL, 0xD6990624D192E819ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG3A, MSG2A, 4);
	TMPB = _mm_alignr_epi8(MSG3B, MSG2B, 4);
	MSG0A = _mm_add_epi32(MSG0A, TMPA);
	MSG0B = _mm_add_epi32(MSG0B, TMPB);
	MSG0A = _mm_sha256msg2_epu32(MSG0A, MSG3A);
	MSG0B = _mm_sha256msg2_epu32(MSG0B, MSG3B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG2A = _mm_sha256msg1_epu32(MSG2A, MSG3A);
	MSG2B = _mm_sha256msg1_epu32(MSG2B, MSG3B);

	MSGA = _mm_add_epi32(MSG0A, _mm_set_epi64x(0x34B0BCB52748774CULL, 0x1E376C0819A4C116ULL));
	MSGB = _mm_add_epi32(MSG0B, _mm_set_epi64x(0x34B0BCB52748774CULL, 0x1E376C0819A4C116ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG0A, MSG3A, 4);
	TMPB = _mm_alignr_epi8(MSG0B, MSG3B, 4);
	MSG1A = _mm_add_epi32(MSG1A, TMPA);
	MSG1B = _mm_add_epi32(MSG1B, TMPB);
	MSG1A = _mm_sha256msg2_epu32(MSG1A, MSG0A);
	MSG1B = _mm_sha256msg2_epu32(MSG1B, MSG0B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);
	MSG3A = _mm_sha256msg1_epu32(MSG3A, MSG0A);
	MSG3B = _mm_sha256msg1_epu32(MSG3B, MSG0B);

	MSGA = _mm_add_epi32(MSG1A, _mm_set_epi64x(0x682E6FF35B9CCA4FULL, 0x4ED8AA4A391C0CB3ULL));
	MSGB = _mm_add_epi32(MSG1B, _mm_set_epi64x(0x682E6FF35B9CCA4FULL, 0x4ED8AA4A391C0CB3ULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG1A, MSG0A, 4);
	TMPB = _mm_alignr_epi8(MSG1B, MSG0B, 4);
	MSG2A = _mm_add_epi32(MSG2A, TMPA);
	MSG2B = _mm_add_epi32(MSG2B, TMPB);
	MSG2A = _mm_sha256msg2_epu32(MSG2A, MSG1A);
	MSG2B = _mm_sha256msg2_epu32(MSG2B, MSG1B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);

	MSGA = _mm_add_epi32(MSG2A, _mm_set_epi64x(0x8CC7020884C87814ULL, 0x78A5636F748F82EEULL));
	MSGB = _mm_add_epi32(MSG2B, _mm_set_epi64x(0x8CC7020884C87814ULL, 0x78A5636F748F82EEULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	TMPA = _mm_alignr_epi8(MSG2A, MSG1A, 4);
	TMPB = _mm_alignr_epi8(MSG2B, MSG1B, 4);
	MSG3A = _mm_add_epi32(MSG3A, TMPA);
	MSG3B = _mm_add_epi32(MSG3B, TMPB);
	MSG3A = _mm_sha256msg2_epu32(MSG3A, MSG2A);
	MSG3B = _mm_sha256msg2_epu32(MSG3B, MSG2B);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);

	MSGA = _mm_add_epi32(MSG3A, _mm_set_epi64x(0xC67178F2BEF9A3F7ULL, 0xA4506CEB90BEFFFAULL));
	MSGB = _mm_add_epi32(MSG3B, _mm_set_epi64x(0xC67178F2BEF9A3F7ULL, 0xA4506CEB90BEFFFAULL));
	STATE1A = _mm_sha256rnds2_epu32(STATE1A, STATE0A, MSGA);
	STATE1B = _mm_sha256rnds2_epu32(STATE1B, STATE0B, MSGB);
	MSGA = _mm_shuffle_epi32(MSGA, 0x0E);
	MSGB = _mm_shuffle_epi32(MSGB, 0x0E);
	STATE0A = _mm_sha256rnds2_epu32(STATE0A, STATE1A, MSGA);
	STATE0B = _mm_sha256rnds2_epu32(STATE0B, STATE1B, MSGB);

	STATE0A = _mm_add_epi32(STATE0A, ABEF_A);
	STATE1A = _mm_add_epi32(STATE1A, CDGH_A);
	STATE0B = _mm_add_epi32(STATE0B, ABEF_B);
	STATE1B = _mm_add_epi32(STATE1B, CDGH_B);

	TMPA = _mm_shuffle_epi32(STATE0A, 0x1B);
	TMPB = _mm_shuffle_epi32(STATE0B, 0x1B);
	STATE1A = _mm_shuffle_epi32(STATE1A, 0xB1);
	STATE1B = _mm_shuffle_epi32(STATE1B, 0xB1);
	STATE0A = _mm_blend_epi16(TMPA, STATE1A, 0xF0);
	STATE0B = _mm_blend_epi16(TMPB, STATE1B, 0xF0);
	STATE1A = _mm_alignr_epi8(STATE1A, TMPA, 8);
	STATE1B = _mm_alignr_epi8(STATE1B, TMPB, 8);

	_mm_storeu_si128((__m128i *)&stateA[0], STATE0A);
	_mm_storeu_si128((__m128i *)&stateA[4], STATE1A);
	_mm_storeu_si128((__m128i *)&stateB[0], STATE0B);
	_mm_storeu_si128((__m128i *)&stateB[4], STATE1B);
}

#ifndef SHA256_SHANI_HEADER_ONLY
extern "C" {
SHA256_SHANI_TARGET
void crypto_sha256_shani(unsigned int *msg, unsigned int *digest)
{
	sha256_process_words(digest, msg);
}

SHA256_SHANI_TARGET
void crypto_sha256_shani2(unsigned int *msg0, unsigned int *digest0,
	unsigned int *msg1, unsigned int *digest1)
{
	sha256_process_words2(digest0, msg0, digest1, msg1);
}
}
#endif

#endif /* x86_64 */

#endif /* SHA256_SHANI_INCLUDED */
