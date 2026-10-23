/*
 * Lotus Navigator - macos9_woff2.c
 *
 * Copyright 2026 Chris Ruter
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * WOFF2 to sfnt decoder, written only from the W3C "WOFF File Format 2.0"
 * Recommendation and the OpenType table formats (clean room; see Lotus
 * Navigator docs/woff2-clean-room.md). Font collections are rejected
 * because the only caller cannot use them.
 */

#include <stdlib.h>
#include <string.h>

#include "macos9_brotli.h"
#include "macos9_woff2.h"

#define TAG(a, b, c, d) \
	(((unsigned long)(a) << 24) | ((unsigned long)(b) << 16) | \
	 ((unsigned long)(c) << 8) | (unsigned long)(d))

#define TAG_GLYF TAG('g', 'l', 'y', 'f')
#define TAG_LOCA TAG('l', 'o', 'c', 'a')
#define TAG_HMTX TAG('h', 'm', 't', 'x')
#define TAG_HHEA TAG('h', 'h', 'e', 'a')
#define TAG_HEAD TAG('h', 'e', 'a', 'd')
#define TAG_MAXP TAG('m', 'a', 'x', 'p')

#define WOFF2_SIGNATURE TAG('w', 'O', 'F', '2')
#define WOFF2_COLLECTION_FLAVOR TAG('t', 't', 'c', 'f')
#define WOFF2_HEADER_SIZE 48UL
#define WOFF2_EXPLICIT_TAG_INDEX 63U
#define WOFF2_TAG_INDEX_MASK 0x3FU
#define WOFF2_VERSION_SHIFT 6
#define WOFF2_VERSION_MASK 0x03U
#define WOFF2_GLYF_NULL_TRANSFORM 3U
#define WOFF2_HMTX_TRANSFORM 1U
#define WOFF2_MAX_SFNT_SIZE (32UL * 1024UL * 1024UL)
#define WOFF2_BASE128_MAX_BYTES 5
#define WOFF2_BASE128_OVERFLOW_BITS 0xFE000000UL

#define U255_WORD_CODE 253U
#define U255_ONE_MORE_BYTE_CODE_2 254U
#define U255_ONE_MORE_BYTE_CODE_1 255U
#define U255_LOWEST_UCODE 253U

#define GLYF_OPTION_OVERLAP_BITMAP 0x0001U
#define GLYF_INDEX_FORMAT_LONG 1U
#define GLYF_BBOX_BITMAP_WORD_BITS 32UL
#define GLYF_EMPTY_CONTOURS 0L
#define GLYF_COMPOSITE_CONTOURS (-1L)
#define GLYF_HEADER_SIZE 10UL
#define GLYF_MAX_POINTS 65536UL
#define GLYF_SHORT_DELTA_MAX 255L
#define GLYF_COORD_MIN (-32768L)
#define GLYF_COORD_MAX 32767L

#define POINT_ON_CURVE 0x01U
#define POINT_X_SHORT 0x02U
#define POINT_Y_SHORT 0x04U
#define POINT_X_SAME_OR_POSITIVE 0x10U
#define POINT_Y_SAME_OR_POSITIVE 0x20U
#define POINT_OVERLAP_SIMPLE 0x40U

#define TRIPLET_OFF_CURVE 0x80U
#define TRIPLET_INDEX_MASK 0x7FU

#define COMPONENT_ARGS_ARE_WORDS 0x0001U
#define COMPONENT_HAVE_SCALE 0x0008U
#define COMPONENT_MORE_COMPONENTS 0x0020U
#define COMPONENT_HAVE_XY_SCALE 0x0040U
#define COMPONENT_HAVE_2X2 0x0080U
#define COMPONENT_HAVE_INSTRUCTIONS 0x0100U

#define HMTX_NO_PROPORTIONAL_LSB 0x01U
#define HMTX_NO_MONOSPACED_LSB 0x02U
#define HMTX_RESERVED_FLAGS 0xFCU

#define SFNT_HEADER_SIZE 12UL
#define SFNT_RECORD_SIZE 16UL
#define SFNT_CHECKSUM_MAGIC 0xB1B0AFBAUL
#define SFNT_U32_MASK 0xFFFFFFFFUL
#define HEAD_ADJUSTMENT_OFFSET 8UL
#define HEAD_LOC_FORMAT_OFFSET 50UL
#define HEAD_MIN_SIZE 54UL
#define HHEA_NUM_METRICS_OFFSET 34UL
#define HHEA_MIN_SIZE 36UL
#define MAXP_NUM_GLYPHS_OFFSET 4UL
#define MAXP_MIN_SIZE 6UL

#define BUFFER_INITIAL_CAPACITY 4096UL

/* Table tags for directory flag indices 0-62 (spec section 4.1). */
static const unsigned long KNOWN_TAGS[63] = {
	TAG('c','m','a','p'), TAG('h','e','a','d'), TAG('h','h','e','a'),
	TAG('h','m','t','x'), TAG('m','a','x','p'), TAG('n','a','m','e'),
	TAG('O','S','/','2'), TAG('p','o','s','t'), TAG('c','v','t',' '),
	TAG('f','p','g','m'), TAG('g','l','y','f'), TAG('l','o','c','a'),
	TAG('p','r','e','p'), TAG('C','F','F',' '), TAG('V','O','R','G'),
	TAG('E','B','D','T'), TAG('E','B','L','C'), TAG('g','a','s','p'),
	TAG('h','d','m','x'), TAG('k','e','r','n'), TAG('L','T','S','H'),
	TAG('P','C','L','T'), TAG('V','D','M','X'), TAG('v','h','e','a'),
	TAG('v','m','t','x'), TAG('B','A','S','E'), TAG('G','D','E','F'),
	TAG('G','P','O','S'), TAG('G','S','U','B'), TAG('E','B','S','C'),
	TAG('J','S','T','F'), TAG('M','A','T','H'), TAG('C','B','D','T'),
	TAG('C','B','L','C'), TAG('C','O','L','R'), TAG('C','P','A','L'),
	TAG('S','V','G',' '), TAG('s','b','i','x'), TAG('a','c','n','t'),
	TAG('a','v','a','r'), TAG('b','d','a','t'), TAG('b','l','o','c'),
	TAG('b','s','l','n'), TAG('c','v','a','r'), TAG('f','d','s','c'),
	TAG('f','e','a','t'), TAG('f','m','t','x'), TAG('f','v','a','r'),
	TAG('g','v','a','r'), TAG('h','s','t','y'), TAG('j','u','s','t'),
	TAG('l','c','a','r'), TAG('m','o','r','t'), TAG('m','o','r','x'),
	TAG('o','p','b','d'), TAG('p','r','o','p'), TAG('t','r','a','k'),
	TAG('Z','a','p','f'), TAG('S','i','l','f'), TAG('G','l','a','t'),
	TAG('G','l','o','c'), TAG('F','e','a','t'), TAG('S','i','l','l')
};

/* One row of the spec's triplet encoding table (section 5.2): total bytes
 * including the flag byte, bit widths, base deltas and signs. */
typedef struct {
	unsigned char byte_count;
	unsigned char x_bits;
	unsigned char y_bits;
	unsigned short x_base;
	unsigned short y_base;
	signed char x_sign;
	signed char y_sign;
} woff2_triplet;

static const woff2_triplet TRIPLETS[128] = {
	{2, 0, 8, 0, 0, 0, -1},
	{2, 0, 8, 0, 0, 0, 1},
	{2, 0, 8, 0, 256, 0, -1},
	{2, 0, 8, 0, 256, 0, 1},
	{2, 0, 8, 0, 512, 0, -1},
	{2, 0, 8, 0, 512, 0, 1},
	{2, 0, 8, 0, 768, 0, -1},
	{2, 0, 8, 0, 768, 0, 1},
	{2, 0, 8, 0, 1024, 0, -1},
	{2, 0, 8, 0, 1024, 0, 1},
	{2, 8, 0, 0, 0, -1, 0},
	{2, 8, 0, 0, 0, 1, 0},
	{2, 8, 0, 256, 0, -1, 0},
	{2, 8, 0, 256, 0, 1, 0},
	{2, 8, 0, 512, 0, -1, 0},
	{2, 8, 0, 512, 0, 1, 0},
	{2, 8, 0, 768, 0, -1, 0},
	{2, 8, 0, 768, 0, 1, 0},
	{2, 8, 0, 1024, 0, -1, 0},
	{2, 8, 0, 1024, 0, 1, 0},
	{2, 4, 4, 1, 1, -1, -1},
	{2, 4, 4, 1, 1, 1, -1},
	{2, 4, 4, 1, 1, -1, 1},
	{2, 4, 4, 1, 1, 1, 1},
	{2, 4, 4, 1, 17, -1, -1},
	{2, 4, 4, 1, 17, 1, -1},
	{2, 4, 4, 1, 17, -1, 1},
	{2, 4, 4, 1, 17, 1, 1},
	{2, 4, 4, 1, 33, -1, -1},
	{2, 4, 4, 1, 33, 1, -1},
	{2, 4, 4, 1, 33, -1, 1},
	{2, 4, 4, 1, 33, 1, 1},
	{2, 4, 4, 1, 49, -1, -1},
	{2, 4, 4, 1, 49, 1, -1},
	{2, 4, 4, 1, 49, -1, 1},
	{2, 4, 4, 1, 49, 1, 1},
	{2, 4, 4, 17, 1, -1, -1},
	{2, 4, 4, 17, 1, 1, -1},
	{2, 4, 4, 17, 1, -1, 1},
	{2, 4, 4, 17, 1, 1, 1},
	{2, 4, 4, 17, 17, -1, -1},
	{2, 4, 4, 17, 17, 1, -1},
	{2, 4, 4, 17, 17, -1, 1},
	{2, 4, 4, 17, 17, 1, 1},
	{2, 4, 4, 17, 33, -1, -1},
	{2, 4, 4, 17, 33, 1, -1},
	{2, 4, 4, 17, 33, -1, 1},
	{2, 4, 4, 17, 33, 1, 1},
	{2, 4, 4, 17, 49, -1, -1},
	{2, 4, 4, 17, 49, 1, -1},
	{2, 4, 4, 17, 49, -1, 1},
	{2, 4, 4, 17, 49, 1, 1},
	{2, 4, 4, 33, 1, -1, -1},
	{2, 4, 4, 33, 1, 1, -1},
	{2, 4, 4, 33, 1, -1, 1},
	{2, 4, 4, 33, 1, 1, 1},
	{2, 4, 4, 33, 17, -1, -1},
	{2, 4, 4, 33, 17, 1, -1},
	{2, 4, 4, 33, 17, -1, 1},
	{2, 4, 4, 33, 17, 1, 1},
	{2, 4, 4, 33, 33, -1, -1},
	{2, 4, 4, 33, 33, 1, -1},
	{2, 4, 4, 33, 33, -1, 1},
	{2, 4, 4, 33, 33, 1, 1},
	{2, 4, 4, 33, 49, -1, -1},
	{2, 4, 4, 33, 49, 1, -1},
	{2, 4, 4, 33, 49, -1, 1},
	{2, 4, 4, 33, 49, 1, 1},
	{2, 4, 4, 49, 1, -1, -1},
	{2, 4, 4, 49, 1, 1, -1},
	{2, 4, 4, 49, 1, -1, 1},
	{2, 4, 4, 49, 1, 1, 1},
	{2, 4, 4, 49, 17, -1, -1},
	{2, 4, 4, 49, 17, 1, -1},
	{2, 4, 4, 49, 17, -1, 1},
	{2, 4, 4, 49, 17, 1, 1},
	{2, 4, 4, 49, 33, -1, -1},
	{2, 4, 4, 49, 33, 1, -1},
	{2, 4, 4, 49, 33, -1, 1},
	{2, 4, 4, 49, 33, 1, 1},
	{2, 4, 4, 49, 49, -1, -1},
	{2, 4, 4, 49, 49, 1, -1},
	{2, 4, 4, 49, 49, -1, 1},
	{2, 4, 4, 49, 49, 1, 1},
	{3, 8, 8, 1, 1, -1, -1},
	{3, 8, 8, 1, 1, 1, -1},
	{3, 8, 8, 1, 1, -1, 1},
	{3, 8, 8, 1, 1, 1, 1},
	{3, 8, 8, 1, 257, -1, -1},
	{3, 8, 8, 1, 257, 1, -1},
	{3, 8, 8, 1, 257, -1, 1},
	{3, 8, 8, 1, 257, 1, 1},
	{3, 8, 8, 1, 513, -1, -1},
	{3, 8, 8, 1, 513, 1, -1},
	{3, 8, 8, 1, 513, -1, 1},
	{3, 8, 8, 1, 513, 1, 1},
	{3, 8, 8, 257, 1, -1, -1},
	{3, 8, 8, 257, 1, 1, -1},
	{3, 8, 8, 257, 1, -1, 1},
	{3, 8, 8, 257, 1, 1, 1},
	{3, 8, 8, 257, 257, -1, -1},
	{3, 8, 8, 257, 257, 1, -1},
	{3, 8, 8, 257, 257, -1, 1},
	{3, 8, 8, 257, 257, 1, 1},
	{3, 8, 8, 257, 513, -1, -1},
	{3, 8, 8, 257, 513, 1, -1},
	{3, 8, 8, 257, 513, -1, 1},
	{3, 8, 8, 257, 513, 1, 1},
	{3, 8, 8, 513, 1, -1, -1},
	{3, 8, 8, 513, 1, 1, -1},
	{3, 8, 8, 513, 1, -1, 1},
	{3, 8, 8, 513, 1, 1, 1},
	{3, 8, 8, 513, 257, -1, -1},
	{3, 8, 8, 513, 257, 1, -1},
	{3, 8, 8, 513, 257, -1, 1},
	{3, 8, 8, 513, 257, 1, 1},
	{3, 8, 8, 513, 513, -1, -1},
	{3, 8, 8, 513, 513, 1, -1},
	{3, 8, 8, 513, 513, -1, 1},
	{3, 8, 8, 513, 513, 1, 1},
	{4, 12, 12, 0, 0, -1, -1},
	{4, 12, 12, 0, 0, 1, -1},
	{4, 12, 12, 0, 0, -1, 1},
	{4, 12, 12, 0, 0, 1, 1},
	{5, 16, 16, 0, 0, -1, -1},
	{5, 16, 16, 0, 0, 1, -1},
	{5, 16, 16, 0, 0, -1, 1},
	{5, 16, 16, 0, 0, 1, 1}
};

typedef struct {
	const unsigned char *data;
	unsigned long size;
	unsigned long pos;
} woff2_reader;

typedef struct {
	unsigned char *data;
	unsigned long size;
	unsigned long capacity;
} woff2_buffer;

typedef struct {
	unsigned long tag;
	int transformed;
	unsigned long orig_length;
	unsigned long stream_length;
	unsigned long stream_offset;
	const unsigned char *bytes;
	unsigned long length;
	unsigned char *owned;
} woff2_table;

typedef struct {
	unsigned long flavor;
	unsigned int num_tables;
	woff2_table *tables;
	unsigned char *stream;
	unsigned long stream_size;
	unsigned int num_glyphs;
	long *x_mins;
} woff2_font;

enum {
	STREAM_CONTOUR_COUNTS,
	STREAM_POINT_COUNTS,
	STREAM_FLAGS,
	STREAM_GLYPHS,
	STREAM_COMPOSITES,
	STREAM_BBOXES,
	STREAM_INSTRUCTIONS,
	GLYF_STREAM_COUNT
};

typedef struct {
	woff2_reader stream[GLYF_STREAM_COUNT];
	const unsigned char *bbox_bitmap;
	const unsigned char *overlap_bitmap;
	unsigned int num_glyphs;
	unsigned int index_format;
} glyf_streams;

typedef struct {
	long *x;
	long *y;
	unsigned char *on_curve;
	unsigned long point_capacity;
	unsigned long *end_points;
	unsigned long contour_capacity;
} glyph_points;

typedef struct {
	long x_min;
	long y_min;
	long x_max;
	long y_max;
} glyph_bbox;

/* ---- big-endian helpers ---------------------------------------------- */

static unsigned int get_u16(const unsigned char *p)
{
	return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static long get_s16(const unsigned char *p)
{
	unsigned int value = get_u16(p);
	return (value & 0x8000U) ? (long)value - 65536L : (long)value;
}

static unsigned long get_u32(const unsigned char *p)
{
	return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
		((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

static void put_u16(unsigned char *p, unsigned long value)
{
	p[0] = (unsigned char)((value >> 8) & 0xFF);
	p[1] = (unsigned char)(value & 0xFF);
}

static void put_u32(unsigned char *p, unsigned long value)
{
	p[0] = (unsigned char)((value >> 24) & 0xFF);
	p[1] = (unsigned char)((value >> 16) & 0xFF);
	p[2] = (unsigned char)((value >> 8) & 0xFF);
	p[3] = (unsigned char)(value & 0xFF);
}

static unsigned long align4(unsigned long value)
{
	return (value + 3UL) & ~3UL;
}

static int bit_is_set(const unsigned char *bitmap, unsigned int index)
{
	return (bitmap[index >> 3] & (0x80U >> (index & 7U))) != 0;
}

/* ---- bounded reader --------------------------------------------------- */

static void reader_init(woff2_reader *r, const unsigned char *data,
		unsigned long size)
{
	r->data = data;
	r->size = size;
	r->pos = 0;
}

static int read_bytes(woff2_reader *r, unsigned long count,
		const unsigned char **bytes)
{
	if (count > r->size - r->pos)
		return 0;
	*bytes = r->data + r->pos;
	r->pos += count;
	return 1;
}

static int read_u8(woff2_reader *r, unsigned int *value)
{
	const unsigned char *p;
	if (!read_bytes(r, 1, &p))
		return 0;
	*value = p[0];
	return 1;
}

static int read_u16(woff2_reader *r, unsigned int *value)
{
	const unsigned char *p;
	if (!read_bytes(r, 2, &p))
		return 0;
	*value = get_u16(p);
	return 1;
}

static int read_s16(woff2_reader *r, long *value)
{
	const unsigned char *p;
	if (!read_bytes(r, 2, &p))
		return 0;
	*value = get_s16(p);
	return 1;
}

static int read_u32(woff2_reader *r, unsigned long *value)
{
	const unsigned char *p;
	if (!read_bytes(r, 4, &p))
		return 0;
	*value = get_u32(p);
	return 1;
}

/* UIntBase128 (spec section 3): no leading zero byte, at most five bytes,
 * value below 2^32. */
static int read_base128(woff2_reader *r, unsigned long *value)
{
	unsigned long accumulated = 0;
	unsigned int byte;
	int i;

	for (i = 0; i < WOFF2_BASE128_MAX_BYTES; i++) {
		if (!read_u8(r, &byte))
			return 0;
		if (i == 0 && byte == 0x80U)
			return 0;
		if (accumulated & WOFF2_BASE128_OVERFLOW_BITS)
			return 0;
		accumulated = (accumulated << 7) | (byte & 0x7FU);
		if ((byte & 0x80U) == 0) {
			*value = accumulated;
			return 1;
		}
	}
	return 0;
}

/* 255UInt16 (spec section 3). */
static int read_255_u16(woff2_reader *r, unsigned int *value)
{
	unsigned int code;
	unsigned int extra;

	if (!read_u8(r, &code))
		return 0;
	if (code == U255_WORD_CODE)
		return read_u16(r, value);
	if (code == U255_ONE_MORE_BYTE_CODE_1) {
		if (!read_u8(r, &extra))
			return 0;
		*value = extra + U255_LOWEST_UCODE;
		return 1;
	}
	if (code == U255_ONE_MORE_BYTE_CODE_2) {
		if (!read_u8(r, &extra))
			return 0;
		*value = extra + U255_LOWEST_UCODE * 2U;
		return 1;
	}
	*value = code;
	return 1;
}

/* ---- growable output buffer ------------------------------------------- */

static int buffer_reserve(woff2_buffer *b, unsigned long extra)
{
	unsigned long needed;
	unsigned long capacity;
	unsigned char *grown;

	if (extra > WOFF2_MAX_SFNT_SIZE - b->size)
		return 0;
	needed = b->size + extra;
	if (needed <= b->capacity)
		return 1;
	capacity = b->capacity ? b->capacity : BUFFER_INITIAL_CAPACITY;
	while (capacity < needed)
		capacity *= 2;
	grown = (unsigned char *)realloc(b->data, capacity);
	if (grown == NULL)
		return 0;
	b->data = grown;
	b->capacity = capacity;
	return 1;
}

static int buffer_append(woff2_buffer *b, const unsigned char *bytes,
		unsigned long count)
{
	if (!buffer_reserve(b, count))
		return 0;
	if (count > 0)
		memcpy(b->data + b->size, bytes, count);
	b->size += count;
	return 1;
}

static int buffer_append_u8(woff2_buffer *b, unsigned long value)
{
	unsigned char byte = (unsigned char)(value & 0xFF);
	return buffer_append(b, &byte, 1);
}

static int buffer_append_u16(woff2_buffer *b, unsigned long value)
{
	unsigned char bytes[2];
	put_u16(bytes, value);
	return buffer_append(b, bytes, 2);
}

static int buffer_pad4(woff2_buffer *b)
{
	static const unsigned char zeros[3] = {0, 0, 0};
	return buffer_append(b, zeros, align4(b->size) - b->size);
}

/* ---- table directory ---------------------------------------------------- */

/* Returns 1 for a transformed table, 0 for the null transform, -1 for a
 * transform version the spec does not define for this tag (section 4.1). */
static int table_transform(unsigned long tag, unsigned int version)
{
	if (tag == TAG_GLYF || tag == TAG_LOCA) {
		if (version == 0)
			return 1;
		if (version == WOFF2_GLYF_NULL_TRANSFORM)
			return 0;
		return -1;
	}
	if (version == 0)
		return 0;
	if (tag == TAG_HMTX && version == WOFF2_HMTX_TRANSFORM)
		return 1;
	return -1;
}

static int read_table_entry(woff2_reader *r, woff2_table *table)
{
	unsigned int flags;
	unsigned int tag_index;

	if (!read_u8(r, &flags))
		return 0;
	tag_index = flags & WOFF2_TAG_INDEX_MASK;
	if (tag_index == WOFF2_EXPLICIT_TAG_INDEX) {
		if (!read_u32(r, &table->tag))
			return 0;
	} else {
		table->tag = KNOWN_TAGS[tag_index];
	}
	table->transformed = table_transform(table->tag,
			(flags >> WOFF2_VERSION_SHIFT) & WOFF2_VERSION_MASK);
	if (table->transformed < 0)
		return 0;
	if (!read_base128(r, &table->orig_length))
		return 0;
	if (!table->transformed) {
		table->stream_length = table->orig_length;
		return 1;
	}
	if (!read_base128(r, &table->stream_length))
		return 0;
	return table->tag != TAG_LOCA || table->stream_length == 0;
}

static woff2_table *find_table(woff2_font *font, unsigned long tag)
{
	unsigned int i;
	for (i = 0; i < font->num_tables; i++) {
		if (font->tables[i].tag == tag)
			return &font->tables[i];
	}
	return NULL;
}

static int has_duplicate_tags(const woff2_font *font)
{
	unsigned int i;
	unsigned int j;
	for (i = 0; i < font->num_tables; i++) {
		for (j = i + 1; j < font->num_tables; j++) {
			if (font->tables[i].tag == font->tables[j].tag)
				return 1;
		}
	}
	return 0;
}

/* Lays the tables out in the decompressed stream in directory order with
 * no gaps (section 5) and returns the total stream size. */
static int assign_stream_offsets(woff2_font *font)
{
	unsigned long offset = 0;
	unsigned int i;

	for (i = 0; i < font->num_tables; i++) {
		woff2_table *table = &font->tables[i];
		if (table->stream_length > WOFF2_MAX_SFNT_SIZE - offset)
			return 0;
		table->stream_offset = offset;
		offset += table->stream_length;
	}
	font->stream_size = offset;
	return 1;
}

/* ---- glyf / loca (section 5.1 - 5.3) ---------------------------------- */

static int read_glyf_streams(const unsigned char *data, unsigned long size,
		glyf_streams *streams)
{
	woff2_reader r;
	unsigned long stream_sizes[GLYF_STREAM_COUNT];
	unsigned int reserved;
	unsigned int option_flags;
	unsigned long bitmap_size;
	const unsigned char *bytes;
	int i;

	reader_init(&r, data, size);
	if (!read_u16(&r, &reserved) || !read_u16(&r, &option_flags) ||
			!read_u16(&r, &streams->num_glyphs) ||
			!read_u16(&r, &streams->index_format))
		return 0;
	if (streams->index_format > GLYF_INDEX_FORMAT_LONG)
		return 0;
	for (i = 0; i < GLYF_STREAM_COUNT; i++) {
		if (!read_u32(&r, &stream_sizes[i]))
			return 0;
	}
	for (i = 0; i < GLYF_STREAM_COUNT; i++) {
		if (!read_bytes(&r, stream_sizes[i], &bytes))
			return 0;
		reader_init(&streams->stream[i], bytes, stream_sizes[i]);
	}

	bitmap_size = 4UL * (((unsigned long)streams->num_glyphs +
			GLYF_BBOX_BITMAP_WORD_BITS - 1) / GLYF_BBOX_BITMAP_WORD_BITS);
	if (!read_bytes(&streams->stream[STREAM_BBOXES], bitmap_size,
			&streams->bbox_bitmap))
		return 0;

	streams->overlap_bitmap = NULL;
	if (option_flags & GLYF_OPTION_OVERLAP_BITMAP) {
		if (!read_bytes(&r, ((unsigned long)streams->num_glyphs + 7) / 8,
				&streams->overlap_bitmap))
			return 0;
	}
	return 1;
}

static int grow_points(glyph_points *points, unsigned long point_count,
		unsigned long contour_count)
{
	if (point_count > points->point_capacity) {
		long *x = (long *)realloc(points->x, point_count * sizeof(long));
		long *y;
		unsigned char *on_curve;
		if (x == NULL)
			return 0;
		points->x = x;
		y = (long *)realloc(points->y, point_count * sizeof(long));
		if (y == NULL)
			return 0;
		points->y = y;
		on_curve = (unsigned char *)realloc(points->on_curve, point_count);
		if (on_curve == NULL)
			return 0;
		points->on_curve = on_curve;
		points->point_capacity = point_count;
	}
	if (contour_count > points->contour_capacity) {
		unsigned long *end_points = (unsigned long *)realloc(
				points->end_points,
				contour_count * sizeof(unsigned long));
		if (end_points == NULL)
			return 0;
		points->end_points = end_points;
		points->contour_capacity = contour_count;
	}
	return 1;
}

static void free_points(glyph_points *points)
{
	free(points->x);
	free(points->y);
	free(points->on_curve);
	free(points->end_points);
}

static int decode_triplet(woff2_reader *glyphs, unsigned int flag,
		long *dx, long *dy)
{
	const woff2_triplet *triplet = &TRIPLETS[flag & TRIPLET_INDEX_MASK];
	const unsigned char *bytes;
	unsigned long packed = 0;
	unsigned long x_raw;
	unsigned long y_raw;
	unsigned int i;

	if (!read_bytes(glyphs, (unsigned long)triplet->byte_count - 1, &bytes))
		return 0;
	for (i = 0; i + 1 < triplet->byte_count; i++)
		packed = (packed << 8) | bytes[i];
	x_raw = (packed >> triplet->y_bits) & ((1UL << triplet->x_bits) - 1);
	y_raw = packed & ((1UL << triplet->y_bits) - 1);
	*dx = (long)triplet->x_sign * (long)(x_raw + triplet->x_base);
	*dy = (long)triplet->y_sign * (long)(y_raw + triplet->y_base);
	return 1;
}

static int read_bbox(woff2_reader *bboxes, glyph_bbox *bbox)
{
	return read_s16(bboxes, &bbox->x_min) && read_s16(bboxes, &bbox->y_min) &&
		read_s16(bboxes, &bbox->x_max) && read_s16(bboxes, &bbox->y_max);
}

static void compute_bbox(const glyph_points *points, unsigned long count,
		glyph_bbox *bbox)
{
	unsigned long i;

	memset(bbox, 0, sizeof(*bbox));
	for (i = 0; i < count; i++) {
		if (i == 0 || points->x[i] < bbox->x_min)
			bbox->x_min = points->x[i];
		if (i == 0 || points->y[i] < bbox->y_min)
			bbox->y_min = points->y[i];
		if (i == 0 || points->x[i] > bbox->x_max)
			bbox->x_max = points->x[i];
		if (i == 0 || points->y[i] > bbox->y_max)
			bbox->y_max = points->y[i];
	}
}

static int append_glyph_header(woff2_buffer *glyf, long contours,
		const glyph_bbox *bbox)
{
	return buffer_append_u16(glyf, (unsigned long)contours & 0xFFFFUL) &&
		buffer_append_u16(glyf, (unsigned long)bbox->x_min & 0xFFFFUL) &&
		buffer_append_u16(glyf, (unsigned long)bbox->y_min & 0xFFFFUL) &&
		buffer_append_u16(glyf, (unsigned long)bbox->x_max & 0xFFFFUL) &&
		buffer_append_u16(glyf, (unsigned long)bbox->y_max & 0xFFFFUL);
}

static int read_instructions(glyf_streams *streams, unsigned int *length,
		const unsigned char **bytes)
{
	return read_255_u16(&streams->stream[STREAM_GLYPHS], length) &&
		read_bytes(&streams->stream[STREAM_INSTRUCTIONS], *length, bytes);
}

static int is_short_delta(long delta)
{
	return delta >= -GLYF_SHORT_DELTA_MAX && delta <= GLYF_SHORT_DELTA_MAX;
}

static unsigned int coordinate_flags(long delta, unsigned int short_flag,
		unsigned int same_or_positive_flag)
{
	if (delta == 0)
		return same_or_positive_flag;
	if (is_short_delta(delta))
		return delta > 0 ? short_flag | same_or_positive_flag : short_flag;
	return 0;
}

static int append_coordinate(woff2_buffer *glyf, long delta)
{
	if (delta == 0)
		return 1;
	if (is_short_delta(delta))
		return buffer_append_u8(glyf, (unsigned long)(delta < 0 ? -delta : delta));
	return buffer_append_u16(glyf, (unsigned long)delta & 0xFFFFUL);
}

static int append_simple_glyph(woff2_buffer *glyf, const glyph_points *points,
		long contours, unsigned long point_count, const glyph_bbox *bbox,
		unsigned int instruction_length, const unsigned char *instructions,
		int overlap)
{
	unsigned long i;
	unsigned int flags;
	long previous;

	if (!append_glyph_header(glyf, contours, bbox))
		return 0;
	for (i = 0; i < (unsigned long)contours; i++) {
		if (!buffer_append_u16(glyf, points->end_points[i]))
			return 0;
	}
	if (!buffer_append_u16(glyf, instruction_length) ||
			!buffer_append(glyf, instructions, instruction_length))
		return 0;
	for (i = 0; i < point_count; i++) {
		long dx = points->x[i] - (i ? points->x[i - 1] : 0);
		long dy = points->y[i] - (i ? points->y[i - 1] : 0);
		flags = coordinate_flags(dx, POINT_X_SHORT, POINT_X_SAME_OR_POSITIVE) |
			coordinate_flags(dy, POINT_Y_SHORT, POINT_Y_SAME_OR_POSITIVE);
		if (points->on_curve[i])
			flags |= POINT_ON_CURVE;
		if (i == 0 && overlap)
			flags |= POINT_OVERLAP_SIMPLE;
		if (!buffer_append_u8(glyf, flags))
			return 0;
	}
	previous = 0;
	for (i = 0; i < point_count; i++) {
		if (!append_coordinate(glyf, points->x[i] - previous))
			return 0;
		previous = points->x[i];
	}
	previous = 0;
	for (i = 0; i < point_count; i++) {
		if (!append_coordinate(glyf, points->y[i] - previous))
			return 0;
		previous = points->y[i];
	}
	return 1;
}

static int decode_simple_glyph(glyf_streams *streams, unsigned int glyph,
		long contours, glyph_points *points, woff2_buffer *glyf,
		long *x_min)
{
	unsigned long point_count = 0;
	unsigned long i;
	unsigned int count;
	unsigned int flag;
	unsigned int instruction_length;
	const unsigned char *instructions;
	long x = 0;
	long y = 0;
	long dx;
	long dy;
	glyph_bbox bbox;
	int overlap;

	if (!grow_points(points, 0, (unsigned long)contours))
		return 0;
	for (i = 0; i < (unsigned long)contours; i++) {
		if (!read_255_u16(&streams->stream[STREAM_POINT_COUNTS], &count))
			return 0;
		point_count += count;
		if (point_count == 0 || point_count > GLYF_MAX_POINTS)
			return 0;
		points->end_points[i] = point_count - 1;
	}
	if (!grow_points(points, point_count, 0))
		return 0;
	for (i = 0; i < point_count; i++) {
		if (!read_u8(&streams->stream[STREAM_FLAGS], &flag) ||
				!decode_triplet(&streams->stream[STREAM_GLYPHS], flag,
					&dx, &dy))
			return 0;
		x += dx;
		y += dy;
		if (x < GLYF_COORD_MIN || x > GLYF_COORD_MAX ||
				y < GLYF_COORD_MIN || y > GLYF_COORD_MAX)
			return 0;
		points->x[i] = x;
		points->y[i] = y;
		points->on_curve[i] = (flag & TRIPLET_OFF_CURVE) ? 0 : 1;
	}
	if (!read_instructions(streams, &instruction_length, &instructions))
		return 0;
	if (bit_is_set(streams->bbox_bitmap, glyph)) {
		if (!read_bbox(&streams->stream[STREAM_BBOXES], &bbox))
			return 0;
	} else {
		compute_bbox(points, point_count, &bbox);
	}
	overlap = streams->overlap_bitmap != NULL &&
		bit_is_set(streams->overlap_bitmap, glyph);
	*x_min = bbox.x_min;
	return append_simple_glyph(glyf, points, contours, point_count, &bbox,
			instruction_length, instructions, overlap);
}

/* Bytes after the flags word of one composite component (OpenType glyf). */
static unsigned long component_argument_size(unsigned int flags)
{
	unsigned long size = 2 + ((flags & COMPONENT_ARGS_ARE_WORDS) ? 4 : 2);
	if (flags & COMPONENT_HAVE_SCALE)
		size += 2;
	else if (flags & COMPONENT_HAVE_XY_SCALE)
		size += 4;
	else if (flags & COMPONENT_HAVE_2X2)
		size += 8;
	return size;
}

static int decode_composite_glyph(glyf_streams *streams, unsigned int glyph,
		woff2_buffer *glyf, long *x_min)
{
	woff2_reader *composites = &streams->stream[STREAM_COMPOSITES];
	unsigned long start = composites->pos;
	unsigned int flags;
	unsigned int instruction_length = 0;
	const unsigned char *instructions = NULL;
	const unsigned char *arguments;
	int have_instructions = 0;
	glyph_bbox bbox;

	if (!bit_is_set(streams->bbox_bitmap, glyph) ||
			!read_bbox(&streams->stream[STREAM_BBOXES], &bbox))
		return 0;
	do {
		if (!read_u16(composites, &flags) ||
				!read_bytes(composites, component_argument_size(flags),
					&arguments))
			return 0;
		if (flags & COMPONENT_HAVE_INSTRUCTIONS)
			have_instructions = 1;
	} while (flags & COMPONENT_MORE_COMPONENTS);
	if (have_instructions &&
			!read_instructions(streams, &instruction_length, &instructions))
		return 0;

	*x_min = bbox.x_min;
	if (!append_glyph_header(glyf, GLYF_COMPOSITE_CONTOURS, &bbox) ||
			!buffer_append(glyf, composites->data + start,
				composites->pos - start))
		return 0;
	if (!have_instructions)
		return 1;
	return buffer_append_u16(glyf, instruction_length) &&
		buffer_append(glyf, instructions, instruction_length);
}

static int decode_glyph(glyf_streams *streams, unsigned int glyph,
		glyph_points *points, woff2_buffer *glyf, long *x_min)
{
	long contours;

	if (!read_s16(&streams->stream[STREAM_CONTOUR_COUNTS], &contours))
		return 0;
	if (contours == GLYF_EMPTY_CONTOURS) {
		*x_min = 0;
		return !bit_is_set(streams->bbox_bitmap, glyph);
	}
	if (contours > 0)
		return decode_simple_glyph(streams, glyph, contours, points, glyf,
				x_min);
	if (contours == GLYF_COMPOSITE_CONTOURS)
		return decode_composite_glyph(streams, glyph, glyf, x_min);
	return 0;
}

static int append_loca_entry(woff2_buffer *loca, unsigned long offset,
		unsigned int index_format)
{
	unsigned char bytes[4];

	if (index_format == GLYF_INDEX_FORMAT_LONG) {
		put_u32(bytes, offset);
		return buffer_append(loca, bytes, 4);
	}
	if (offset / 2 > 0xFFFFUL)
		return 0;
	return buffer_append_u16(loca, offset / 2);
}

static int head_matches_index_format(woff2_font *font,
		unsigned int index_format)
{
	woff2_table *head = find_table(font, TAG_HEAD);
	if (head == NULL || head->length < HEAD_MIN_SIZE)
		return 1;
	return get_u16(head->bytes + HEAD_LOC_FORMAT_OFFSET) == index_format;
}

static int rebuild_glyf_and_loca(woff2_font *font, woff2_table *glyf_table,
		woff2_table *loca_table)
{
	glyf_streams streams;
	glyph_points points;
	woff2_buffer glyf;
	woff2_buffer loca;
	unsigned long entry_size;
	unsigned int glyph;
	int ok = 0;

	memset(&points, 0, sizeof(points));
	memset(&glyf, 0, sizeof(glyf));
	memset(&loca, 0, sizeof(loca));

	if (!read_glyf_streams(glyf_table->bytes, glyf_table->stream_length,
			&streams))
		return 0;
	entry_size = streams.index_format == GLYF_INDEX_FORMAT_LONG ? 4 : 2;
	if (loca_table->orig_length !=
			((unsigned long)streams.num_glyphs + 1) * entry_size)
		return 0;
	if (!head_matches_index_format(font, streams.index_format))
		return 0;

	font->num_glyphs = streams.num_glyphs;
	font->x_mins = (long *)malloc(((unsigned long)streams.num_glyphs + 1) *
			sizeof(long));
	if (font->x_mins == NULL)
		return 0;

	for (glyph = 0; glyph < streams.num_glyphs; glyph++) {
		if (!append_loca_entry(&loca, glyf.size, streams.index_format) ||
				!decode_glyph(&streams, glyph, &points, &glyf,
					&font->x_mins[glyph]) ||
				!buffer_pad4(&glyf))
			goto done;
	}
	if (!append_loca_entry(&loca, glyf.size, streams.index_format))
		goto done;

	glyf_table->owned = glyf.data;
	glyf_table->bytes = glyf.data;
	glyf_table->length = glyf.size;
	glyf.data = NULL;
	loca_table->owned = loca.data;
	loca_table->bytes = loca.data;
	loca_table->length = loca.size;
	loca.data = NULL;
	ok = 1;
done:
	free(glyf.data);
	free(loca.data);
	free_points(&points);
	return ok;
}

/* xMin of every glyph read from an untransformed glyf table. */
static int read_x_mins(woff2_font *font)
{
	woff2_table *glyf = find_table(font, TAG_GLYF);
	woff2_table *loca = find_table(font, TAG_LOCA);
	woff2_table *head = find_table(font, TAG_HEAD);
	woff2_table *maxp = find_table(font, TAG_MAXP);
	unsigned long entry_size;
	unsigned long start;
	unsigned long end;
	unsigned int glyph;
	int long_offsets;

	if (glyf == NULL || loca == NULL || head == NULL || maxp == NULL ||
			head->length < HEAD_MIN_SIZE || maxp->length < MAXP_MIN_SIZE)
		return 0;
	font->num_glyphs = get_u16(maxp->bytes + MAXP_NUM_GLYPHS_OFFSET);
	long_offsets = get_u16(head->bytes + HEAD_LOC_FORMAT_OFFSET) != 0;
	entry_size = long_offsets ? 4 : 2;
	if (loca->length < ((unsigned long)font->num_glyphs + 1) * entry_size)
		return 0;
	font->x_mins = (long *)malloc(((unsigned long)font->num_glyphs + 1) *
			sizeof(long));
	if (font->x_mins == NULL)
		return 0;
	for (glyph = 0; glyph < font->num_glyphs; glyph++) {
		const unsigned char *entry = loca->bytes + glyph * entry_size;
		start = long_offsets ? get_u32(entry) : get_u16(entry) * 2UL;
		end = long_offsets ? get_u32(entry + 4) : get_u16(entry + 2) * 2UL;
		if (end < start || end > glyf->length)
			return 0;
		font->x_mins[glyph] = end - start >= GLYF_HEADER_SIZE ?
			get_s16(glyf->bytes + start + 2) : 0;
	}
	return 1;
}

/* ---- hmtx (section 5.4) ------------------------------------------------- */

static int rebuild_hmtx(woff2_font *font, woff2_table *hmtx)
{
	woff2_table *hhea = find_table(font, TAG_HHEA);
	woff2_reader r;
	unsigned int flags;
	unsigned int num_metrics;
	unsigned long length;
	unsigned long glyph;
	const unsigned char *advances;
	const unsigned char *proportional_lsbs = NULL;
	const unsigned char *monospaced_lsbs = NULL;
	unsigned char *out;
	unsigned char *p;

	if (hhea == NULL || hhea->length < HHEA_MIN_SIZE)
		return 0;
	if (font->x_mins == NULL && !read_x_mins(font))
		return 0;
	num_metrics = get_u16(hhea->bytes + HHEA_NUM_METRICS_OFFSET);
	if (num_metrics < 1 || num_metrics > font->num_glyphs)
		return 0;
	length = 4UL * num_metrics + 2UL * (font->num_glyphs - num_metrics);
	if (length != hmtx->orig_length)
		return 0;

	reader_init(&r, hmtx->bytes, hmtx->stream_length);
	if (!read_u8(&r, &flags))
		return 0;
	if ((flags & HMTX_RESERVED_FLAGS) ||
			(flags & (HMTX_NO_PROPORTIONAL_LSB | HMTX_NO_MONOSPACED_LSB)) == 0)
		return 0;
	if (!read_bytes(&r, 2UL * num_metrics, &advances))
		return 0;
	if (!(flags & HMTX_NO_PROPORTIONAL_LSB) &&
			!read_bytes(&r, 2UL * num_metrics, &proportional_lsbs))
		return 0;
	if (!(flags & HMTX_NO_MONOSPACED_LSB) &&
			!read_bytes(&r, 2UL * (font->num_glyphs - num_metrics),
				&monospaced_lsbs))
		return 0;

	out = (unsigned char *)malloc(length);
	if (out == NULL)
		return 0;
	p = out;
	for (glyph = 0; glyph < num_metrics; glyph++) {
		memcpy(p, advances + glyph * 2, 2);
		if (proportional_lsbs != NULL)
			memcpy(p + 2, proportional_lsbs + glyph * 2, 2);
		else
			put_u16(p + 2, (unsigned long)font->x_mins[glyph] & 0xFFFFUL);
		p += 4;
	}
	for (glyph = num_metrics; glyph < font->num_glyphs; glyph++) {
		if (monospaced_lsbs != NULL)
			memcpy(p, monospaced_lsbs + (glyph - num_metrics) * 2, 2);
		else
			put_u16(p, (unsigned long)font->x_mins[glyph] & 0xFFFFUL);
		p += 2;
	}
	hmtx->owned = out;
	hmtx->bytes = out;
	hmtx->length = length;
	return 1;
}

/* ---- decode ------------------------------------------------------------- */

static int read_header(woff2_reader *r, woff2_font *font,
		unsigned long src_len, unsigned long *compressed_size)
{
	unsigned long signature;
	unsigned long length;
	unsigned long ignored32;
	unsigned int ignored16;

	if (!read_u32(r, &signature) || signature != WOFF2_SIGNATURE ||
			!read_u32(r, &font->flavor) ||
			font->flavor == WOFF2_COLLECTION_FLAVOR ||
			!read_u32(r, &length) || length < WOFF2_HEADER_SIZE ||
			length > src_len ||
			!read_u16(r, &font->num_tables) || font->num_tables == 0 ||
			!read_u16(r, &ignored16) ||        /* reserved */
			!read_u32(r, &ignored32) ||        /* totalSfntSize */
			!read_u32(r, compressed_size))
		return 0;
	r->size = length;
	r->pos = WOFF2_HEADER_SIZE;
	return 1;
}

static int decompress_tables(woff2_reader *r, woff2_font *font,
		unsigned long compressed_size)
{
	const unsigned char *compressed;
	size_t decoded_size = font->stream_size;

	if (!read_bytes(r, compressed_size, &compressed))
		return 0;
	font->stream = (unsigned char *)malloc(font->stream_size ?
			font->stream_size : 1);
	if (font->stream == NULL)
		return 0;
	if (BrotliDecoderDecompress(compressed_size, compressed, &decoded_size,
			font->stream) != BROTLI_DECODER_RESULT_SUCCESS)
		return 0;
	return decoded_size == font->stream_size;
}

static int rebuild_transformed_tables(woff2_font *font)
{
	woff2_table *glyf = find_table(font, TAG_GLYF);
	woff2_table *loca = find_table(font, TAG_LOCA);
	woff2_table *hmtx = find_table(font, TAG_HMTX);
	int glyf_transformed = glyf != NULL && glyf->transformed;
	int loca_transformed = loca != NULL && loca->transformed;

	if (glyf_transformed != loca_transformed)
		return 0;
	if (glyf_transformed && !rebuild_glyf_and_loca(font, glyf, loca))
		return 0;
	if (hmtx != NULL && hmtx->transformed && !rebuild_hmtx(font, hmtx))
		return 0;
	return 1;
}

static int decode_font(woff2_font *font, const unsigned char *src,
		unsigned long src_len)
{
	woff2_reader r;
	unsigned long compressed_size;
	unsigned int i;

	reader_init(&r, src, src_len);
	if (!read_header(&r, font, src_len, &compressed_size))
		return 0;
	font->tables = (woff2_table *)calloc(font->num_tables,
			sizeof(woff2_table));
	if (font->tables == NULL)
		return 0;
	for (i = 0; i < font->num_tables; i++) {
		if (!read_table_entry(&r, &font->tables[i]))
			return 0;
	}
	if (has_duplicate_tags(font) || !assign_stream_offsets(font) ||
			!decompress_tables(&r, font, compressed_size))
		return 0;
	for (i = 0; i < font->num_tables; i++) {
		woff2_table *table = &font->tables[i];
		table->bytes = font->stream + table->stream_offset;
		table->length = table->stream_length;
	}
	return rebuild_transformed_tables(font);
}

static void free_font(woff2_font *font)
{
	unsigned int i;

	if (font->tables != NULL) {
		for (i = 0; i < font->num_tables; i++)
			free(font->tables[i].owned);
		free(font->tables);
	}
	free(font->stream);
	free(font->x_mins);
}

/* ---- sfnt output -------------------------------------------------------- */

static unsigned long sfnt_checksum(const unsigned char *data,
		unsigned long length)
{
	unsigned long sum = 0;
	unsigned long i;

	for (i = 0; i < align4(length); i += 4)
		sum = (sum + get_u32(data + i)) & SFNT_U32_MASK;
	return sum;
}

static unsigned int *sort_tables_by_tag(const woff2_font *font)
{
	unsigned int *order;
	unsigned int i;
	unsigned int j;
	unsigned int moving;

	order = (unsigned int *)malloc(font->num_tables * sizeof(unsigned int));
	if (order == NULL)
		return NULL;
	for (i = 0; i < font->num_tables; i++) {
		moving = i;
		for (j = i; j > 0 &&
				font->tables[order[j - 1]].tag > font->tables[moving].tag; j--)
			order[j] = order[j - 1];
		order[j] = moving;
	}
	return order;
}

static void write_sfnt_header(unsigned char *out, const woff2_font *font)
{
	unsigned long entry_selector = 0;
	unsigned long search_range;

	while ((1UL << (entry_selector + 1)) <= font->num_tables)
		entry_selector++;
	search_range = (1UL << entry_selector) * SFNT_RECORD_SIZE;
	put_u32(out, font->flavor);
	put_u16(out + 4, font->num_tables);
	put_u16(out + 6, search_range);
	put_u16(out + 8, entry_selector);
	put_u16(out + 10, font->num_tables * SFNT_RECORD_SIZE - search_range);
}

static int write_sfnt(const woff2_font *font, unsigned char **out,
		long *out_len)
{
	unsigned int *order = sort_tables_by_tag(font);
	unsigned char *sfnt;
	unsigned char *head = NULL;
	unsigned long total;
	unsigned long offset;
	unsigned int i;

	if (order == NULL)
		return 0;
	total = SFNT_HEADER_SIZE + SFNT_RECORD_SIZE * font->num_tables;
	for (i = 0; i < font->num_tables; i++) {
		unsigned long length = font->tables[i].length;
		if (length > WOFF2_MAX_SFNT_SIZE - align4(total)) {
			free(order);
			return 0;
		}
		total = align4(total) + length;
	}
	total = align4(total);
	sfnt = (unsigned char *)calloc(total, 1);
	if (sfnt == NULL) {
		free(order);
		return 0;
	}

	write_sfnt_header(sfnt, font);
	offset = SFNT_HEADER_SIZE + SFNT_RECORD_SIZE * font->num_tables;
	for (i = 0; i < font->num_tables; i++) {
		const woff2_table *table = &font->tables[order[i]];
		unsigned char *record = sfnt + SFNT_HEADER_SIZE + SFNT_RECORD_SIZE * i;

		offset = align4(offset);
		if (table->length > 0)
			memcpy(sfnt + offset, table->bytes, table->length);
		if (table->tag == TAG_HEAD && table->length >= HEAD_ADJUSTMENT_OFFSET + 4) {
			head = sfnt + offset;
			put_u32(head + HEAD_ADJUSTMENT_OFFSET, 0);
		}
		put_u32(record, table->tag);
		put_u32(record + 4, sfnt_checksum(sfnt + offset, table->length));
		put_u32(record + 8, offset);
		put_u32(record + 12, table->length);
		offset += table->length;
	}
	if (head != NULL)
		put_u32(head + HEAD_ADJUSTMENT_OFFSET,
				(SFNT_CHECKSUM_MAGIC - sfnt_checksum(sfnt, total)) &
				SFNT_U32_MASK);

	free(order);
	*out = sfnt;
	*out_len = (long)total;
	return 1;
}

int macos9_woff2_to_ttf(const unsigned char *src, long src_len,
		unsigned char **out, long *out_len)
{
	woff2_font font;
	int ok;

	if (out == NULL || out_len == NULL)
		return 0;
	*out = NULL;
	*out_len = 0;
	if (src == NULL || src_len < (long)WOFF2_HEADER_SIZE)
		return 0;

	memset(&font, 0, sizeof(font));
	ok = decode_font(&font, src, (unsigned long)src_len) &&
		write_sfnt(&font, out, out_len);
	free_font(&font);
	return ok;
}
