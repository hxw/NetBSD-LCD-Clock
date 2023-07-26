// ili9486.h

#if !defined(ILI9486_H)
#define ILI9486_H 1

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

// rotation of the LCD
typedef enum {
  // origin (0,0) aligns with pin 1 of the Pi GPIO header, USB RHS
  ILI9486_ROTATION_0 = 0,
  // origin (0,0) aligns with Pi Ethernet connector, USB LHS
  ILI9486_ROTATION_180 = 1,
} ILI9486_rotation_type;

// functions
// =========

// create connection to LCD
bool ILI9486_create(ILI9486_rotation_type rotate);

// disconnect LCD and release resources
bool ILI9486_destroy(void);

// sync any changes in the internal buffer to the LCD
// updated largest marked area and zeros the mark
void ILI9486_sync(void);

// sync whole internal buffer to the LCD
void ILI9486_refresh(void);

// clear the internal buffer to a colour
// marks whole buffer as changed so either sync or refresh can be used
void ILI9486_clear(uint8_t red, uint8_t green, uint8_t blue);

// send a rectangular bitmap to the internal buffer
//
// returns truncation occurred
bool ILI9486_rect_rgba(
    int x,             // X coordinate on LCD
    int y,             // Y coordinate on LCD
    int offset_x,      // X offset in bitmap
    int offset_y,      // Y offset in bitmap
    int width,         // bitmap width in pixels
    int height,        // bitmap height in pixels
    size_t stride,     // number of bytes in a bitmap row (for alignment)
    const void *buffer // buffer of bytes in RGBA order (4 bytes/pixel)
                       // total bytes = height * stride * 4
);

#endif
