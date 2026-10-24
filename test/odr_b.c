// ODR check part B: second TU including byte_order.h.
#include "byte_order.h"
int odr_b(void) { uint32_t x = ftohl(0x12345678); return (int)(x >> 31); }
