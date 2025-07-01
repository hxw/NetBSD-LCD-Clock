// main.c

#include <err.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>  // chmod
#include <sys/time.h>  // ntp_gettime
#include <sys/timex.h> // ntp_gettime
#include <sys/un.h>    // Unix-domain sockets
#include <time.h>
#include <unistd.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_COLOR_H
#include FT_BITMAP_H

#include "ili9486.h"
#include "unicode.h"

#define X11_RGB(R, G, B)                                                       \
  {                                                                            \
      .red = R,                                                                \
      .green = G,                                                              \
      .blue = B,                                                               \
      .alpha = 255,                                                            \
  }
#include "x11-rgb-txt.h"

// for setting the message line using:
//   echo 'your message here' | nc -U /tmp/clock.sock
// suggest some leading spaces (14) for message to scroll better
#define UNIX_SOCKET_1 "/tmp/clock.sock"
#define UNIX_SOCKET_2 "/tmp/clock2.sock"
#define SOCKET_MODE (0777)

// font configuration

// #define FONT_FILE "/usr/pkg/share/fonts/X11/TTF/FreeMonoBold.ttf"
// #define FONT_FILE "/usr/pkg/share/fonts/X11/TTF/DejaVuSansMono-Bold.ttf"
// #define FONT_FILE "/usr/pkg/share/fonts/X11/TTF/NotoSansMonoCJKtc-Bold.otf"
// #define FONT_FILE "/usr/pkg/share/fonts/X11/TTF/NotoSans-Bold.ttf"

#define TIME_FONT_FILE "/usr/pkg/share/fonts/X11/TTF/NotoSans-Bold.ttf"
const int time_font_width = 0;
const int time_font_height = 120;

#define DATE_FONT_FILE "/usr/pkg/share/fonts/X11/TTF/NotoSansMonoCJKtc-Bold.otf"
const int date_font_width = 0;
const int date_font_height = 92;

#define MESSAGE_FONT_FILE                                                      \
  "/usr/pkg/share/fonts/X11/TTF/NotoSansMonoCJKtc-Bold.otf"
const int message_font_width = 0;
const int message_font_height = 64;

#define SIZE_OF_ARRAY(a) (sizeof(a) / sizeof((a)[0]))

static bool render(int x, int y, int x_offset, int incr, const char *str,
                   FT_Library library, FT_Face face, FT_Color foreground,
                   FT_Color background);

static void dump_bitmap(const char *title, FT_Bitmap *bitmap) {
#if 1
  (void)title;
  (void)bitmap;
#else
  printf("------------------------------\n");
  printf("Dump of: %s\n", title);
  printf("rows:  %6d\n", bitmap->rows);
  printf("width: %6d\n", bitmap->width);
  printf("pitch: %6d\n", bitmap->pitch);
  printf("grays: %6d\n", bitmap->num_grays);
  printf("pixel: %6d\n", bitmap->pixel_mode);

  uint8_t *p = (uint8_t *)(bitmap->buffer);
  for (int r = 0; r < bitmap->rows; ++r) {
    for (int w = 0; w < bitmap->pitch; ++w) {
      printf("%02x ", p[w]);
    }
    p += bitmap->pitch;
    printf("\n");
  }
#endif
}

static int make_listen_socket(const char *unix_path) {

  // Unix socket for message setup
  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    err(EXIT_FAILURE, "cannot create server socket");
    /* NOTREACHED */
  }

  int value = 1;
  int rc =
      setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
  if (rc < 0) {
    err(EXIT_FAILURE, "cannot set server socket options");
    /* NOTREACHED */
  }

  struct sockaddr_un saddr = {
      .sun_len = sizeof(struct sockaddr_un),
      .sun_family = AF_UNIX,
      .sun_path = "/tmp/s", // dummy value
  };

  if (strlen(unix_path) > sizeof(saddr.sun_path) - 1) {
    err(EXIT_FAILURE, "unix path is too long");
    /* NOTREACHED */
  }
  strlcpy(saddr.sun_path, unix_path, sizeof(saddr.sun_path));

  unlink(saddr.sun_path);

  socklen_t saddr_len = sizeof(saddr);

  rc = bind(server_fd, (struct sockaddr *)&saddr, saddr_len);
  if (rc < 0) {
    err(EXIT_FAILURE, "cannot bind server socket");
    /* NOTREACHED */
  }

  rc = chmod(saddr.sun_path, SOCKET_MODE);
  if (rc < 0) {
    err(EXIT_FAILURE, "cannot set server socket mode");
    /* NOTREACHED */
  }

  listen(server_fd, 10);

  return server_fd;
}

void usage(const char *program) {
  printf("usage: %s [options]\n", program);
  printf("       --help                 -h            this message\n"
         "       --verbose              -v            more messages\n"
         "       --daemon               -b            background as a daemon\n"
         "       --rotate               -r            rotate display 180 "
         "degrees\n");
  exit(1);
}

int main(int argc, char *argv[]) {

  extern char *optarg;
  extern int optind;

  // options descriptor
  static struct option longopts[] = {
      {"help", no_argument, NULL, 'h'},
      {"verbose", no_argument, NULL, 'v'},
      {"daemon", no_argument, NULL, 'b'},
      {"rotate", no_argument, NULL, 'r'},
      //{"pidfile", required_argument, NULL, 'p'},
      {NULL, 0, NULL, 0}};

  const char *program = "lcd_clock";
  int verbose = 0;
  bool background = false;
  ILI9486_rotation_type rotate = ILI9486_ROTATION_0;

  int ch = 0;
  while ((ch = getopt_long(argc, argv, "brvh", longopts, NULL)) != -1)
    switch (ch) {
    case 'b':
      background = true;
      break;
    case 'r':
      rotate = ILI9486_ROTATION_180;
      break;
    case 'v':
      ++verbose;
      break;
    case 'h':
    case '?':
    default:
      usage(program);
    }
  argc -= optind;
  argv += optind;

  if (background) {
    int rc = daemon(0, 0);
    if (rc < 0) {
      err(EXIT_FAILURE, "cannot background the process");
    }
  }

  if (verbose > 0) {
    printf("TZ: %s\n", getenv("TZ"));
  }
  tzset();

  // Unix socket for message setup
  int server_1_fd = make_listen_socket(UNIX_SOCKET_1);
  if (server_1_fd < 0) {
    err(EXIT_FAILURE, "cannot create server socket");
    /* NOTREACHED */
  }

  int server_2_fd = make_listen_socket(UNIX_SOCKET_2);
  if (server_2_fd < 0) {
    err(EXIT_FAILURE, "cannot create server socket");
    /* NOTREACHED */
  }

  // LCD configuration

  if (!ILI9486_create(rotate)) {
    err(EXIT_FAILURE, "ili9486 create failed");
  }

  ILI9486_clear(0, 0, 0);

  FT_Library library;

  int error = FT_Init_FreeType(&library);
  if (error != 0) {
    err(EXIT_FAILURE, "FreeType setup failed: error: %d", error);
  }

  FT_Face time_face;
  error = FT_New_Face(library, TIME_FONT_FILE, 0, &time_face);
  if (error == FT_Err_Unknown_File_Format) {
    err(EXIT_FAILURE, "FreeType unknown font format: %d", error);
  } else if (error != 0) {
    err(EXIT_FAILURE, "FreeType face error: %d", error);
  }
  error = FT_Set_Pixel_Sizes(time_face, time_font_width, time_font_height);
  if (error != 0) {
    err(EXIT_FAILURE, "FreeType pixel sizes error: %d", error);
  }

  FT_Face date_face;
  error = FT_New_Face(library, DATE_FONT_FILE, 0, &date_face);
  if (error == FT_Err_Unknown_File_Format) {
    err(EXIT_FAILURE, "FreeType unknown font format: %d", error);
  } else if (error != 0) {
    err(EXIT_FAILURE, "FreeType face error: %d", error);
  }
  error = FT_Set_Pixel_Sizes(date_face, date_font_width, date_font_height);
  if (error != 0) {
    err(EXIT_FAILURE, "FreeType pixel sizes error: %d", error);
  }

  FT_Face message_face;
  error = FT_New_Face(library, MESSAGE_FONT_FILE, 0, &message_face);
  if (error == FT_Err_Unknown_File_Format) {
    err(EXIT_FAILURE, "FreeType unknown font format: %d", error);
  } else if (error != 0) {
    err(EXIT_FAILURE, "FreeType face error: %d", error);
  }
  error =
      FT_Set_Pixel_Sizes(message_face, message_font_width, message_font_height);
  if (error != 0) {
    err(EXIT_FAILURE, "FreeType pixel sizes error: %d", error);
  }

  typedef struct {
    FT_Color time;
    FT_Color day;
    FT_Color date;
    FT_Color message;
    FT_Color background;
  } colours_type;

  typedef struct {
    colours_type early;
    colours_type morning;
    colours_type afternoon;
    colours_type evening;
    colours_type unsync;
  } theme_type;

  theme_type themes = {
      .early =
          {
              .time = X11_RGB_SteelBlue,
              .day = X11_RGB_DarkBlue,
              .date = X11_RGB_MidnightBlue,
              .message = X11_RGB_SlateBlue,
              .background = X11_RGB_grey5,
          },
      .morning =
          {
              .time = X11_RGB_yellow,
              .day = X11_RGB_gold,
              .date = X11_RGB_orange,
              .message = X11_RGB_gold2,
              .background = X11_RGB_black,
          },
      .afternoon =
          {
              .time = X11_RGB_pink,
              .day = X11_RGB_HotPink,
              .date = X11_RGB_DeepPink,
              .message = X11_RGB_DeepPink2,
              .background = X11_RGB_black,
          },
      .evening =
          {
              .time = X11_RGB_LightCyan,
              .day = X11_RGB_cyan,
              .date = X11_RGB_SkyBlue,
              .message = X11_RGB_LightBlue,
              .background = X11_RGB_grey10,
          },
      .unsync =
          {
              .time = X11_RGB_black,
              .day = X11_RGB_grey10,
              .date = X11_RGB_grey20,
              .message = X11_RGB_grey15,
              .background = X11_RGB_red,
          },
  };
  colours_type *theme = &themes.morning;

  ILI9486_clear(theme->background.red, theme->background.green,
                theme->background.blue);
  ILI9486_refresh();

  size_t m_pos = 0;
  int m_offset = 0;
#if 1
  char message[1024];
  strlcpy(message,
          "               "          // initial spaces
          "Loading ... Loading ... " // description
          //"27-33度  多雲，午後有局部短暫雷陣雨" // sample text
          "Loading ... Loading ... " // description
          ,
          sizeof(message));
#else
  const char message[] =
      "               "                         // initial spaces
      "A sample for testing the message line: " // description
      "27-33度  多雲，午後有局部短暫雷陣雨"     // sample text
      ;
#endif

  char message1[500];
  char message2[500];
  memset(message1, 0, sizeof(message1));
  memset(message2, 0, sizeof(message2));

  bool sync = false;
  for (;;) {
    time_t clk = time(NULL);
    struct tm now;
    localtime_r(&clk, &now);

    if (now.tm_sec == 0) {
      struct ntptimeval ntv;
      sync = ntp_gettime(&ntv) != TIME_ERROR;
    }
    if (!sync) {
      theme = &themes.unsync;
    } else if (now.tm_hour < 6) {
      theme = &themes.early;
    } else if (now.tm_hour < 12) {
      theme = &themes.morning;
    } else if (now.tm_hour < 18) {
      theme = &themes.afternoon;
    } else {
      theme = &themes.evening;
    }

    ILI9486_clear(theme->background.red, theme->background.green,
                  theme->background.blue);

    char buffer[20];

    (void)strftime(buffer, sizeof(buffer), "%H:%M:%S", &now);
    render(0, 100, 0, 0, buffer, library, time_face, theme->time,
           theme->background);

    const char *wday[7] = {
        "Su日", "Mo一", "Tu二", "We三", "Th四", "Fr五", "Sa六",
    };

    render(0, 200, 0, 0, wday[now.tm_wday], library, date_face, theme->day,
           theme->background);

    (void)strftime(buffer, sizeof(buffer), " %m-%d", &now);
    render(200, 200, 0, 0, buffer, library, date_face, theme->date,
           theme->background);

    const int incr = 32; // one ASCII or ½ Chinese
    bool trunc = render(0, 290, m_offset, incr, &message[m_pos], library,
                        message_face, theme->message, theme->background);

    if (trunc) {
      m_offset = 0;
      ++m_pos;
      while ((message[m_pos] & 0xc0) == 0x80) {
        ++m_pos;
      }
      if (message[m_pos] == '\0' || m_pos > sizeof(message) - 1) {
        m_pos = 0;
      }
    } else {
      m_offset += incr;
    }
    // printf("pos: %2lu, %2d\n", m_pos, m_offset);

#if 1
    ILI9486_refresh();
#else
    ILI9486_sync();
    // sleep(1);
#endif

    // usleep(1000);

    fd_set accepting;
    FD_ZERO(&accepting);
    FD_SET(server_1_fd, &accepting);
    FD_SET(server_2_fd, &accepting);

    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 1000,
    };
    int nfds = select(FD_SETSIZE, &accepting, NULL, NULL, &timeout);
    if (nfds < 0) {
      err(EXIT_FAILURE, "cannot select on server socket");
    }

    if (FD_ISSET(server_1_fd, &accepting)) {
      int client_fd = accept(server_1_fd, NULL, NULL);

      write(client_fd, "send a one line message:\r\n", 7);

      ssize_t n = read(client_fd, message1, sizeof(message1) - 1);
      if (n >= 0) {
        message1[n] = '\0';
        // printf("%s", message);
        strlcpy(message, message1, sizeof(message));
        strlcat(message, message2, sizeof(message));
      }

      close(client_fd);

      m_pos = 0; // reset scroll point
    }

    if (FD_ISSET(server_2_fd, &accepting)) {
      int client_fd = accept(server_2_fd, NULL, NULL);

      write(client_fd, "send a one line message:\r\n", 7);

      ssize_t n = read(client_fd, message2, sizeof(message2) - 1);
      if (n >= 0) {
        message2[n] = '\0';
        // printf("%s", message2);
        strlcpy(message, message1, sizeof(message));
        strlcat(message, message2, sizeof(message));
      }

      close(client_fd);

      m_pos = 0; // reset scroll point
    }
  }

  // ILI9486_rect_rgba(70, 50, 0, 0, abitmap.width, abitmap.rows,
  //                  abitmap.pitch,
  //                  abitmap.buffer);
  ILI9486_refresh();
  sleep(2);

  if (!ILI9486_destroy()) {
    err(EXIT_FAILURE, "ili9486 destroy failed");
  }

  return EXIT_SUCCESS;
}

static bool render(int x, int y, int x_offset, int incr, const char *str,
                   FT_Library library, FT_Face face, FT_Color foreground,
                   FT_Color background) {

  bool rc = false;
  uint32_t text[20];
  size_t text_length = SIZE_OF_ARRAY(text);
  (void)string_to_ucs4(str, text, &text_length);

  FT_GlyphSlot slot = face->glyph; // a small shortcut

  for (size_t n = 0; n < text_length; ++n) {

    // load glyph image into the slot (erase previous one)
    int error = FT_Load_Char(face, text[n], FT_LOAD_RENDER);
    if (error != 0) {
      continue; // ignore errors
    }

    dump_bitmap("the glyph", &slot->bitmap);
    FT_Vector zero = {.x = 0, .y = 0};

    FT_Bitmap abitmap;
    FT_Bitmap_Init(&abitmap);

    // make background only bitmap
    FT_Bitmap_Blend(library, &slot->bitmap, zero, &abitmap, &zero, background);
    uint8_t *p = (uint8_t *)(abitmap.buffer);
    for (size_t r = 0; r < abitmap.rows; ++r) {
      for (size_t w = 0; w < abitmap.width; ++w) {
        ((FT_Color *)p)[w] = background;
      }
      p += abitmap.pitch;
    }

    FT_Bitmap_Blend(library, &slot->bitmap, zero, &abitmap, &zero, foreground);

    // render on LCD
    bool trunc = ILI9486_rect_rgba(x + slot->bitmap_left, y - slot->bitmap_top,
                                   x_offset, 0, abitmap.width, abitmap.rows,
                                   abitmap.pitch, abitmap.buffer);

    // advance cursor
    int advance = (slot->advance.x >> 6) - x_offset;
    if (n == 0 && advance <= incr) {
      rc = true;
    }
    x += advance;
    x_offset = 0;

    FT_Bitmap_Done(library, &abitmap);
    if (trunc) {
      break; // end-of line / end of screen
    }
  }
  return rc;
}
