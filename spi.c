// spi.c

#include <dev/spi/spi_io.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "spi.h"

// spi information
struct SPI_struct {
  int fd;
  SPI_addr_type addr;
  uint32_t bps;
  SPI_mode_type mode;
  uint8_t *buffer;
  size_t buffer_length;
};

// enable SPI access SPI fd
SPI_type *SPI_create(const char *spi_path, SPI_addr_type addr, uint32_t bps,
                     SPI_mode_type mode) {

  // allocate memory
  SPI_type *spi = malloc(sizeof(SPI_type));
  if (NULL == spi) {
    warn("falled to allocate SPI structure");
    return NULL;
  }
  spi->fd = open(spi_path, O_RDWR);
  if (spi->fd < 0) {
    warn("cannot open spi: %s", spi_path);
    free(spi);
    return NULL;
  }

  spi->addr = addr;
  spi->bps = bps;
  spi->mode = mode;
  spi->buffer = malloc(256);
  spi->buffer_length = 256;

  if (NULL == spi->buffer) {
    warn("cannot allocate spi buffer: %s", spi_path);
    free(spi);
    return NULL;
  }

  spi_ioctl_configure_t cfg;
  cfg.sic_addr = addr;
  cfg.sic_mode = mode;
  cfg.sic_speed = bps;

  if (ioctl(spi->fd, SPI_IOCTL_CONFIGURE, &cfg) == -1) {
    warn("SPI_create error: %d", errno);
    free(spi);
    return NULL;
  }

  return spi;
}

// release SPI fd (if open)
bool SPI_destroy(SPI_type *spi) {
  if (NULL == spi) {
    return false;
  }
  close(spi->fd);
  spi->fd = -1;
  free(spi->buffer);
  spi->buffer = NULL;
  spi->buffer_length = 0;
  free(spi);
  return true;
}

// internal function
static int spi_transfer(int fd, SPI_addr_type addr, void *send, size_t slen,
                        void *recv, size_t rlen) {

  spi_ioctl_transfer_t tr;

  tr.sit_addr = addr;
  tr.sit_send = send;
  tr.sit_sendlen = slen;
  tr.sit_recv = recv;
  tr.sit_recvlen = rlen;

  if (ioctl(fd, SPI_IOCTL_TRANSFER, &tr) == -1) {
    return errno;
  }
  return 0;
}

// send a data block to SPI device
void SPI_send(SPI_type *spi, const void *buffer, size_t length) {

  if (length > spi->buffer_length) {
    free(spi->buffer);
    spi->buffer_length = length;
    spi->buffer = malloc(spi->buffer_length);
    if (NULL == spi->buffer) {
      warn("SPI: send malloc %ld bytes failure", spi->buffer_length);
      return;
    }
  }

  int err = spi_transfer(spi->fd, spi->addr,
                         memcpy(spi->buffer, buffer, length), length, NULL, 0);
  if (err != 0) {
    warn("SPI: send error: %d", err);
  }
}

// send a data block to SPI and return last bytes returned by device
void SPI_read(SPI_type *spi, const void *buffer, void *received,
              size_t length) {

  memcpy(received, buffer, length);

  int err =
      spi_transfer(spi->fd, spi->addr, received, length, received, length);
  if (err != 0) {
    warn("SPI: read error: %d", err);
  }
}
