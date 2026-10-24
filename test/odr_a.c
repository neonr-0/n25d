// ODR check part A: TU including byte_order.h (see test-odr in makefile).
// byte_order.h helpers must be static inline or linking two such TUs fails
// with "multiple definition of htoff/ftohf/ftohl/htofl".
#include "byte_order.h"
int odr_a(void) { uint32_t x = htofl(0x12345678); return (int)(x & 1); }
