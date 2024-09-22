#ifndef SIMD_H
#define SIMD_H
#include <stdint.h>

void add_8_int16(int16_t *a, const int16_t *b);
void substract_8_int16(int16_t *a, const int16_t *b);
int16_t fullNnuePass(int16_t *pInput1, int16_t *pInput2, int8_t *pWeights1, int16_t *pBias1,
                     int8_t *pWeights2, int16_t *pBias2, int8_t *pWeights3, int16_t *pBias3);
int16_t fullNnueuPass(int16_t *pInput, int8_t *pWeights11, int8_t *pWeights12, int16_t *pBias1,
                      int8_t *pWeights2, int16_t *pBias2, int8_t *pWeights3, int16_t *pBias3);
#endif