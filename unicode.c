// unicode.c

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// see: https://dev.to/rdentato/utf-8-strings-in-c-2-3-3kp1

// the code here has been tidied up

// the high 4 bits of each byte indicates the total bytes in a sequence
// 0xxx xxxx    plain ASCII               length=1
// 10xx xxxx    UTF-8 continuation bytes  cannot be in a start position
// 11xx xxxx    UTF-8 start byte          length=2…4 i.e., 1…3 continuation
// bytes
//
// since valid lenght are > 0 can use zero as error indication
static uint8_t const lengths[] = {
    1, 1, 1, 1, // 00xx xxxx
    1, 1, 1, 1, // 01xx xxxx
    0, 0, 0, 0, // 10xx xxxx
    2, 2, 3, 4, // 11xx xxxx
};

// macro to extract the length of a sequence based on the 2 MS bits
#define utf8_length(s) (lengths[(((uint8_t *)(s))[0] & 0xf0U) >> 4])

// reserved Unicode value to represent invalid characters 0xfffd
#define INVALID_CHAR_PACKED (0xefbfbd)
#define INVALID_CHAR_CODE (0xfffd)

// check that a packed sequence is valid
//
// reject sequences that are not minimal length to ensure that no
// extra long sequence will case an unexpected buffer overflow
static bool validate_packed(uint32_t c) {
  bool r = false;
  if (c <= 0x7f) {                                 // ASCII
    r = true;                                      // ..
  } else if (c >= 0xc280 && c <= 0xdfbf) {         // ..
    r = (c & 0xe0c0) == 0xc080;                    // ..
  } else if (c >= 0xeda080 && c <= 0xedbfbf) {     // all UTF-16 surrogates
    r = false;                                     // are invalid
  } else if (c >= 0xe0a080 && c <= 0xefbfbf) {     // ..
    r = (c & 0xf0c0c0) == 0xe08080;                // ..
  } else if (c >= 0xf0908080 && c <= 0xf48fbfbf) { // ..
    r = (c & 0xf8c0c0c0) == 0xf0808080;            // ..
  }
  return r;
}

// get the next char from a UTF-8 string
//
// s       source UTF-8 string
// packed  optional pointer to store packed result
//
// returns
//   number of bytes in the source string to be consumed
//   NOTE: will return a lenght of 1 with packed==0 on '\0'
static int next_char(const char *src, uint32_t *packed) {
  uint32_t encoding = 0;

  int len = utf8_length(src);

  // pack the UTF-8 sequence
  for (int i = 0; i < len && src[i] != '\0'; i++) {
    encoding = (encoding << 8) | (uint8_t)(src[i]);
  }

  // replace anything invalid by the invalid character symbol
  // and consume just one byte of the source string
  if (len == 0 || !validate_packed(encoding)) {
    encoding = INVALID_CHAR_PACKED;
    len = 1;
  }

  // optional store the packed value
  if (packed != NULL) {
    *packed = encoding;
  }
  return len;
}

// From UTF-8 packed encoding to Unicode Codepoint
static uint32_t packed_to_ucs4(uint32_t c) {
  uint32_t mask;

  if (c > 0x7f) {
    mask = (c <= 0x00efbfbf) ? 0x000f0000 : 0x003f0000;
    c = ((c & 0x07000000) >> 6) | ((c & mask) >> 4) | ((c & 0x00003f00) >> 2) |
        (c & 0x0000003f);
  }

  return c;
}

#if TESTING
// From Unicode Codepoint to UTF-8 packed encoding
static uint32_t ucs4_to_packed(uint32_t codepoint) {

  uint32_t c = codepoint;

  if (codepoint > 0x7f) {
    c = (codepoint & 0x000003f) | (codepoint & 0x0000fc0) << 2 |
        (codepoint & 0x003f000) << 4 | (codepoint & 0x01c0000) << 6;

    if (codepoint < 0x0000800) {
      c |= 0x0000c080;
    } else if (codepoint < 0x0010000) {
      c |= 0x00e08080;
    } else {
      c |= 0xf0808080;
    }
  }
  return c;
}
#endif

// convert a NUL ('\0') terminated UTF-8 string to a series of 32 bit
// code points
//
// src               a '\0' termionated UTF-8 byte sequence
// codepoint         array of 32 bit Unicode code points
// codepoint_length  pointer to the number of array elements
//                   updated with actual conversion count
// returns
//   true if the string was completely converted
//   false if string truncated to codepoint_length
bool string_to_ucs4(const char *src, uint32_t *codepoint,
                    size_t *codepoint_length) {

  if (src == NULL || codepoint == NULL || codepoint_length == NULL) {
    return false;
  };

  size_t count = 0;
  size_t max = *codepoint_length;

  for (size_t i = 0; i < max && *src != '\0'; ++i) {
    uint32_t packed = 0;
    int len = next_char(src, &packed);
    if (packed == 0) {
      break;
    }

    codepoint[i] = packed_to_ucs4(packed);
    ++count;

    src += len;
  }
  *codepoint_length = count;
  return *src == '\0';
}

#if TESTING

#include <assert.h>
#include <stdio.h>

#define SIZE_OF_ARRAY(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
  uint32_t p;
  int l;
  uint32_t c;
} pkl_type;

static void test1(const char *s, const pkl_type *p, size_t p_len) {

  for (size_t i = 0; i < p_len && *s != '\0'; ++i) {
    uint32_t packed = 0;
    int len = next_char(s, &packed);

    printf("%3zu: actual:   len: %d  packed: 0x%08x\n", i, len, packed);
    printf("%3zu: expected: len: %d  packed: 0x%08x\n", i, p[i].l, p[i].p);

    assert(len == p[i].l);
    assert(packed == p[i].p);

    uint32_t codepoint = packed_to_ucs4(packed);
    printf("%3zu:      codepoint: 0x%08x  %d\n", i, codepoint, codepoint);

    assert(codepoint == p[i].c);
    assert(ucs4_to_packed(codepoint) == packed);

    s += len;
  }
}

int main(int argc, char *argv[]) {

  assert(packed_to_ucs4(INVALID_CHAR_PACKED) == INVALID_CHAR_CODE);
  assert(ucs4_to_packed(INVALID_CHAR_CODE) == INVALID_CHAR_PACKED);

  assert(packed_to_ucs4('A') == 'A');
  assert(ucs4_to_packed('A') == 'A');

  const char t1[] = "Aaß書.";
  const pkl_type p1[] = {
      {.p = 0x41, .l = 1, .c = 'A'},        // ASCII
      {.p = 0x61, .l = 1, .c = 'a'},        // ASCII
      {.p = 0xc39f, .l = 2, .c = 0xdf},     // accented
      {.p = 0xe69bb8, .l = 3, .c = 0x66f8}, // CJK
      {.p = 0x2e, .l = 1, .c = '.'},        // '.'
      {.p = 0x00, .l = 1, .c = 0},          // '\0' // not reached
  };

  test1(t1, p1, SIZE_OF_ARRAY(p1));

  const char t2[] = "\x94\x41\xe9\x42\xc8";
  const pkl_type p2[] = {
      {.p = INVALID_CHAR_PACKED, .l = 1, .c = INVALID_CHAR_CODE}, // FAIL
      {.p = 0x41, .l = 1, .c = 'A'},                              // ASCII
      {.p = INVALID_CHAR_PACKED, .l = 1, .c = INVALID_CHAR_CODE}, // FAIL
      {.p = 0x42, .l = 1, .c = 'B'},                              // ASCII
      {.p = INVALID_CHAR_PACKED, .l = 1, .c = INVALID_CHAR_CODE}, // FAIL
      {.p = 0x00, .l = 1, .c = 0}, // '\0' // not reached
  };

  test1(t2, p2, SIZE_OF_ARRAY(p2));

  uint32_t cp[10];
  const uint32_t cp_expected[] = {0x41, 0x61, 0xdf, 0x66f8, 0x2e};

  size_t cpl = SIZE_OF_ARRAY(cp);
  bool f = string_to_ucs4(t1, cp, &cpl);

  printf("%s: len: %zu\n", f ? "true" : "false", cpl);

  assert(f);
  assert(cpl == SIZE_OF_ARRAY(cp_expected));

  for (size_t i = 0; i < SIZE_OF_ARRAY(cp_expected); ++i) {
    printf("%3zu: actual: 0x%08x  expected: 0x%08x\n", i, cp[i],
           cp_expected[i]);
    assert(cp[i] == cp_expected[i]);
  }

  cpl = SIZE_OF_ARRAY(cp_expected) - 1;
  cp[cpl] = 0xffffffff; // must not be written
  f = string_to_ucs4(t1, cp, &cpl);

  printf("%s: len: %zu\n", f ? "true" : "false", cpl);

  assert(!f);
  assert(cpl == SIZE_OF_ARRAY(cp_expected) - 1);

  for (size_t i = 0; i < SIZE_OF_ARRAY(cp_expected) - 1; ++i) {
    printf("%3zu: actual: 0x%08x  expected: 0x%08x\n", i, cp[i],
           cp_expected[i]);
    assert(cp[i] == cp_expected[i]);
  }

  assert(cp[SIZE_OF_ARRAY(cp_expected) - 1] == 0xffffffff);

  return 0;
}
#endif
