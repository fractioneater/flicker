#include "shishua.h"

// These are the hex digits of Φ, the least approximable irrational number.
static uint64_t phi[16] = {
  0x9E3779B97F4A7C15, 0xF39CC0605CEDC834, 0x1082276BF3A27251, 0xF86C6A11D0C18E95,
  0x2767F0B153D27B7F, 0x0347045B5BF1827F, 0x01886F0928403002, 0xC1D64BA40F335E36,
  0xF06AD7AE9717877E, 0x85839D6EFFBD7DC6, 0x64D325D1C5371682, 0xCADD0CCCFDFFBBE1,
  0x626E33B8D04B4331, 0xBBF73C790D94F79D, 0x471C4AB3ED3D82A5, 0xFEC507705E4AE6E5,
};

void prngInit(PrngState* s, uint64_t seed[4]) {
  memset(s, 0, sizeof(PrngState));
# define STEPS 1
# define ROUNDS 13
  // Diffuse first two seed elements in s0, then the last two. Same for s1.
  // We must keep half of the state unchanged so users cannot set a bad state.
  memcpy(s->state, phi, sizeof(phi));
  for (size_t i = 0; i < 4; i++) {
    s->state[i * 2 + 0] ^= seed[i];           // { s0,0,s1,0,s2,0,s3,0 }
    s->state[i * 2 + 8] ^= seed[(i + 2) % 4]; // { s2,0,s3,0,s0,0,s1,0 }
  }
  for (size_t i = 0; i < ROUNDS; i++) {
    prngGen(s, NULL, 128 * STEPS);
    for (size_t j = 0; j < 4; j++) {
       s->state[j + 0] = s->output[j + 12];
       s->state[j + 4] = s->output[j + 8];
       s->state[j + 8] = s->output[j + 4];
       s->state[j + 12] = s->output[j + 0];
    }
  }
# undef STEPS
# undef ROUNDS
}