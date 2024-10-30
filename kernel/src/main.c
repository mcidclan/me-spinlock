#include "melib.h"
#include <pspsdk.h>

PSP_MODULE_INFO("ms_klib", 0x1006, 1, 1);
PSP_NO_CREATE_MAIN_THREAD();

#define ME_HANDLER_BASE 0xbfc00040
#define ME_SECTION_SIZE ((unsigned int)(&__stop__me_section - &__start__me_section))
#define ME_SECTION_END_ADDR (ME_SECTION_SIZE + ME_HANDLER_BASE)

extern char __start__me_section;
extern char __stop__me_section;

__attribute__((section("_me_section")))
void meHandler() {
  reg(0xbc100050) = 0b100; // we need this
  _dcache_writeback_invalid_all();
  
  reg(0xbc100004) = 0xFFFFFFFF; // clear all interrupts
  reg(0xbc100040) = 0x02; // allow 64MB ram 
  // disable memory Protection
  regPtr ptr = (regPtr)0xBC000000;
  while (ptr <= (regPtr)0xBC00000C) {
    *(ptr++) = 0xFFFFFFFF;
  }
  _dcache_writeback_invalid_all();
  
  volatile MeCom* const meCom = (volatile MeCom* const)(ME_SECTION_END_ADDR);
  while (1) {
    _dcache_writeback_invalid_all();
    if (!meCom->func()) {
      break;
    }
  }
  reg(0xBC100050) = 0b0;
  _dcache_writeback_invalid_all();
}

void me_init(MeCom* const meCom){
  volatile MeCom* const _meCom = (volatile MeCom* const)(ME_SECTION_END_ADDR);
  _memcpy((void*)_meCom, meCom, sizeof(MeCom));
  _memcpy((void *)ME_HANDLER_BASE, &__start__me_section, ME_SECTION_SIZE);
  reg(0xBC10004C) = 0b0100; // just the me
  sceKernelDcacheWritebackInvalidateAll();
  reg(0xBC10004C) = 0b0;
  sceKernelDcacheWritebackInvalidateAll();
}

int kernel_callback(MeFunc const func) {
  return func();
}

int module_start(SceSize args, void *argp){
  return 0;
}

int module_stop() {
  return 0;
}
