/*
// 2019 November 2
//
// The MIT License
//
// Copyright (c) 2019 NEON Software Solutions. http://neonsoftsol.com
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
*/
#pragma once
#ifndef _BYTE_ORDER_H
#define _BYTE_ORDER_H

#ifndef _STDINT_H
#define _STDINT_H
#include <stdint.h>
#endif // _STDINT_H

#ifdef __GLIBC__
# include <endian.h>
#endif

//if x86 compatible cpu
#if defined(i386) || defined(__i386__) || defined(__i486__) || \
	defined(__i586__) || defined(__i686__) || defined(__pentium__) || \
	defined(__pentiumpro__) || defined(__pentium4__) || \
	defined(__nocona__) || defined(prescott) || defined(__core2__) || \
	defined(__k6__) || defined(__k8__) || defined(__athlon__) || \
	defined(__amd64) || defined(__amd64__) || \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_IX86) || \
	defined(_M_AMD64) || defined(_M_IA64) || defined(_M_X64)
//detect if x86-64 instruction set is supported
# if defined(_LP64) || defined(__LP64__) || defined(__x86_64) || \
	defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
#  define CPU_X64
# else
#  define CPU_IA32
# endif
#endif

//detect CPU endianness
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
		__BYTE_ORDER == __LITTLE_ENDIAN) || \
	defined(CPU_IA32) || defined(CPU_X64) || \
	defined(__ia64) || defined(__ia64__) || defined(__alpha__) || defined(_M_ALPHA) || \
	defined(vax) || defined(MIPSEL) || defined(_ARM_)
# define CPU_LITTLE_ENDIAN
# define IS_BIG_ENDIAN 0
# define IS_LITTLE_ENDIAN 1
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
		__BYTE_ORDER == __BIG_ENDIAN) || \
	defined(__sparc) || defined(__sparc__) || defined(sparc) || \
	defined(_ARCH_PPC) || defined(_ARCH_PPC64) || defined(_POWER) || \
	defined(__POWERPC__) || defined(POWERPC) || defined(__powerpc) || \
	defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || \
	defined(__hpux)  || defined(_MIPSEB) || defined(mc68000) || \
	defined(__s390__) || defined(__s390x__) || defined(sel)
# define CPU_BIG_ENDIAN
# define IS_BIG_ENDIAN 1
# define IS_LITTLE_ENDIAN 0
#else
# error "Can't detect CPU architechture"
#endif


//define bswap_32
#if defined(__GNUC__) && defined(CPU_IA32) && !defined(__i386__)
//for x86 CPU
static inline uint32_t bswap_32(uint32_t x) 
{
	__asm("bswap\t%0" : "=r" (x) : "0" (x));
	return x;
}
#elif defined(__GNUC__)  && (__GNUC__ >= 4) && (__GNUC__ > 4 || __GNUC_MINOR__ >= 3)
//for GCC >= 4.3
# define bswap_32(x) __builtin_bswap32(x)
#elif (_MSC_VER > 1300) && (defined(CPU_IA32) || defined(CPU_X64)) /* MS VC */
# define bswap_32(x) _byteswap_ulong((unsigned long)x)
#elif !defined(__STRICT_ANSI__)
//general bswap_32 definition
static inline uint32_t bswap_32(uint32_t x) 
{
	x = ((x << 8) & 0xFF00FF00) | ((x >> 8) & 0x00FF00FF);
	return (x >> 16) | (x << 16);
}
#else //bswap_32
#define bswap_32(x) ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
	(((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))
#endif 

//Main is little endian, big endian need to swap bytes. (reverse from network byte order)
#pragma region file_endianness
//I assume float it's IEEE-754
//hw float to file function
float htoff(const float in_f32)
{
#if IS_BIG_ENDIAN == 1
	float tmp_f32;
	uint8_t *floatToConvert = (uint8_t*)& in_f32;
	uint8_t *returnFloat = (uint8_t*)& tmp_f32;
	// swap the bytes into a temporary buffer
	returnFloat[0] = floatToConvert[3];
	returnFloat[1] = floatToConvert[2];
	returnFloat[2] = floatToConvert[1];
	returnFloat[3] = floatToConvert[0];
	return tmp_f32;
#else
	return in_f32;
#endif
}
//file float to hw function
float ftohf(const float in_f32)
{
#if IS_BIG_ENDIAN == 1
	float tmp_f32;
	uint8_t *floatToConvert = (uint8_t*)& in_f32;
	uint8_t *returnFloat = (uint8_t*)& tmp_f32;
	// swap the bytes into a temporary buffer
	returnFloat[0] = floatToConvert[3];
	returnFloat[1] = floatToConvert[2];
	returnFloat[2] = floatToConvert[1];
	returnFloat[3] = floatToConvert[0];
	return tmp_f32;
#else
	return in_f32;
#endif
}

//file to hw int32 function
uint32_t ftohl(uint32_t x)
{
#if IS_BIG_ENDIAN == 1	
	return bswap_32(x);
#else
	return x;
#endif
}
//hw int32 to file function
uint32_t htofl(uint32_t x)
{
#if IS_BIG_ENDIAN == 1
	return bswap_32(x);
#else 
	return x;
#endif
}
#pragma endregion file_endianness
#endif //_BYTE_ORDER_H