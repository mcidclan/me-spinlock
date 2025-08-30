#pragma once
#include <psppower.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <cstring>
#include <malloc.h>
#include "kcall.h"

#define u8  unsigned char
#define u16 unsigned short int
#define u32 unsigned int

#define hwp          volatile u32*
#define hw(addr)     (*((hwp)(addr)))
#define uhw(addr)    ((u32*)(0x40000000 | ((u32)addr)))

#define UNCACHED_USER_MASK    0x40000000
#define CACHED_KERNEL_MASK    0x80000000
#define UNCACHED_KERNEL_MASK  0xA0000000

#define ME_HANDLER_BASE       0xbfc00000

#define delayPipeline() asm volatile("nop; nop; nop; nop; nop; nop; nop;")

inline void meGetUncached32(volatile u32** const mem, const u32 size) {
  static void* _base = nullptr;
  if (!_base) {
    const u32 byteCount = size * 4;
    _base = memalign(16, byteCount);
    memset(_base, 0, byteCount);
    sceKernelDcacheWritebackInvalidateAll();
    *mem = (u32*)(UNCACHED_USER_MASK | (u32)_base);
    __asm__ volatile (
      "cache 0x1b, 0(%0)  \n"
      "sync               \n"
      : : "r" (mem) : "memory"
    );
    return;
  } else if (!size) {
    free(_base);
  }
  *mem = nullptr;
  return;
}

inline void meHalt() {
  asm volatile(".word 0x70000000");
}

static inline void meDCacheWritebackInvalidAll() {
  asm volatile ("sync");
  for (int i = 0; i < 8192; i += 64) {
    asm("cache 0x14, 0(%0)" :: "r"(i));
    asm("cache 0x14, 0(%0)" :: "r"(i));
  }
  asm volatile ("sync");
}

static inline void meDCacheWritebackInvalidRange(const u32 addr, const u32 size) {
  asm volatile("sync");
  for (volatile u32 i = addr; i < addr + size; i += 64) {
    asm volatile(
      "cache 0x1b, 0(%0)\n"
      "cache 0x1b, 0(%0)\n"
      :: "r"(i)
    );
  }
  asm volatile("sync");
}

static inline void meDCacheInvalidRange(const u32 addr, const u32 size) {
  asm volatile("sync");
  for (volatile u32 i = addr; i < addr + size; i += 64) {
    asm volatile(
      "cache 0x19, 0(%0)\n"
      "cache 0x19, 0(%0)\n"
      :: "r"(i)
    );
  }
  asm volatile("sync");
}

static inline void meDCacheWritebackRange(const u32 addr, const u32 size) {
  asm volatile("sync");
  for (volatile u32 i = addr; i < addr + size; i += 64) {
    asm volatile(
      "cache 0x1a, 0(%0)\n"
      "cache 0x1a, 0(%0)\n"
      :: "r"(i)
    );
  }
  asm volatile("sync");
}

static inline u32 getlocalUID() {
  u32 unique;
  asm volatile(
    "sync\n"
    "mfc0 %0, $22\n"
    "sync"
    : "=r" (unique)
  );
  return (unique + 1) & 3;
  // reads processor id from cp0 register $22
  // 0 = main cpu
  // 1 = me
}
