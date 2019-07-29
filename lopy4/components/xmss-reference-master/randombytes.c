/*
This code was taken from the SPHINCS reference implementation and is public domain.
*/
#include <stdint.h>
#include <Arduino.h>

void randombytes(unsigned char *x, unsigned long long xlen)
{
    analogReadResolution(12);
    int analogPin = A0;
    while (xlen > 0) {
        unsigned char val = 0;
        for (unsigned i = 0; i < 8; ++i) {
            int init = analogRead(analogPin);
            int count = 0;
            while (analogRead(analogPin) == init) {
                ++count;
            }
      
        if (count == 0) {
            val = (val << 1) | (init & 0x01);
        } else {
            val = (val << 1) | (count & 0x01);
        }
        }
        *x = val;
        ++x;
        --xlen;
    }
}
