#! /bin/sh
# convert the X11 rgb.ttx file to C header

ERROR() {
  printf 'error: '
  printf -- "${@}"
  printf '\n'
  exit 1
}

[ -z "${1}" ] && ERROR 'missing argument: /path/to/rgb.txt'
rgb_src="${1}"

[ -f "${rgb_src}" ] || ERROR 'file: %s does not exist' "${rgb_src}"

cat <<EOF
// x11-rgb-txt.h

#if !defined(X11_RGB_TXT_H)
#define X11_RGB_TXT_H 1

#if !defined(X11_RGB)
#define X11_RGB(R, G, B) { \\
          .red = R,        \\
          .green = G,      \\
          .blue = B,       \\
        }
#endif

EOF

awk < "${rgb_src}" '
  BEGIN {
    line = 1
    max = 0
    longest = ""
  }

  END {
    # print "// longest name: " longest " = " max " chars"
  }

  {
    line = 1
  }

  /^[[:space:]]*#/ {
    line = 0
  }

  /^[[:space:]]*$/ {
    line = 0
  }

  {
    if (line) {
      # print " // " $0
      r = $1
      g = $2
      b = $3
      name = $4
      n = 5
      while ($n != "") {
        name = name "_" $n
        n = n + 1
      }
      if (length(name) > max) {
        max = length(name)
        longest = name
      }
      # longest name: light_goldenrod_yellow = 22 chars
      printf("#define X11_RGB_%-24s X11_RGB(%d, %d, %d)\n", name, r, g, b)
    }
  }
'

cat <<EOF

#endif
EOF
