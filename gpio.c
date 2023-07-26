// gpio.c

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/endian.h>
#include <sys/gpio.h>
#include <sys/ioctl.h>

#include "gpio.h"

#define GPIO_INVALID_HANDLE (-1)

// global handle to access gpio
int handle = GPIO_INVALID_HANDLE;

// set up access to the GPIO
bool GPIO_setup(const char *gpio_path) {
  if (handle == GPIO_INVALID_HANDLE) {
    handle = open(gpio_path, O_RDWR);
    if (handle == GPIO_INVALID_HANDLE) {
      warn("cannot open gpio: %s", gpio_path);
      return false;
    }
  }

  return true;
}

// revoke access to GPIO
bool GPIO_teardown() {
  if (handle != GPIO_INVALID_HANDLE) {
    close(handle);
  }

  handle = GPIO_INVALID_HANDLE;

  return true;
}

// For NetBSD the input/output state is defined in /etc/gpio.conf
// and is configured during boot, so no-op here
void GPIO_mode(GPIO_pin_type pin, GPIO_mode_type mode) {
  (void)pin;
  switch (mode) {
  default:
  case GPIO_INPUT:
    // gpio_pin_input(handle, pin);
    break;
  case GPIO_OUTPUT:
    // gpio_pin_output(handle, pin);
    break;
  }
}

int GPIO_read(GPIO_pin_type pin) {
  if ((unsigned)(pin) > 63) {
    return 0;
  }
  struct gpio_req req;

  req.gp_name[0] = '\0';
  req.gp_pin = pin;
  req.gp_value = 0;

  if (ioctl(handle, GPIOREAD, &req) == -1) {
    warn("GPIO_read error: %d\n", errno);
  };
  if (GPIO_PIN_LOW == req.gp_value) {
    return 0;
  } else {
    return 1;
  }
}

void GPIO_write(GPIO_pin_type pin, int value) {
  if ((unsigned)(pin) > 63) {
    return;
  }
  struct gpio_req req;

  req.gp_name[0] = '\0';
  req.gp_pin = pin;
  req.gp_value = value;

  if (ioctl(handle, GPIOWRITE, &req) == -1) {
    warn("GPIO_write error: %d\n", errno);
  };
}
