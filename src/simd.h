#ifndef SIMD_H
#define SIMD_H
#include <stdint.h>

extern int8_t thirdLayerWeights[8 * 4];
extern int8_t finalLayerWeights[8];

extern int16_t secondLayerBiases[8];
extern int16_t thirdLayerBiases[4];
extern int16_t finalLayerBias;

void add_8_int16(int16_t *a, const int16_t *b);
void substract_8_int16(int16_t *a, const int16_t *b);
int16_t fullNnueuPass(int16_t *pInput, int8_t *pWeights11, int8_t *pWeights12);
// int16_t fullNnueuPassArmadilloRef(const int16_t *pInput,const int8_t *pWeights11, const int8_t *pWeights12);
#endif