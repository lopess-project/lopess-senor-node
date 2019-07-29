/*
 * Parameters:
 * - q = 12289
 * - n = 32
 * - psi = 563
 * - omega = psi^2 = 9744
 * - inverse of psi = 5828
 * - inverse of omega = 11077
 * - inverse of n = 11905
 */

#include "ntt32_tables.h"

const uint16_t ntt32_bitrev[BITREV32_NPAIRS][2] = {
    {     1,    16 }, {     2,     8 }, {     3,    24 }, {     5,    20 },
    {     6,    12 }, {     7,    28 }, {     9,    18 }, {    11,    26 },
    {    13,    22 }, {    15,    30 }, {    19,    25 }, {    23,    29 },
};

const uint16_t ntt32_psi_powers[32] = {
        1,   563,  9744,  4978,   722,   949,  5860,  5728,
     5146,  9283,  3504,  6512,  4134,  4821, 10643,  7266,
    10810,  2975,  3621, 10938,  1305,  9664,  9094,  7698,
     8246,  9545,  3542,  3328,  5736,  9650,  1212,  6461,
};

const uint16_t ntt32_inv_psi_powers[32] = {
        1,  5828, 11077,  2639,  6553,  8961,  8747,  2744,
     4043,  4591,  3195,  2625, 10984,  1351,  8668,  9314,
     1479,  5023,  1646,  7468,  8155,  5777,  8785,  3006,
     7143,  6561,  6429, 11340, 11567,  7311,  2545, 11726,
};

const uint16_t ntt32_scaled_inv_psi_powers[32] = {
    11905, 10935, 10715,  6611,  2893, 12185,  8338,  3158,
     8191,  6672,  2020, 11987,  9560,  9643,  1807, 11812,
     9647,   541,  6964,  7914,  2175,  5941,  6035,   862,
     9824, 12110,  1353,  8035,  6890,  6757,  5840,  7279,
};

const uint16_t ntt32_omega_powers[32] = {
        0,     1,     1, 10810,     1,  5146, 10810,  8246,
        1,   722,  5146,  4134, 10810,  1305,  8246,  5736,
        1,  9744,   722,  5860,  5146,  3504,  4134, 10643,
    10810,  3621,  1305,  9094,  8246,  3542,  5736,  1212,
};

const uint16_t ntt32_omega_powers_rev[32] = {
        0,     1,     1, 10810,     1, 10810,  5146,  8246,
        1, 10810,  5146,  8246,   722,  1305,  4134,  5736,
        1, 10810,  5146,  8246,   722,  1305,  4134,  5736,
     9744,  3621,  3504,  3542,  5860,  9094, 10643,  1212,
};

const uint16_t ntt32_inv_omega_powers[32] = {
        0,     1,     1,  1479,     1,  4043,  1479,  7143,
        1,  6553,  4043, 10984,  1479,  8155,  7143, 11567,
        1, 11077,  6553,  8747,  4043,  3195, 10984,  8668,
     1479,  1646,  8155,  8785,  7143,  6429, 11567,  2545,
};

const uint16_t ntt32_inv_omega_powers_rev[32] = {
        0,     1,     1,  1479,     1,  1479,  4043,  7143,
        1,  1479,  4043,  7143,  6553,  8155, 10984, 11567,
        1,  1479,  4043,  7143,  6553,  8155, 10984, 11567,
    11077,  1646,  3195,  6429,  8747,  8785,  8668,  2545,
};

const uint16_t ntt32_mixed_powers[32] = {
        0, 10810,  5146,  8246,   722,  4134,  1305,  5736,
     9744,  5860,  3504, 10643,  3621,  9094,  3542,  1212,
      563,  4978,   949,  5728,  9283,  6512,  4821,  7266,
     2975, 10938,  9664,  7698,  9545,  3328,  9650,  6461,
};

const uint16_t ntt32_mixed_powers_rev[32] = {
        0, 10810,  5146,  8246,   722,  1305,  4134,  5736,
     9744,  3621,  3504,  3542,  5860,  9094, 10643,  1212,
      563,  2975,  9283,  9545,   949,  9664,  4821,  9650,
     4978, 10938,  6512,  3328,  5728,  7698,  7266,  6461,
};

const uint16_t ntt32_inv_mixed_powers[32] = {
        0,  1479,  4043,  7143,  6553, 10984,  8155, 11567,
    11077,  8747,  3195,  8668,  1646,  8785,  6429,  2545,
     5828,  2639,  8961,  2744,  4591,  2625,  1351,  9314,
     5023,  7468,  5777,  3006,  6561, 11340,  7311, 11726,
};

const uint16_t ntt32_inv_mixed_powers_rev[32] = {
        0,  1479,  4043,  7143,  6553,  8155, 10984, 11567,
    11077,  1646,  3195,  6429,  8747,  8785,  8668,  2545,
     5828,  5023,  4591,  6561,  8961,  5777,  1351,  7311,
     2639,  7468,  2625, 11340,  2744,  3006,  9314, 11726,
};

