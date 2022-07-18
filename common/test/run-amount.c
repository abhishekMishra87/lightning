#include "config.h"
#include "../amount.c"
#include <common/setup.h>
#include <stdio.h>

/* AUTOGENERATED MOCKS START */
/* Generated stub for fromwire */
const u8 *fromwire(const u8 **cursor UNNEEDED, size_t *max UNNEEDED, void *copy UNNEEDED, size_t n UNNEEDED)
{ fprintf(stderr, "fromwire called!\n"); abort(); }
/* Generated stub for fromwire_bool */
bool fromwire_bool(const u8 **cursor UNNEEDED, size_t *max UNNEEDED)
{ fprintf(stderr, "fromwire_bool called!\n"); abort(); }
/* Generated stub for fromwire_fail */
void *fromwire_fail(const u8 **cursor UNNEEDED, size_t *max UNNEEDED)
{ fprintf(stderr, "fromwire_fail called!\n"); abort(); }
/* Generated stub for fromwire_secp256k1_ecdsa_signature */
void fromwire_secp256k1_ecdsa_signature(const u8 **cursor UNNEEDED, size_t *max UNNEEDED,
					secp256k1_ecdsa_signature *signature UNNEEDED)
{ fprintf(stderr, "fromwire_secp256k1_ecdsa_signature called!\n"); abort(); }
/* Generated stub for fromwire_sha256 */
void fromwire_sha256(const u8 **cursor UNNEEDED, size_t *max UNNEEDED, struct sha256 *sha256 UNNEEDED)
{ fprintf(stderr, "fromwire_sha256 called!\n"); abort(); }
/* Generated stub for fromwire_tal_arrn */
u8 *fromwire_tal_arrn(const tal_t *ctx UNNEEDED,
		       const u8 **cursor UNNEEDED, size_t *max UNNEEDED, size_t num UNNEEDED)
{ fprintf(stderr, "fromwire_tal_arrn called!\n"); abort(); }
/* Generated stub for fromwire_u32 */
u32 fromwire_u32(const u8 **cursor UNNEEDED, size_t *max UNNEEDED)
{ fprintf(stderr, "fromwire_u32 called!\n"); abort(); }
/* Generated stub for fromwire_u64 */
u64 fromwire_u64(const u8 **cursor UNNEEDED, size_t *max UNNEEDED)
{ fprintf(stderr, "fromwire_u64 called!\n"); abort(); }
/* Generated stub for fromwire_u8 */
u8 fromwire_u8(const u8 **cursor UNNEEDED, size_t *max UNNEEDED)
{ fprintf(stderr, "fromwire_u8 called!\n"); abort(); }
/* Generated stub for fromwire_u8_array */
void fromwire_u8_array(const u8 **cursor UNNEEDED, size_t *max UNNEEDED, u8 *arr UNNEEDED, size_t num UNNEEDED)
{ fprintf(stderr, "fromwire_u8_array called!\n"); abort(); }
/* Generated stub for towire */
void towire(u8 **pptr UNNEEDED, const void *data UNNEEDED, size_t len UNNEEDED)
{ fprintf(stderr, "towire called!\n"); abort(); }
/* Generated stub for towire_bool */
void towire_bool(u8 **pptr UNNEEDED, bool v UNNEEDED)
{ fprintf(stderr, "towire_bool called!\n"); abort(); }
/* Generated stub for towire_secp256k1_ecdsa_signature */
void towire_secp256k1_ecdsa_signature(u8 **pptr UNNEEDED,
			      const secp256k1_ecdsa_signature *signature UNNEEDED)
{ fprintf(stderr, "towire_secp256k1_ecdsa_signature called!\n"); abort(); }
/* Generated stub for towire_sha256 */
void towire_sha256(u8 **pptr UNNEEDED, const struct sha256 *sha256 UNNEEDED)
{ fprintf(stderr, "towire_sha256 called!\n"); abort(); }
/* Generated stub for towire_u32 */
void towire_u32(u8 **pptr UNNEEDED, u32 v UNNEEDED)
{ fprintf(stderr, "towire_u32 called!\n"); abort(); }
/* Generated stub for towire_u64 */
void towire_u64(u8 **pptr UNNEEDED, u64 v UNNEEDED)
{ fprintf(stderr, "towire_u64 called!\n"); abort(); }
/* Generated stub for towire_u8 */
void towire_u8(u8 **pptr UNNEEDED, u8 v UNNEEDED)
{ fprintf(stderr, "towire_u8 called!\n"); abort(); }
/* Generated stub for towire_u8_array */
void towire_u8_array(u8 **pptr UNNEEDED, const u8 *arr UNNEEDED, size_t num UNNEEDED)
{ fprintf(stderr, "towire_u8_array called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

#define FAIL_MBRO(mbrop, str) \
	assert(!parse_amount_mbro((mbrop), (str), strlen(str)))
#define PASS_MBRO(mbrop, str, val)					\
	do {								\
		assert(parse_amount_mbro((mbrop), (str), strlen(str))); \
		assert((mbrop)->millibronees == val);			\
	} while (0)

#define FAIL_BRO(brop, str) \
	assert(!parse_amount_bro((brop), (str), strlen(str)))
#define PASS_BRO(brop, str, val)					\
	do {								\
		assert(parse_amount_bro((brop), (str), strlen(str)));	\
		assert((brop)->bronees == val);			\
	} while (0)

int main(int argc, char *argv[])
{
	struct amount_mbro mbro;
	struct amount_bro bro;

	common_setup(argv[0]);

	/* Grossly malformed */
	FAIL_MBRO(&mbro, "x");
	FAIL_MBRO(&mbro, "x100");

	PASS_MBRO(&mbro, "0", 0);
	PASS_MBRO(&mbro, "1", 1);
	PASS_MBRO(&mbro, "2100000000000000000", 2100000000000000000ULL);
	FAIL_MBRO(&mbro, "0.0");
	FAIL_MBRO(&mbro, "0.00000000");
	FAIL_MBRO(&mbro, "0.00000000000");
	FAIL_MBRO(&mbro, "0.00000000mbro");
	FAIL_MBRO(&mbro, "-1");

	PASS_MBRO(&mbro, "0mbro", 0);
	PASS_MBRO(&mbro, "1mbro", 1);
	PASS_MBRO(&mbro, "2100000000000000000mbro", 2100000000000000000ULL);
	FAIL_MBRO(&mbro, "-1mbro");

	PASS_MBRO(&mbro, "0bro", 0);
	PASS_MBRO(&mbro, "1bro", 1000);
	PASS_MBRO(&mbro, "2100000000000000bro", 2100000000000000000ULL);
	FAIL_MBRO(&mbro, "-1bro");

	PASS_MBRO(&mbro, "0.00000000bron", 0);
	PASS_MBRO(&mbro, "0.00000000000bron", 0);
	PASS_MBRO(&mbro, "0.00000001bron", 1000);
	PASS_MBRO(&mbro, "0.00000000001bron", 1);
	PASS_MBRO(&mbro, "1.2bron", 120000000000);
	PASS_MBRO(&mbro, "1.23bron", 123000000000);
	PASS_MBRO(&mbro, "1.234bron", 123400000000);
	PASS_MBRO(&mbro, "1.2345bron", 123450000000);
	PASS_MBRO(&mbro, "1.23456bron", 123456000000);
	PASS_MBRO(&mbro, "1.234567bron", 123456700000);
	PASS_MBRO(&mbro, "1.2345678bron", 123456780000);
	PASS_MBRO(&mbro, "1.23456789bron", 123456789000);
	PASS_MBRO(&mbro, "1.234567890bron", 123456789000);
	PASS_MBRO(&mbro, "1.2345678901bron", 123456789010);
	PASS_MBRO(&mbro, "1.23456789012bron", 123456789012);
	FAIL_MBRO(&mbro, "1.000000000000bron");
	FAIL_MBRO(&mbro, "-1.23456789bron");
	FAIL_MBRO(&mbro, "-1.23456789012bron");

	/* Overflowingly big. */
	FAIL_MBRO(&mbro, "21000000000000000000000000.00000000bron");

	/* Grossly malformed */
	FAIL_BRO(&bro, "x");
	FAIL_BRO(&bro, "x100");

	PASS_BRO(&bro, "0", 0);
	PASS_BRO(&bro, "1", 1);
	PASS_BRO(&bro, "2100000000000000", 2100000000000000ULL);
	FAIL_BRO(&bro, "0.0");
	FAIL_BRO(&bro, "0.00000000");
	FAIL_BRO(&bro, "0.00000000000");
	FAIL_BRO(&bro, "0.00000000bro");
	FAIL_BRO(&bro, "0.00000000000mbro");
	FAIL_BRO(&bro, "-1");

	PASS_BRO(&bro, "0bro", 0);
	PASS_BRO(&bro, "1bro", 1);
	PASS_BRO(&bro, "2100000000000000bro", 2100000000000000ULL);
	FAIL_BRO(&bro, "-1bro");

	PASS_BRO(&bro, "1000mbro", 1);
	PASS_BRO(&bro, "1000000mbro", 1000);
	PASS_BRO(&bro, "2100000000000000000mbro", 2100000000000000ULL);
	PASS_BRO(&bro, "0mbro", 0);
	FAIL_BRO(&bro, "100mbro");
	FAIL_BRO(&bro, "2000000000000000999mbro");
	FAIL_BRO(&bro, "-1000mbro");

	PASS_BRO(&bro, "0.00000000bron", 0);
	FAIL_BRO(&bro, "0.00000000000bron");
	PASS_BRO(&bro, "0.00000001bron", 1);
	FAIL_BRO(&bro, "0.00000000001bron");
	PASS_BRO(&bro, "1.23456789bron", 123456789);
	PASS_BRO(&bro, "1.2bron", 120000000);
	PASS_BRO(&bro, "1.23bron", 123000000);
	PASS_BRO(&bro, "1.234bron", 123400000);
	PASS_BRO(&bro, "1.2345bron", 123450000);
	PASS_BRO(&bro, "1.23456bron", 123456000);
	PASS_BRO(&bro, "1.234567bron", 123456700);
	PASS_BRO(&bro, "1.2345678bron", 123456780);
	PASS_BRO(&bro, "1.23456789bron", 123456789);
	FAIL_BRO(&bro, "1.234567890bron");
	FAIL_BRO(&bro, "-1.23456789bron");

	/* Overflowingly big. */
	FAIL_BRO(&bro, "21000000000000000000000000.00000000bron");

	/* Test fmt_amount_mbro_bron, fmt_amount_mbro */
	for (u64 i = 0; i <= UINT64_MAX / 10; i = i ? i * 10 : 1) {
		const char *with, *without;

		mbro.millibronees = i;
		with = fmt_amount_mbro_bron(tmpctx, mbro, true);
		without = fmt_amount_mbro_bron(tmpctx, mbro, false);
		assert(strends(with, "bron"));
		assert(strlen(with) == strlen(without) + 3);
		assert(strncmp(with, without, strlen(without)) == 0);
		/* Make sure it overwrites. */
		mbro.millibronees++;
		assert(parse_amount_mbro(&mbro, with, strlen(with)));
		assert(mbro.millibronees == i);

		with = fmt_amount_mbro(tmpctx, mbro);
		without = tal_fmt(tmpctx, "%"PRIu64, mbro.millibronees);
		assert(strends(with, "mbro"));
		assert(strlen(with) == strlen(without) + 4);
		assert(strncmp(with, without, strlen(without)) == 0);
		/* Make sure it overwrites. */
		mbro.millibronees++;
		assert(parse_amount_mbro(&mbro, with, strlen(with)));
		assert(mbro.millibronees == i);
	}

	/* Test fmt_amount_bro_bron, fmt_amount_bro */
	for (u64 i = 0; i <= UINT64_MAX / 10; i = i ? i * 10 : 1) {
		const char *with, *without;

		bro.bronees = i;
		with = fmt_amount_bro_bron(tmpctx, bro, true);
		without = fmt_amount_bro_bron(tmpctx, bro, false);
		assert(strends(with, "bron"));
		assert(strlen(with) == strlen(without) + 3);
		assert(strncmp(with, without, strlen(without)) == 0);
		/* Make sure it overwrites. */
		bro.bronees++;
		assert(parse_amount_bro(&bro, with, strlen(with)));
		assert(bro.bronees == i);

		with = fmt_amount_bro(tmpctx, bro);
		without = tal_fmt(tmpctx, "%"PRIu64, bro.bronees);
		assert(strends(with, "bro"));
		assert(strlen(with) == strlen(without) + 3);
		assert(strncmp(with, without, strlen(without)) == 0);
		/* Make sure it overwrites. */
		bro.bronees++;
		assert(parse_amount_bro(&bro, with, strlen(with)));
		assert(bro.bronees == i);
	}

	common_shutdown();
}
