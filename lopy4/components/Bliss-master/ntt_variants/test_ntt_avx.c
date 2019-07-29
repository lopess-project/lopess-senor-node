#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "ntt_red.h"
#include "ntt_asm.h"
#include "test_ntt_red_tables.h"
#include "sort.h"

#define Q 12289

/*
 * For speed measurements: counter of CPU cycles
 */
static inline uint64_t cpucycles(void) {
  uint64_t result;
  __asm__ volatile(".byte 15;.byte 49;shlq $32,%%rdx;orq %%rdx,%%rax"
    : "=a" (result) ::  "%rdx");
  return result;
}

#define NTESTS 102400

static uint64_t t[NTESTS];

// Average run time
static uint64_t average_time(void) {
  uint64_t s;
  uint32_t i;

  s = 0;
  for (i=0; i<NTESTS; i++) {
    s += t[i];
  }
  return s/NTESTS;
}

// Median
static uint64_t median_time(void) {
  uint32_t i;

  sort(t, NTESTS);
  for (i=1; i<NTESTS; i++) {
    if (t[i] < t[i-1]) {
      fprintf(stderr, "BUG in sort\n");
      exit(1);
    }
  }

  return t[NTESTS/2];
}


/*
 * Print array of size n
 */
static void print_array(FILE *f, const int32_t *a, int32_t n) {
  uint32_t i, k;

  k = 0;
  for (i=0; i<n; i++) {
    if (k == 0) fprintf(f, "  ");
    fprintf(f, "%6"PRId32, a[i]);
    k ++;
    if (k == 16) {
      fprintf(f, "\n");
      k = 0;
    } else {
      fprintf(f, " ");
    }
  }
  if (k > 0) {
    fprintf(f, "\n");
  }
}

#if 0
// same thing for an array of 16bit integers
static void print_array16(FILE *f, const int16_t *a, int32_t n) {
  uint32_t i, k;

  k = 0;
  for (i=0; i<n; i++) {
    if (k == 0) fprintf(f, "  ");
    fprintf(f, "%5"PRId16, a[i]);
    k ++;
    if (k == 16) {
      fprintf(f, "\n");
      k = 0;
    } else {
      fprintf(f, " ");
    }
  }
  if (k > 0) {
    fprintf(f, "\n");
  }
}
#endif

/*
 * Check equality between a and b: arrays of n elements
 */
static bool equal_arrays(const int32_t *a, const int32_t *b, uint32_t n) {
  uint32_t i;

  for (i=0; i<n; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}


/*
 * Random integer in the range [-n, n-1]
 */
static int32_t random_coeff(int32_t n) {
  int32_t x;

  assert(n > 0);

  x = random() % (2 * n);
  x -= n;
  assert(-n <= x && x <= n-1);
  return x;
}

/*
 * Store random integers in a
 * - n = number of elements
 */
static void random_array(int32_t *a, uint32_t n) {
  uint32_t i;

  for (i=0; i<n; i++) {
    a[i] = random_coeff(200000);
  }
}

// 16bit elements
static void random_array16(int16_t *p, uint32_t n) {
  uint32_t i;

  for (i=0; i<n; i++) {
    p[i] = (int16_t) random_coeff(32768);
  }
}

/*
 * Copy a into b
 */
static void copy_array(int32_t *b, const int32_t *a, uint32_t n) {
  uint32_t i;

  for (i=0; i<n; i++) {
    b[i] = a[i];
  }  
}


/*
 * Cross check: apply functions f and g to the same input
 * - f and g are expected to compute the same thing
 * - f is the assembler function (from ntt_asm.h) and g is the trusted oracle (from ntt_red.h)
 */
static void cross_check(const char *name, uint32_t n, void (*f)(int32_t *), void (*g)(int32_t *)) {
  int32_t a[n], b[n], c[n];
  uint32_t j;

  printf("Testing %s: n = %"PRIu32"\n", name, n);
  for (j=0; j<100000; j++) {
    random_array(a, n);
    copy_array(b, a, n);
    copy_array(c, a, n); // keep a copy in case of error
    f(a);
    g(b);
    if (!equal_arrays(a, b, n)) {
      printf("failed on test %"PRIu32"\n", j);
      printf("--> input:\n");
      print_array(stdout, c, n);
      printf("--> output:\n");
      print_array(stdout, a, n);
      printf("correct result:\n");
      print_array(stdout, b, n);
      exit(1);
    }
  }
  printf("all tests passed\n");
}


/*
 * Speed test: function f(a, n)
 */

// global buffers used for speed tests. alignment matters (for speed)
static int32_t a[2048] __attribute__ ((aligned(32)));
static int16_t p[2048] __attribute__ ((aligned(32)));

#if 0
static void speed_test(const char *name, uint32_t n, void (*f)(int32_t *, uint32_t)) {
  uint32_t i;
  uint64_t avg, med, c;

  random_array(a, n);

  for (i=0; i<NTESTS; i++) {
    t[i] = cpucycles();
    f(a, n);
  }
  c = cpucycles();
  for (i=0; i<NTESTS-1; i++) {
    t[i] = t[i+1] - t[i]; 
  }
  t[i] = c - t[i];

  avg = average_time();
  med = median_time();
  printf("speed test %s (n=%"PRIu32"): median = %"PRIu64", average = %"PRIu64"\n", name, n, med, avg);
}
#endif

// variant for f(a, n, p) where p = fixed array of 16bit constants
static void speed_test2(const char *name, uint32_t n, void (*f)(int32_t *, uint32_t, const int16_t *)) {
  uint32_t i;
  uint64_t avg, med, c;

  random_array(a, n);
  random_array16(p, n);

  for (i=0; i<NTESTS; i++) {
    t[i] = cpucycles();
    f(a, n, p);
  }
  c = cpucycles();
  for (i=0; i<NTESTS-1; i++) {
    t[i] = t[i+1] - t[i]; 
  }
  t[i] = c - t[i];

  avg = average_time();
  med = median_time();
  printf("speed test %s (n=%"PRIu32"): median = %"PRIu64", average = %"PRIu64"\n", name, n, med, avg);
}

/*
 * ntt_ct_rev2std:
 */
static void ntt_ct_rev2std16_asm(int32_t *a) {
  ntt_red_ct_rev2std_asm(a, 16, shoup_sred_ntt16_12289);
}

static void ntt_ct_rev2std16_base(int32_t *a) {
  ntt_red_ct_rev2std(a, 16, shoup_sred_ntt16_12289);
}

static void ntt_ct_rev2std128_asm(int32_t *a) {
  ntt_red_ct_rev2std_asm(a, 128, shoup_sred_ntt128_12289);
}

static void ntt_ct_rev2std128_base(int32_t *a) {
  ntt_red_ct_rev2std(a, 128, shoup_sred_ntt128_12289);
}

static void ntt_ct_rev2std256_asm(int32_t *a) {
  ntt_red_ct_rev2std_asm(a, 256, shoup_sred_ntt256_12289);
}

static void ntt_ct_rev2std256_base(int32_t *a) {
  ntt_red_ct_rev2std(a, 256, shoup_sred_ntt256_12289);
}

static void ntt_ct_rev2std512_asm(int32_t *a) {
  ntt_red_ct_rev2std_asm(a, 512, shoup_sred_ntt512_12289);
}

static void ntt_ct_rev2std512_base(int32_t *a) {
  ntt_red_ct_rev2std(a, 512, shoup_sred_ntt512_12289);
}

static void ntt_ct_rev2std1024_asm(int32_t *a) {
  ntt_red_ct_rev2std_asm(a, 1024, shoup_sred_ntt1024_12289);
}

static void ntt_ct_rev2std1024_base(int32_t *a) {
  ntt_red_ct_rev2std(a, 1024, shoup_sred_ntt1024_12289);
}

static void ntt_ct_rev2std2048_asm(int32_t *a) {
  ntt_red_ct_rev2std_asm(a, 2048, shoup_sred_ntt2048_12289);
}

static void ntt_ct_rev2std2048_base(int32_t *a) {
  ntt_red_ct_rev2std(a, 2048, shoup_sred_ntt2048_12289);
}

/*
 * mulntt_ct_rev2std:
 */
static void mulntt_ct_rev2std16_asm(int32_t *a) {
  mulntt_red_ct_rev2std_asm(a, 16, shoup_sred_ntt16_12289);
}

static void mulntt_ct_rev2std16_base(int32_t *a) {
  mulntt_red_ct_rev2std(a, 16, shoup_sred_ntt16_12289);
}

static void mulntt_ct_rev2std128_asm(int32_t *a) {
  mulntt_red_ct_rev2std_asm(a, 128, shoup_sred_ntt128_12289);
}

static void mulntt_ct_rev2std128_base(int32_t *a) {
  mulntt_red_ct_rev2std(a, 128, shoup_sred_ntt128_12289);
}

static void mulntt_ct_rev2std256_asm(int32_t *a) {
  mulntt_red_ct_rev2std_asm(a, 256, shoup_sred_ntt256_12289);
}

static void mulntt_ct_rev2std256_base(int32_t *a) {
  mulntt_red_ct_rev2std(a, 256, shoup_sred_ntt256_12289);
}

static void mulntt_ct_rev2std512_asm(int32_t *a) {
  mulntt_red_ct_rev2std_asm(a, 512, shoup_sred_ntt512_12289);
}

static void mulntt_ct_rev2std512_base(int32_t *a) {
  mulntt_red_ct_rev2std(a, 512, shoup_sred_ntt512_12289);
}

static void mulntt_ct_rev2std1024_asm(int32_t *a) {
  mulntt_red_ct_rev2std_asm(a, 1024, shoup_sred_ntt1024_12289);
}

static void mulntt_ct_rev2std1024_base(int32_t *a) {
  mulntt_red_ct_rev2std(a, 1024, shoup_sred_ntt1024_12289);
}

static void mulntt_ct_rev2std2048_asm(int32_t *a) {
  mulntt_red_ct_rev2std_asm(a, 2048, shoup_sred_ntt2048_12289);
}

static void mulntt_ct_rev2std2048_base(int32_t *a) {
  mulntt_red_ct_rev2std(a, 2048, shoup_sred_ntt2048_12289);
}


/*
 * ntt_ct_std2rev
 */
static void ntt_ct_std2rev16_asm(int32_t *a) {
  ntt_red_ct_std2rev_asm(a, 16, rev_shoup_sred_ntt16_12289);
}

static void ntt_ct_std2rev16_base(int32_t *a) {
  ntt_red_ct_std2rev(a, 16, rev_shoup_sred_ntt16_12289);
}

static void ntt_ct_std2rev128_asm(int32_t *a) {
  ntt_red_ct_std2rev_asm(a, 128, rev_shoup_sred_ntt128_12289);
}

static void ntt_ct_std2rev128_base(int32_t *a) {
  ntt_red_ct_std2rev(a, 128, rev_shoup_sred_ntt128_12289);
}

static void ntt_ct_std2rev256_asm(int32_t *a) {
  ntt_red_ct_std2rev_asm(a, 256, rev_shoup_sred_ntt256_12289);
}

static void ntt_ct_std2rev256_base(int32_t *a) {
  ntt_red_ct_std2rev(a, 256, rev_shoup_sred_ntt256_12289);
}

static void ntt_ct_std2rev512_asm(int32_t *a) {
  ntt_red_ct_std2rev_asm(a, 512, rev_shoup_sred_ntt512_12289);
}

static void ntt_ct_std2rev512_base(int32_t *a) {
  ntt_red_ct_std2rev(a, 512, rev_shoup_sred_ntt512_12289);
}

static void ntt_ct_std2rev1024_asm(int32_t *a) {
  ntt_red_ct_std2rev_asm(a, 1024, rev_shoup_sred_ntt1024_12289);
}

static void ntt_ct_std2rev1024_base(int32_t *a) {
  ntt_red_ct_std2rev(a, 1024, rev_shoup_sred_ntt1024_12289);
}

static void ntt_ct_std2rev2048_asm(int32_t *a) {
  ntt_red_ct_std2rev_asm(a, 2048, rev_shoup_sred_ntt2048_12289);
}

static void ntt_ct_std2rev2048_base(int32_t *a) {
  ntt_red_ct_std2rev(a, 2048, rev_shoup_sred_ntt2048_12289);
}


/*
 * mulntt_ct_std2rev
 */
static void mulntt_ct_std2rev16_asm(int32_t *a) {
  mulntt_red_ct_std2rev_asm(a, 16, rev_shoup_sred_scaled_ntt16_12289);
}

static void mulntt_ct_std2rev16_base(int32_t *a) {
  mulntt_red_ct_std2rev(a, 16, rev_shoup_sred_scaled_ntt16_12289);
}

static void mulntt_ct_std2rev128_asm(int32_t *a) {
  mulntt_red_ct_std2rev_asm(a, 128, rev_shoup_sred_scaled_ntt128_12289);
}

static void mulntt_ct_std2rev128_base(int32_t *a) {
  mulntt_red_ct_std2rev(a, 128, rev_shoup_sred_scaled_ntt128_12289);
}

static void mulntt_ct_std2rev256_asm(int32_t *a) {
  mulntt_red_ct_std2rev_asm(a, 256, rev_shoup_sred_scaled_ntt256_12289);
}

static void mulntt_ct_std2rev256_base(int32_t *a) {
  mulntt_red_ct_std2rev(a, 256, rev_shoup_sred_scaled_ntt256_12289);
}

static void mulntt_ct_std2rev512_asm(int32_t *a) {
  mulntt_red_ct_std2rev_asm(a, 512, rev_shoup_sred_scaled_ntt512_12289);
}

static void mulntt_ct_std2rev512_base(int32_t *a) {
  mulntt_red_ct_std2rev(a, 512, rev_shoup_sred_scaled_ntt512_12289);
}

static void mulntt_ct_std2rev1024_asm(int32_t *a) {
  mulntt_red_ct_std2rev_asm(a, 1024, rev_shoup_sred_scaled_ntt1024_12289);
}

static void mulntt_ct_std2rev1024_base(int32_t *a) {
  mulntt_red_ct_std2rev(a, 1024, rev_shoup_sred_scaled_ntt1024_12289);
}

static void mulntt_ct_std2rev2048_asm(int32_t *a) {
  mulntt_red_ct_std2rev_asm(a, 2048, rev_shoup_sred_scaled_ntt2048_12289);
}

static void mulntt_ct_std2rev2048_base(int32_t *a) {
  mulntt_red_ct_std2rev(a, 2048, rev_shoup_sred_scaled_ntt2048_12289);
}



/*
 * ntt_gs_rev2std
 */
static void ntt_gs_rev2std16_asm(int32_t *a) {
  ntt_red_gs_rev2std_asm(a, 16, rev_shoup_sred_ntt16_12289);
}

static void ntt_gs_rev2std16_base(int32_t *a) {
  ntt_red_gs_rev2std(a, 16, rev_shoup_sred_ntt16_12289);
}

static void ntt_gs_rev2std128_asm(int32_t *a) {
  ntt_red_gs_rev2std_asm(a, 128, rev_shoup_sred_ntt128_12289);
}

static void ntt_gs_rev2std128_base(int32_t *a) {
  ntt_red_gs_rev2std(a, 128, rev_shoup_sred_ntt128_12289);
}

static void ntt_gs_rev2std256_asm(int32_t *a) {
  ntt_red_gs_rev2std_asm(a, 256, rev_shoup_sred_ntt256_12289);
}

static void ntt_gs_rev2std256_base(int32_t *a) {
  ntt_red_gs_rev2std(a, 256, rev_shoup_sred_ntt256_12289);
}

static void ntt_gs_rev2std512_asm(int32_t *a) {
  ntt_red_gs_rev2std_asm(a, 512, rev_shoup_sred_ntt512_12289);
}

static void ntt_gs_rev2std512_base(int32_t *a) {
  ntt_red_gs_rev2std(a, 512, rev_shoup_sred_ntt512_12289);
}

static void ntt_gs_rev2std1024_asm(int32_t *a) {
  ntt_red_gs_rev2std_asm(a, 1024, rev_shoup_sred_ntt1024_12289);
}

static void ntt_gs_rev2std1024_base(int32_t *a) {
  ntt_red_gs_rev2std(a, 1024, rev_shoup_sred_ntt1024_12289);
}

static void ntt_gs_rev2std2048_asm(int32_t *a) {
  ntt_red_gs_rev2std_asm(a, 2048, rev_shoup_sred_ntt2048_12289);
}

static void ntt_gs_rev2std2048_base(int32_t *a) {
  ntt_red_gs_rev2std(a, 2048, rev_shoup_sred_ntt2048_12289);
}



/*
 * nttmul_gs_rev2std
 */
static void nttmul_gs_rev2std16_asm(int32_t *a) {
  nttmul_red_gs_rev2std_asm(a, 16, rev_shoup_sred_scaled_ntt16_12289);
}

static void nttmul_gs_rev2std16_base(int32_t *a) {
  nttmul_red_gs_rev2std(a, 16, rev_shoup_sred_scaled_ntt16_12289);
}

static void nttmul_gs_rev2std128_asm(int32_t *a) {
  nttmul_red_gs_rev2std_asm(a, 128, rev_shoup_sred_scaled_ntt128_12289);
}

static void nttmul_gs_rev2std128_base(int32_t *a) {
  nttmul_red_gs_rev2std(a, 128, rev_shoup_sred_scaled_ntt128_12289);
}

static void nttmul_gs_rev2std256_asm(int32_t *a) {
  nttmul_red_gs_rev2std_asm(a, 256, rev_shoup_sred_scaled_ntt256_12289);
}

static void nttmul_gs_rev2std256_base(int32_t *a) {
  nttmul_red_gs_rev2std(a, 256, rev_shoup_sred_scaled_ntt256_12289);
}

static void nttmul_gs_rev2std512_asm(int32_t *a) {
  nttmul_red_gs_rev2std_asm(a, 512, rev_shoup_sred_scaled_ntt512_12289);
}

static void nttmul_gs_rev2std512_base(int32_t *a) {
  nttmul_red_gs_rev2std(a, 512, rev_shoup_sred_scaled_ntt512_12289);
}

static void nttmul_gs_rev2std1024_asm(int32_t *a) {
  nttmul_red_gs_rev2std_asm(a, 1024, rev_shoup_sred_scaled_ntt1024_12289);
}

static void nttmul_gs_rev2std1024_base(int32_t *a) {
  nttmul_red_gs_rev2std(a, 1024, rev_shoup_sred_scaled_ntt1024_12289);
}

static void nttmul_gs_rev2std2048_asm(int32_t *a) {
  nttmul_red_gs_rev2std_asm(a, 2048, rev_shoup_sred_scaled_ntt2048_12289);
}

static void nttmul_gs_rev2std2048_base(int32_t *a) {
  nttmul_red_gs_rev2std(a, 2048, rev_shoup_sred_scaled_ntt2048_12289);
}



/*
 * ntt_gs_std2rev
 */
static void ntt_gs_std2rev16_asm(int32_t *a) {
  ntt_red_gs_std2rev_asm(a, 16, shoup_sred_ntt16_12289);
}

static void ntt_gs_std2rev16_base(int32_t *a) {
  ntt_red_gs_std2rev(a, 16, shoup_sred_ntt16_12289);
}

static void ntt_gs_std2rev128_asm(int32_t *a) {
  ntt_red_gs_std2rev_asm(a, 128, shoup_sred_ntt128_12289);
}

static void ntt_gs_std2rev128_base(int32_t *a) {
  ntt_red_gs_std2rev(a, 128, shoup_sred_ntt128_12289);
}

static void ntt_gs_std2rev256_asm(int32_t *a) {
  ntt_red_gs_std2rev_asm(a, 256, shoup_sred_ntt256_12289);
}

static void ntt_gs_std2rev256_base(int32_t *a) {
  ntt_red_gs_std2rev(a, 256, shoup_sred_ntt256_12289);
}

static void ntt_gs_std2rev512_asm(int32_t *a) {
  ntt_red_gs_std2rev_asm(a, 512, shoup_sred_ntt512_12289);
}

static void ntt_gs_std2rev512_base(int32_t *a) {
  ntt_red_gs_std2rev(a, 512, shoup_sred_ntt512_12289);
}

static void ntt_gs_std2rev1024_asm(int32_t *a) {
  ntt_red_gs_std2rev_asm(a, 1024, shoup_sred_ntt1024_12289);
}

static void ntt_gs_std2rev1024_base(int32_t *a) {
  ntt_red_gs_std2rev(a, 1024, shoup_sred_ntt1024_12289);
}

static void ntt_gs_std2rev2048_asm(int32_t *a) {
  ntt_red_gs_std2rev_asm(a, 2048, shoup_sred_ntt2048_12289);
}

static void ntt_gs_std2rev2048_base(int32_t *a) {
  ntt_red_gs_std2rev(a, 2048, shoup_sred_ntt2048_12289);
}


/*
 * nttmul_gs_std2rev
 */
static void nttmul_gs_std2rev16_asm(int32_t *a) {
  nttmul_red_gs_std2rev_asm(a, 16, shoup_sred_scaled_ntt16_12289);
}

static void nttmul_gs_std2rev16_base(int32_t *a) {
  nttmul_red_gs_std2rev(a, 16, shoup_sred_scaled_ntt16_12289);
}

static void nttmul_gs_std2rev128_asm(int32_t *a) {
  nttmul_red_gs_std2rev_asm(a, 128, shoup_sred_scaled_ntt128_12289);
}

static void nttmul_gs_std2rev128_base(int32_t *a) {
  nttmul_red_gs_std2rev(a, 128, shoup_sred_scaled_ntt128_12289);
}

static void nttmul_gs_std2rev256_asm(int32_t *a) {
  nttmul_red_gs_std2rev_asm(a, 256, shoup_sred_scaled_ntt256_12289);
}

static void nttmul_gs_std2rev256_base(int32_t *a) {
  nttmul_red_gs_std2rev(a, 256, shoup_sred_scaled_ntt256_12289);
}

static void nttmul_gs_std2rev512_asm(int32_t *a) {
  nttmul_red_gs_std2rev_asm(a, 512, shoup_sred_scaled_ntt512_12289);
}

static void nttmul_gs_std2rev512_base(int32_t *a) {
  nttmul_red_gs_std2rev(a, 512, shoup_sred_scaled_ntt512_12289);
}

static void nttmul_gs_std2rev1024_asm(int32_t *a) {
  nttmul_red_gs_std2rev_asm(a, 1024, shoup_sred_scaled_ntt1024_12289);
}

static void nttmul_gs_std2rev1024_base(int32_t *a) {
  nttmul_red_gs_std2rev(a, 1024, shoup_sred_scaled_ntt1024_12289);
}

static void nttmul_gs_std2rev2048_asm(int32_t *a) {
  nttmul_red_gs_std2rev_asm(a, 2048, shoup_sred_scaled_ntt2048_12289);
}

static void nttmul_gs_std2rev2048_base(int32_t *a) {
  nttmul_red_gs_std2rev(a, 2048, shoup_sred_scaled_ntt2048_12289);
}




#if 0
static void test_nttmul_gs_std2rev16(void) {
  int32_t a[16];
  uint32_t i;

  printf("Test: nttmul_gs_std2rev16\n");
  for (i=0; i<16; i++) {
    a[i] = 100+i;
  }
  printf("--> input\n");
  print_array(stdout, a, 16);
  nttmul_red_gs_std2rev(a, 16, shoup_sred_scaled_ntt16_12289);
  printf("--> base output\n");
  print_array(stdout, a, 16);
  for (i=0; i<16; i++) {
    a[i] = 100+i;
  }
  nttmul_red_gs_std2rev_asm(a, 16, shoup_sred_scaled_ntt16_12289);
  printf("--> assembler output\n");
  print_array(stdout, a, 16);
  printf("\n");

  printf("Test: nttmul_gs_std2rev16\n");
  for (i=0; i<16; i++) {
    a[i] = 100+3*i;
  }
  printf("--> input\n");
  print_array(stdout, a, 16);
  nttmul_red_gs_std2rev(a, 16, shoup_sred_scaled_ntt16_12289);
  printf("--> base output\n");
  print_array(stdout, a, 16);
  for (i=0; i<16; i++) {
    a[i] = 100+3*i;
  }
  nttmul_red_gs_std2rev_asm(a, 16, shoup_sred_scaled_ntt16_12289);
  printf("--> assembler output\n");
  print_array(stdout, a, 16);
  printf("\n");

  printf("Test: nttmul_gs_std2rev16\n");
  for (i=0; i<16; i++) {
    a[i] = 100+7*i;
  }
  printf("--> input\n");
  print_array(stdout, a, 16);
  nttmul_red_gs_std2rev(a, 16, shoup_sred_scaled_ntt16_12289);
  printf("--> base output\n");
  print_array(stdout, a, 16);
  for (i=0; i<16; i++) {
    a[i] = 100+7*i;
  }
  nttmul_red_gs_std2rev_asm(a, 16, shoup_sred_scaled_ntt16_12289);
  printf("--> assembler output\n");
  print_array(stdout, a, 16);
  printf("\n");
}
#endif

#if 0
static void test_nttmul_gs_std2rev128(void) {
  int32_t a[128];
  uint32_t i;

  printf("Test: nttmul_gs_std2rev128\n");
  for (i=0; i<128; i++) {
    a[i] = 100+i;
  }
  printf("--> input\n");
  print_array(stdout, a, 128);
  nttmul_red_gs_std2rev(a, 128, shoup_sred_scaled_ntt128_12289);
  printf("--> base output\n");
  print_array(stdout, a, 128);
  for (i=0; i<128; i++) {
    a[i] = 100+i;
  }
  nttmul_red_gs_std2rev_asm(a, 128, shoup_sred_scaled_ntt128_12289);
  printf("--> assembler output\n");
  print_array(stdout, a, 128);
  printf("\n");

  printf("Test: nttmul_gs_std2rev128\n");
  for (i=0; i<128; i++) {
    a[i] = 100+3*i;
  }
  printf("--> input\n");
  print_array(stdout, a, 128);
  nttmul_red_gs_std2rev(a, 128, shoup_sred_scaled_ntt128_12289);
  printf("--> base output\n");
  print_array(stdout, a, 128);
  for (i=0; i<128; i++) {
    a[i] = 100+3*i;
  }
  nttmul_red_gs_std2rev_asm(a, 128, shoup_sred_scaled_ntt128_12289);
  printf("--> assembler output\n");
  print_array(stdout, a, 128);
  printf("\n");

  printf("Test: nttmul_gs_std2rev128\n");
  for (i=0; i<128; i++) {
    a[i] = 100+7*i;
  }
  printf("--> input\n");
  print_array(stdout, a, 128);
  nttmul_red_gs_std2rev(a, 128, shoup_sred_scaled_ntt128_12289);
  printf("--> base output\n");
  print_array(stdout, a, 128);
  for (i=0; i<128; i++) {
    a[i] = 100+7*i;
  }
  nttmul_red_gs_std2rev_asm(a, 128, shoup_sred_scaled_ntt128_12289);
  printf("--> assembler output\n");
  print_array(stdout, a, 128);
  printf("\n");
}

#endif


/*
 * Tests
 */
static void tests16(void) {
  printf("===== Size 16 =====\n");
  cross_check("ntt_red_ct_rev2std_asm", 16, ntt_ct_rev2std16_asm, ntt_ct_rev2std16_base);
  speed_test2("ntt_red_ct_rev2std", 16, ntt_red_ct_rev2std);
  speed_test2("ntt_red_ct_rev2std_asm", 16, ntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("mulntt_red_ct_rev2std_asm", 16, mulntt_ct_rev2std16_asm, mulntt_ct_rev2std16_base);
  speed_test2("mulntt_red_ct_rev2std", 16, mulntt_red_ct_rev2std);
  speed_test2("mulntt_red_ct_rev2std_asm", 16, mulntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_ct_std2rev_asm", 16, ntt_ct_std2rev16_asm, ntt_ct_std2rev16_base);
  speed_test2("ntt_red_ct_std2rev", 16, ntt_red_ct_std2rev);
  speed_test2("ntt_red_ct_std2rev_asm", 16, ntt_red_ct_std2rev_asm);
  printf("\n");
  cross_check("mulntt_red_ct_std2rev_asm", 16, mulntt_ct_std2rev16_asm, mulntt_ct_std2rev16_base);
  speed_test2("mulntt_red_ct_std2rev", 16, mulntt_red_ct_std2rev);
  speed_test2("mulntt_red_ct_std2rev_asm", 16, mulntt_red_ct_std2rev_asm);
  printf("\n");

  cross_check("ntt_red_gs_rev2std_asm", 16, ntt_gs_rev2std16_asm, ntt_gs_rev2std16_base);
  speed_test2("ntt_red_gs_rev2std", 16, ntt_red_gs_rev2std);
  speed_test2("ntt_red_gs_rev2std_asm", 16, ntt_red_gs_rev2std_asm);
  printf("\n");
  cross_check("nttmul_red_gs_rev2std_asm", 16, nttmul_gs_rev2std16_asm, nttmul_gs_rev2std16_base);
  speed_test2("nttmul_red_gs_rev2std", 16, nttmul_red_gs_rev2std);
  speed_test2("nttmul_red_gs_rev2std_asm", 16, nttmul_red_gs_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_gs_std2rev_asm", 16, ntt_gs_std2rev16_asm, ntt_gs_std2rev16_base);
  speed_test2("ntt_red_gs_std2rev", 16, ntt_red_gs_std2rev);
  speed_test2("ntt_red_gs_std2rev_asm", 16, ntt_red_gs_std2rev_asm);
  printf("\n");
  cross_check("nttmul_red_gs_std2rev_asm", 16, nttmul_gs_std2rev16_asm, nttmul_gs_std2rev16_base);
  speed_test2("nttmul_red_gs_std2rev", 16, nttmul_red_gs_std2rev);
  speed_test2("nttmul_red_gs_std2rev_asm", 16, nttmul_red_gs_std2rev_asm);
  printf("\n");
}

static void tests128(void) {
  printf("===== Size 128 =====\n");
  cross_check("ntt_red_ct_rev2std_asm", 128, ntt_ct_rev2std128_asm, ntt_ct_rev2std128_base);
  speed_test2("ntt_red_ct_rev2std", 128, ntt_red_ct_rev2std);
  speed_test2("ntt_red_ct_rev2std_asm", 128, ntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("mulntt_red_ct_rev2std_asm", 128, mulntt_ct_rev2std128_asm, mulntt_ct_rev2std128_base);
  speed_test2("mulntt_red_ct_rev2std", 128, mulntt_red_ct_rev2std);
  speed_test2("mulntt_red_ct_rev2std_asm", 128, mulntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_ct_std2rev_asm", 128, ntt_ct_std2rev128_asm, ntt_ct_std2rev128_base);
  speed_test2("ntt_red_ct_std2rev", 128, ntt_red_ct_std2rev);
  speed_test2("ntt_red_ct_std2rev_asm", 128, ntt_red_ct_std2rev_asm);
  printf("\n");
  cross_check("mulntt_red_ct_std2rev_asm", 128, mulntt_ct_std2rev128_asm, mulntt_ct_std2rev128_base);
  speed_test2("mulntt_red_ct_std2rev", 128, mulntt_red_ct_std2rev);
  speed_test2("mulntt_red_ct_std2rev_asm", 128, mulntt_red_ct_std2rev_asm);
  printf("\n");

  cross_check("ntt_red_gs_rev2std_asm", 128, ntt_gs_rev2std128_asm, ntt_gs_rev2std128_base);
  speed_test2("ntt_red_gs_rev2std", 128, ntt_red_gs_rev2std);
  speed_test2("ntt_red_gs_rev2std_asm", 128, ntt_red_gs_rev2std_asm);
  printf("\n");
  cross_check("nttmul_red_gs_rev2std_asm", 128, nttmul_gs_rev2std128_asm, nttmul_gs_rev2std128_base);
  speed_test2("nttmul_red_gs_rev2std", 128, nttmul_red_gs_rev2std);
  speed_test2("nttmul_red_gs_rev2std_asm", 128, nttmul_red_gs_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_gs_std2rev_asm", 128, ntt_gs_std2rev128_asm, ntt_gs_std2rev128_base);
  speed_test2("ntt_red_gs_std2rev", 128, ntt_red_gs_std2rev);
  speed_test2("ntt_red_gs_std2rev_asm", 128, ntt_red_gs_std2rev_asm);
  printf("\n");
  cross_check("nttmul_red_gs_std2rev_asm", 128, nttmul_gs_std2rev128_asm, nttmul_gs_std2rev128_base);
  speed_test2("nttmul_red_gs_std2rev", 128, nttmul_red_gs_std2rev);
  speed_test2("nttmul_red_gs_std2rev_asm", 128, nttmul_red_gs_std2rev_asm);
  printf("\n");
}

static void tests256(void) {
  printf("===== Size 256 =====\n");
  cross_check("ntt_red_ct_rev2std_asm", 256, ntt_ct_rev2std256_asm, ntt_ct_rev2std256_base);
  speed_test2("ntt_red_ct_rev2std", 256, ntt_red_ct_rev2std);
  speed_test2("ntt_red_ct_rev2std_asm", 256, ntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("mulntt_red_ct_rev2std_asm", 256, mulntt_ct_rev2std256_asm, mulntt_ct_rev2std256_base);
  speed_test2("mulntt_red_ct_rev2std", 256, mulntt_red_ct_rev2std);
  speed_test2("mulntt_red_ct_rev2std_asm", 256, mulntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_ct_std2rev_asm", 256, ntt_ct_std2rev256_asm, ntt_ct_std2rev256_base);
  speed_test2("ntt_red_ct_std2rev", 256, ntt_red_ct_std2rev);
  speed_test2("ntt_red_ct_std2rev_asm", 256, ntt_red_ct_std2rev_asm);
  printf("\n");
  cross_check("mulntt_red_ct_std2rev_asm", 256, mulntt_ct_std2rev256_asm, mulntt_ct_std2rev256_base);
  speed_test2("mulntt_red_ct_std2rev", 256, mulntt_red_ct_std2rev);
  speed_test2("mulntt_red_ct_std2rev_asm", 256, mulntt_red_ct_std2rev_asm);
  printf("\n");

  cross_check("ntt_red_gs_rev2std_asm", 256, ntt_gs_rev2std256_asm, ntt_gs_rev2std256_base);
  speed_test2("ntt_red_gs_rev2std", 256, ntt_red_gs_rev2std);
  speed_test2("ntt_red_gs_rev2std_asm", 256, ntt_red_gs_rev2std_asm);
  printf("\n");
  cross_check("nttmul_red_gs_rev2std_asm", 256, nttmul_gs_rev2std256_asm, nttmul_gs_rev2std256_base);
  speed_test2("nttmul_red_gs_rev2std", 256, nttmul_red_gs_rev2std);
  speed_test2("nttmul_red_gs_rev2std_asm", 256, nttmul_red_gs_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_gs_std2rev_asm", 256, ntt_gs_std2rev256_asm, ntt_gs_std2rev256_base);
  speed_test2("ntt_red_gs_std2rev", 256, ntt_red_gs_std2rev);
  speed_test2("ntt_red_gs_std2rev_asm", 256, ntt_red_gs_std2rev_asm);
  printf("\n");
  cross_check("nttmul_red_gs_std2rev_asm", 256, nttmul_gs_std2rev256_asm, nttmul_gs_std2rev256_base);
  speed_test2("nttmul_red_gs_std2rev", 256, nttmul_red_gs_std2rev);
  speed_test2("nttmul_red_gs_std2rev_asm", 256, nttmul_red_gs_std2rev_asm);
  printf("\n");
}

static void tests512(void) {
  printf("===== Size 512 =====\n");
  cross_check("ntt_red_ct_rev2std_asm", 512, ntt_ct_rev2std512_asm, ntt_ct_rev2std512_base);
  speed_test2("ntt_red_ct_rev2std", 512, ntt_red_ct_rev2std);
  speed_test2("ntt_red_ct_rev2std_asm", 512, ntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("mulntt_red_ct_rev2std_asm", 512, mulntt_ct_rev2std512_asm, mulntt_ct_rev2std512_base);
  speed_test2("mulntt_red_ct_rev2std", 512, mulntt_red_ct_rev2std);
  speed_test2("mulntt_red_ct_rev2std_asm", 512, mulntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_ct_std2rev_asm", 512, ntt_ct_std2rev512_asm, ntt_ct_std2rev512_base);
  speed_test2("ntt_red_ct_std2rev", 512, ntt_red_ct_std2rev);
  speed_test2("ntt_red_ct_std2rev_asm", 512, ntt_red_ct_std2rev_asm);
  printf("\n");
  cross_check("mulntt_red_ct_std2rev_asm", 512, mulntt_ct_std2rev512_asm, mulntt_ct_std2rev512_base);
  speed_test2("mulntt_red_ct_std2rev", 512, mulntt_red_ct_std2rev);
  speed_test2("mulntt_red_ct_std2rev_asm", 512, mulntt_red_ct_std2rev_asm);
  printf("\n");

  cross_check("ntt_red_gs_rev2std_asm", 512, ntt_gs_rev2std512_asm, ntt_gs_rev2std512_base);
  speed_test2("ntt_red_gs_rev2std", 512, ntt_red_gs_rev2std);
  speed_test2("ntt_red_gs_rev2std_asm", 512, ntt_red_gs_rev2std_asm);
  printf("\n");
  cross_check("nttmul_red_gs_rev2std_asm", 512, nttmul_gs_rev2std512_asm, nttmul_gs_rev2std512_base);
  speed_test2("nttmul_red_gs_rev2std", 512, nttmul_red_gs_rev2std);
  speed_test2("nttmul_red_gs_rev2std_asm", 512, nttmul_red_gs_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_gs_std2rev_asm", 512, ntt_gs_std2rev512_asm, ntt_gs_std2rev512_base);
  speed_test2("ntt_red_gs_std2rev", 512, ntt_red_gs_std2rev);
  speed_test2("ntt_red_gs_std2rev_asm", 512, ntt_red_gs_std2rev_asm);
  printf("\n");
  cross_check("nttmul_red_gs_std2rev_asm", 512, nttmul_gs_std2rev512_asm, nttmul_gs_std2rev512_base);
  speed_test2("nttmul_red_gs_std2rev", 512, nttmul_red_gs_std2rev);
  speed_test2("nttmul_red_gs_std2rev_asm", 512, nttmul_red_gs_std2rev_asm);
  printf("\n");
}

static void tests1024(void) {
  printf("===== Size 1024 =====\n");
  cross_check("ntt_red_ct_rev2std_asm", 1024, ntt_ct_rev2std1024_asm, ntt_ct_rev2std1024_base);
  speed_test2("ntt_red_ct_rev2std", 1024, ntt_red_ct_rev2std);
  speed_test2("ntt_red_ct_rev2std_asm", 1024, ntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("mulntt_red_ct_rev2std_asm", 1024, mulntt_ct_rev2std1024_asm, mulntt_ct_rev2std1024_base);
  speed_test2("mulntt_red_ct_rev2std", 1024, mulntt_red_ct_rev2std);
  speed_test2("mulntt_red_ct_rev2std_asm", 1024, mulntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_ct_std2rev_asm", 1024, ntt_ct_std2rev1024_asm, ntt_ct_std2rev1024_base);
  speed_test2("ntt_red_ct_std2rev", 1024, ntt_red_ct_std2rev);
  speed_test2("ntt_red_ct_std2rev_asm", 1024, ntt_red_ct_std2rev_asm);
  printf("\n");
  cross_check("mulntt_red_ct_std2rev_asm", 1024, mulntt_ct_std2rev1024_asm, mulntt_ct_std2rev1024_base);
  speed_test2("mulntt_red_ct_std2rev", 1024, mulntt_red_ct_std2rev);
  speed_test2("mulntt_red_ct_std2rev_asm", 1024, mulntt_red_ct_std2rev_asm);
  printf("\n");

  cross_check("ntt_red_gs_rev2std_asm", 1024, ntt_gs_rev2std1024_asm, ntt_gs_rev2std1024_base);
  speed_test2("ntt_red_gs_rev2std", 1024, ntt_red_gs_rev2std);
  speed_test2("ntt_red_gs_rev2std_asm", 1024, ntt_red_gs_rev2std_asm);
  printf("\n");
  cross_check("nttmul_red_gs_rev2std_asm", 1024, nttmul_gs_rev2std1024_asm, nttmul_gs_rev2std1024_base);
  speed_test2("nttmul_red_gs_rev2std", 1024, nttmul_red_gs_rev2std);
  speed_test2("nttmul_red_gs_rev2std_asm", 1024, nttmul_red_gs_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_gs_std2rev_asm", 1024, ntt_gs_std2rev1024_asm, ntt_gs_std2rev1024_base);
  speed_test2("ntt_red_gs_std2rev", 1024, ntt_red_gs_std2rev);
  speed_test2("ntt_red_gs_std2rev_asm", 1024, ntt_red_gs_std2rev_asm);
  printf("\n");
  cross_check("nttmul_red_gs_std2rev_asm", 1024, nttmul_gs_std2rev1024_asm, nttmul_gs_std2rev1024_base);
  speed_test2("nttmul_red_gs_std2rev", 1024, nttmul_red_gs_std2rev);
  speed_test2("nttmul_red_gs_std2rev_asm", 1024, nttmul_red_gs_std2rev_asm);
  printf("\n");
}

static void tests2048(void) {
  printf("===== Size 2048 =====\n");
  cross_check("ntt_red_ct_rev2std_asm", 2048, ntt_ct_rev2std2048_asm, ntt_ct_rev2std2048_base);
  speed_test2("ntt_red_ct_rev2std", 2048, ntt_red_ct_rev2std);
  speed_test2("ntt_red_ct_rev2std_asm", 2048, ntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("mulntt_red_ct_rev2std_asm", 2048, mulntt_ct_rev2std2048_asm, mulntt_ct_rev2std2048_base);
  speed_test2("mulntt_red_ct_rev2std", 2048, mulntt_red_ct_rev2std);
  speed_test2("mulntt_red_ct_rev2std_asm", 2048, mulntt_red_ct_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_ct_std2rev_asm", 2048, ntt_ct_std2rev2048_asm, ntt_ct_std2rev2048_base);
  speed_test2("ntt_red_ct_std2rev", 2048, ntt_red_ct_std2rev);
  speed_test2("ntt_red_ct_std2rev_asm", 2048, ntt_red_ct_std2rev_asm);
  printf("\n");
  cross_check("mulntt_red_ct_std2rev_asm", 2048, mulntt_ct_std2rev2048_asm, mulntt_ct_std2rev2048_base);
  speed_test2("mulntt_red_ct_std2rev", 2048, mulntt_red_ct_std2rev);
  speed_test2("mulntt_red_ct_std2rev_asm", 2048, mulntt_red_ct_std2rev_asm);
  printf("\n");

  cross_check("ntt_red_gs_rev2std_asm", 2048, ntt_gs_rev2std2048_asm, ntt_gs_rev2std2048_base);
  speed_test2("ntt_red_gs_rev2std", 2048, ntt_red_gs_rev2std);
  speed_test2("ntt_red_gs_rev2std_asm", 2048, ntt_red_gs_rev2std_asm);
  printf("\n");
  cross_check("nttmul_red_gs_rev2std_asm", 2048, nttmul_gs_rev2std2048_asm, nttmul_gs_rev2std2048_base);
  speed_test2("nttmul_red_gs_rev2std", 2048, nttmul_red_gs_rev2std);
  speed_test2("nttmul_red_gs_rev2std_asm", 2048, nttmul_red_gs_rev2std_asm);
  printf("\n");
  cross_check("ntt_red_gs_std2rev_asm", 2048, ntt_gs_std2rev2048_asm, ntt_gs_std2rev2048_base);
  speed_test2("ntt_red_gs_std2rev", 2048, ntt_red_gs_std2rev);
  speed_test2("ntt_red_gs_std2rev_asm", 2048, ntt_red_gs_std2rev_asm);
  printf("\n");
  cross_check("nttmul_red_gs_std2rev_asm", 2048, nttmul_gs_std2rev2048_asm, nttmul_gs_std2rev2048_base);
  speed_test2("nttmul_red_gs_std2rev", 2048, nttmul_red_gs_std2rev);
  speed_test2("nttmul_red_gs_std2rev_asm", 2048, nttmul_red_gs_std2rev_asm);
  printf("\n");
}


static void run_tests(void) {
  tests16();
  tests128();
  tests256();
  tests512();
  tests1024();
  tests2048();
}

int main(void) {
  if (avx2_supported()) {
    printf("AVX2 is supported\n\n");
    run_tests();
  } else {
    printf("AVX2 is not supported\n");
  }
  return 0;
}
