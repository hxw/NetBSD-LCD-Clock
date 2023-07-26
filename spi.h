// spi.h

#if !defined(SPI_H)
#define SPI_H 1

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

// type to hold SPI data
typedef struct SPI_struct SPI_type;

typedef enum {
  SPI_MODE_0 = 0,
  SPI_MODE_1 = 1,
  SPI_MODE_2 = 2,
  SPI_MODE_3 = 3,
} SPI_mode_type;

typedef enum {
  SPI_ADDR_0 = 0,
  SPI_ADDR_1 = 1,
} SPI_addr_type;

// SPI device for RaspberryPi
#define SPI_DEVICE "/dev/spi0"

// functions
// =========

// enable SPI access SPI fd
SPI_type *SPI_create(const char *spi_path, SPI_addr_type addr, uint32_t bps,
                     SPI_mode_type mode);

// release SPI fd
bool SPI_destroy(SPI_type *spi);

// send a data block to SPI
void SPI_send(SPI_type *spi, const void *buffer, size_t length);

// send a data block to SPI and return last bytes returned by slave
void SPI_read(SPI_type *spi, const void *buffer, void *received, size_t length);

#endif
