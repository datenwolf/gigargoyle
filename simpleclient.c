#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include "gg_simple_client.h"

#define COLS 24
#define ROWS 4
#define FRAME_DURATION (uint32_t)(1.0f/30*1e6)

int main(int argc, char *argv[]) {
  gg_frame *f;
  gg_socket *s;

  int i, col, row;

  f = gg_init_frame(COLS, ROWS, 3);
  s = gg_init_socket("localhost", 0xabac);

  /* 30 fps */
  printf("duration %d\n", FRAME_DURATION);
  gg_set_duration(s, FRAME_DURATION);

  for (i = 0; i < 500; ++i) {

    for (col = 0; col < COLS; ++col) {
      for (row = 0; row < ROWS; ++row) {
        gg_set_pixel_color(f, col, row,
                           (uint8_t)(127*(sin(i/5.0)+1)),
                           (uint8_t)(127*(sin(i/10.0)+1)),
                           (uint8_t)(127*(sin(i/20.0)+1)));
      }
    }

    printf("Sent frame %d\n", i);
    gg_send_frame(s, f);
  }

  gg_deinit_frame(f);
  gg_deinit_socket(s);

  return 0;
}
