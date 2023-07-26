# NetBSD-LCD-Clock

User mode access to a Waveshare 320x480 3.5inch LCD (C) Main control
is via SPI with some GPIO for reset.  The LCD uses SPI address 0.

This code only uses the LCD portion of the device, the Resistive Touch
panel is not accessed. I think this would be accessed by the same SPI
channel using address 1.


## Connections

Pi Pin       Name       Description
===========  =========  =========================
1, 17        3.3V       3.3V Power
2, 4         5V         5V Power
3, 5, 7, 8   NC         No connection
8, 10, 12    NC         No connection
13, 15, 16   NC         No connection
6, 9, 14     GND        Ground
20, 25       GND        Ground
11           TP\_IRQ    Touch Panel interrupt (active low)
18           LCD\_RS    Instruction/Data Register selection
19           MOSI       SPI data input of LCD/Touch Panel
21           MISO       SPI data output of Touch Panel
22           RST        Reset
23           SCLK       SPI clock to LCD and Touch Panel
24           LCD\_CS    LCD chip select (active low)
26           TP\_CS     Touch Panel chip select (active low)

## Enable GPIO and SPI

**Add to `/etc/gpio.conf`**

~~~ascii
## Waveshare LCD
## =============

# P1-11 = GPIO 17
gpio0 17 set in tp_irq

# P1-18 = GPIO 24
gpio0 24 set out lcd_rs

# P1-22 = GPIO 25
gpio0 25 set out rst

# spi0_ceX are configured by spi.dtbo, do not set as outputs

# P1-24 = GPIO  8 (spi0_ce0)
#gpio0  8 set out lcd_cs

# P1-26 = GPIO  7 (spi0_ce1)
#gpio0  7 set out tp_cs
~~~

**Patch `/boot/config.txt`**

~~~diff
--- /boot/config.txt.orig	2023-06-14 08:27:06.239183035 +0000
+++ /boot/config.txt	2023-06-01 12:38:06.000000000 +0000
@@ -6,2 +6,3 @@
 enable_uart=1
+dtoverlay=spi.dtbo
 force_turbo=0
~~~

**Install the SPI overlay**

~~~shell
mkdir -p /boot/overlays
cp -p spi.dtbo /boot/overlays/
~~~

GPIO will be configured and SPI will be available after a reboot.

## Compiling

Use the proivided `Makefile`.  There is a `test` target to chjeck that
the Unicode routine works and an `all` target to build the clock
program.  Currently it requires root access to be able to access SPI
and GPIO.

## Crontab for clock to fetch Weather

In the example below the `getweather` program must only return a
single line of text.  The extra leading spaces allow the text to
scroll in from the right rather than immediately starting at the left.

~~~
# crontab

#MAILTO=root
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/pkg/sbin:/usr/pkg/bin

# Minute  Hour  Day-M   Month  Day-Wk    Command
11        */3   *       *      *         printf '              %s' "$(/path/to/getweather)" | nc -U /tmp/clock.sock
~~~

## Running

The clock program can be run by installing the `rc.d/lcd_clock` to
`/etc/rc.d` and adding the following to `/etc/rc.conf`:

```
lcd_clock=YES
#lcd_clock_flags='--rotate'
```

remove the `#` fron the flags setting if the display needs to be
rotated 180 degrees. For the "C" version of the display, the top of
the display is aligned with the PI GPIO pins.  For the "B" version, it
appears to require the rotate enabled to align the display to the GPIO
at the top.
