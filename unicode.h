// spi.h

#if !defined(UNICODE_H)
#define UNICODE_H 1

#include <stdbool.h>
#include <stdint.h>

// convert a NUL ('\0') terminated UTF-8 string to a series of 32 bit
// code points, update the converted length
// return true if no truncation
bool string_to_ucs4(const char *src, uint32_t *codepoint,
                    size_t *codepoint_length);

#endif
