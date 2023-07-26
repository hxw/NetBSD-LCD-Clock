// ili9486.c

#include <err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gpio.h"
#include "ili9486.h"
#include "spi.h"

// preset for WAVESHARE 3.5inch RPI LCD (C)
#define DISPLAY_18BIT 1
#define DISPLAY_INVERTED 0
#define DISPLAY_BGR 1
#define DISPLAY_SWAP_XY 1
#define DISPLAY_SPI_16BIT 1

// Memory Access Control register bits
#define MAC_HORIZONTAL_REFRESH_ORDER (1 << 2)
#define MAC_BGR_ORDER (1 << 3)
#define MAC_VERTICAL_REFRESH_ORDER (1 << 4)
#define MAC_ROW_COLUMN_EXCHANGE (1 << 5)
#define MAC_COLUMN_ADDRESS_ORDER (1 << 6)
#define MAC_ROW_ADDRESS_ORDER (1 << 7)

#define MAC_ROTATE_180 (MAC_COLUMN_ADDRESS_ORDER | MAC_ROW_ADDRESS_ORDER)

// if 16 bit SPI format, add a padding zero byte
#if DISPLAY_SPI_16BIT
#define SPX(b) 0x00, (b)
#else
#define SPX(b) (b)
#endif

// SPI clock frequency
static const int spi_bps = 30000000;

// GPIO settings
static const int tp_intr = 17;
static const int lcd_rs = 24;
static const int rst = 25;

static const int lcd_pixel_width = 480;
static const int lcd_pixel_height = 320;
static const int nl =
    lcd_pixel_width / 8 - 1; // pixels → bytes (number of lines)

// framebuffer byte layout must match the ili9486 mapping order
typedef struct __attribute__((packed, aligned(1))) {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} rgb_type;

static rgb_type *framebuffer = NULL;
static const size_t framebuffer_pixels = lcd_pixel_width * lcd_pixel_height;

static SPI_type *spi = NULL;

// send setup commands over SPI
#define SEND(spi, cmd, ...)                                                    \
  do {                                                                         \
    const uint8_t _cmd[] = {0x00, cmd};                                        \
    const uint8_t _data[] = {__VA_ARGS__};                                     \
    GPIO_write(lcd_rs, 0);                                                     \
    SPI_send((spi), _cmd, sizeof(_cmd));                                       \
    GPIO_write(lcd_rs, 1);                                                     \
    if (sizeof(_data) > 0) {                                                   \
      SPI_send((spi), _data, sizeof(_data));                                   \
    }                                                                          \
  } while (0)

// send data commands over SPI
#define DATA(spi, cmd, data)                                                   \
  do {                                                                         \
    const uint8_t _cmd[] = {0x00, cmd};                                        \
    GPIO_write(lcd_rs, 0);                                                     \
    SPI_send((spi), _cmd, sizeof(_cmd));                                       \
    GPIO_write(lcd_rs, 1);                                                     \
    SPI_send((spi), data, sizeof(data));                                       \
  } while (0)

#define DATA_S(spi, cmd, data, size)                                           \
  do {                                                                         \
    const uint8_t _cmd[] = {0x00, cmd};                                        \
    GPIO_write(lcd_rs, 0);                                                     \
    SPI_send((spi), _cmd, sizeof(_cmd));                                       \
    GPIO_write(lcd_rs, 1);                                                     \
    SPI_send((spi), data, size);                                               \
  } while (0)

// if bus is 16 bit need to prefix with a zero byte
#if DISPLAY_SPI_16BIT
#define CVT(b) 0x00, (b)
#else
#define CVT(b) (b)
#endif

static void delay_ms(int ms) {
  if (ms > 0) {
    usleep(1000 * ms);
  }
}

// create connection to LCD
bool ILI9486_create(ILI9486_rotation_type rotate) {

  // Memory Access Control value
  static uint8_t mac = (0
#if DISPLAY_BGR
                        | MAC_BGR_ORDER
#endif
#if DISPLAY_SWAP_XY
                        | MAC_ROW_COLUMN_EXCHANGE
#endif
  );
  switch (rotate) {
  case ILI9486_ROTATION_0:
    mac |= MAC_ROTATE_180;
    break;

  case ILI9486_ROTATION_180:
  default:
    break;
  }

  // allocate and clear the framebuffer
  if (framebuffer == NULL) {
    framebuffer = (rgb_type *)calloc(framebuffer_pixels, sizeof(rgb_type));
    if (framebuffer == NULL) {
      err(EXIT_FAILURE, "allocate framebuffer failed");
      goto fail;
    }
  }

  // GPIO
  if (!GPIO_setup(GPIO_DEVICE)) {
    err(EXIT_FAILURE, "gpio setup failed");
    goto fail;
  }

  // SPI
  spi = SPI_create(SPI_DEVICE, SPI_ADDR_0, spi_bps, SPI_MODE_0);
  if (NULL == spi) {
    err(EXIT_FAILURE, "spi create failed");
    goto fail;
  }

  // GPIO configuration
  GPIO_mode(tp_intr, GPIO_INPUT);
  GPIO_mode(lcd_rs, GPIO_OUTPUT);
  GPIO_mode(rst, GPIO_OUTPUT);

  // return a value (0/1) for a given input pin
  // int GPIO_read(tp_intr);

  // reset the ili9486 chip
  GPIO_write(rst, 1); // reset = inactive
  delay_ms(1);        // minimum delay
  GPIO_write(rst, 0); // reset = active
  delay_ms(10);       // reset pulse width = 10ms
  GPIO_write(rst, 1); // reset = inactive
  delay_ms(120);      // need 120 ms delay for chip to reset

  GPIO_write(lcd_rs, 0);

  SEND(spi, 0xb0, 0x00, 0x00); // SPI should be set by hardware reset

  SEND(spi, 0x11); // Sleep OUT
  delay_ms(120);

#if DISPLAY_18BIT
  SEND(spi, 0x3a, SPX(0x66)); // 18 bit pixel
#else
  SEND(spi, 0x3a, SPX(0x55)); // 16 bit pixel
#endif

  SEND(spi, 0xb4, SPX(0x00)); // Display Inversion Control

#if DISPLAY_INVERTED
  SEND(spi, 0x21); // Display Inversion ON
#else
  SEND(spi, 0x20);            // Display Inversion OFF
#endif

  SEND(spi, 0xc0, SPX(0x09), SPX(0x09)); // Power Control 1
  SEND(spi, 0xc1, SPX(0x41), SPX(0x00)); // Power Control 2
  SEND(spi, 0xc2, SPX(0x33));            // Power Control 3
  SEND(spi, 0xc5, SPX(0x00), SPX(0x36)); // VCOM Control 1
  SEND(spi, 0x36, SPX(mac));             // Memory Access Control

  // Positive Gamma Control
  SEND(spi, 0xe0, SPX(0x00), SPX(0x2c), SPX(0x2c), SPX(0x0b), SPX(0x0c),
       SPX(0x04), SPX(0x4C), SPX(0x64), SPX(0x36), SPX(0x03), SPX(0x0e),
       SPX(0x01), SPX(0x10), SPX(0x01), SPX(0x00));

  // Negative Gamma Control
  SEND(spi, 0xe1, SPX(0x0f), SPX(0x37), SPX(0x37), SPX(0x0c), SPX(0x0f),
       SPX(0x05), SPX(0x50), SPX(0x32), SPX(0x36), SPX(0x04), SPX(0x0b),
       SPX(0x00), SPX(0x19), SPX(0x14), SPX(0x0f));

  // Display Function Control
  SEND(spi, 0xb6, SPX(0x00), SPX(0x02), SPX((uint8_t)(nl)));

  SEND(spi, 0x11); // Sleep OUT
  delay_ms(120);
  SEND(spi, 0x29); // Display ON
  SEND(spi, 0x38); // Idle Mode OFF
  SEND(spi, 0x13); // Normal Display Mode ON

  return true;

fail:
  ILI9486_destroy();
  return false;
}

// disconnect LCD and release resources
bool ILI9486_destroy(void) {

  bool ok = true;

  GPIO_write(rst, 0); // reset = active

  if (framebuffer != NULL) {
    free(framebuffer);
    framebuffer = NULL;
  }

  if (!SPI_destroy(spi)) {
    warn("spi destroy failed");
    ok = false;
  }
  spi = NULL;

  if (!GPIO_teardown()) {
    warn("gpio teardown failed");
    ok = false;
  }
  return ok;
}

// sync any changes in the internal buffer to the LCD
// updated largest marked area and zeros the mark
void ILI9486_sync(void) {
  // TO DO: just do a full refresh for now
  ILI9486_refresh();
}

// sync whole internal buffer to the LCD
void ILI9486_refresh(void) {
  if (framebuffer == NULL) {
    return;
  }
  for (int y = 0; y < lcd_pixel_height; ++y) {
    SEND(spi, 0x2a, 0, 0, 0, 0, 0, (lcd_pixel_width - 1) >> 8, 0,
         (lcd_pixel_width - 1) & 0xff);
    SEND(spi, 0x2b, 0, (uint8_t)(y >> 8), 0, (uint8_t)(y & 0xff), 0,
         (lcd_pixel_height - 1) >> 8, 0, (lcd_pixel_height - 1) & 0xff);
    rgb_type *p = &framebuffer[y * lcd_pixel_width];
    DATA_S(spi, 0x2c, (uint8_t *)p, lcd_pixel_width * sizeof(rgb_type));
  }
}

// clear the internal buffer to a colour
// marks whole buffer as changed so either sync or refresh can be used
void ILI9486_clear(uint8_t red, uint8_t green, uint8_t blue) {
  if (framebuffer == NULL) {
    return;
  }
  for (size_t n = 0; n < framebuffer_pixels; ++n) {
    framebuffer[n].red = red;
    framebuffer[n].green = green;
    framebuffer[n].blue = blue;
  }
}

// send a rectangular bitmap to the internal buffer
// and mark changed area
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
) {

  bool truncated = false;

  if (x >= lcd_pixel_width || y >= lcd_pixel_height) {
    return true; // off the screen
  }

  if (y + height > lcd_pixel_height) {
    height = lcd_pixel_height - y;
    truncated = true;
  }

  if (x + width > lcd_pixel_width) {
    width = lcd_pixel_width - x;
    truncated = true;
  }

  // 4 byte pixels R/G/B/A
  for (int h = offset_y; h < height; ++h) {
    rgb_type *p = &framebuffer[y * lcd_pixel_width + x];
    uint8_t *s = (uint8_t *)(buffer) + h * stride + (4 * offset_x);
    for (int w = offset_x; w < width; ++w) {
      p->blue = *s++;
      p->green = *s++;
      p->red = *s++;
      ++s; // ignore alpha
      ++p;
    }
    ++y;
  }

  return truncated;
}
