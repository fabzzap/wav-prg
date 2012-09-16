#include <stdint.h>

struct display_interface_internal;

struct prg2wav_display_interface {
  void (*start)(struct display_interface_internal *internal, uint32_t length, const char* name, uint32_t index, uint32_t total);
  void (*update)(struct display_interface_internal *internal, uint32_t length);
  void (*end)(struct display_interface_internal *internal);
};
