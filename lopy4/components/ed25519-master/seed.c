#include "ed25519.h"
#include <Arduino.h>

#ifndef ED25519_NO_SEED

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <stdio.h>
#endif

int ed25519_create_seed(unsigned char *dest, size_t size) {
  while (size) {
    unsigned char val = 0;
    for (unsigned i = 0; i < 8; ++i) {
      int init = analogRead(36);
      int count = 0;
      while (analogRead(36) == init) {
        ++count;
      }
      
      if (count == 0) {
         val = (val << 1) | (init & 0x01);
      } else {
         val = (val << 1) | (count & 0x01);
      }
    }
    *dest = val;
    ++dest;
    --size;
  }
  return 0;
}

#endif
