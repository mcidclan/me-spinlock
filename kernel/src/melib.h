#ifndef _ME_LIB_H
#define _ME_LIB_H
#include <pspkernel.h>
#define regPtr       volatile u32*
#define reg(addr)    (*((regPtr)(addr)))

#ifdef __cplusplus
extern "C" {
#endif
  typedef int (*MeFunc)(void);
  
  typedef struct MeCom {
    u32* data;
    MeFunc func;
  } MeCom;
  
  void me_init(MeCom* const func);
  int kernel_callback(MeFunc const func);

  inline void _dcache_writeback_invalid_all() {
    asm("sync");
    for (int i = 0; i < 8192; i += 64) {
    __builtin_allegrex_cache(0x14, i);
    __builtin_allegrex_cache(0x14, i);
    }
    asm("sync");
  }
  
  inline void _memcpy(void *d,  void *s, int size) {
   for (int i = 0; i < size; i++) {
     ((u8 *)d)[i] = ((u8 *)s)[i];
   }
  }
#ifdef __cplusplus
}
#endif
#endif
