#include "melib.h"
#include <pspsdk.h>

PSP_MODULE_INFO("mls_klib", 0x1006, 1, 1);
PSP_NO_CREATE_MAIN_THREAD();

#define ME_HANDLER_BASE 0xbfc00040
#define ME_SECTION_SIZE ((unsigned int)(&__stop__me_section - &__start__me_section))
#define ME_SECTION_END_ADDR (ME_SECTION_SIZE + ME_HANDLER_BASE)

extern char __start__me_section;
extern char __stop__me_section;

__attribute__((section("_me_section")))
void meHandler() {
  reg(0xbc100050) = 0b100; // we need this
  reg(0xbc100004) = 0xFFFFFFFF; // clear all interrupts, just usefull
  reg(0xbc100040) = 0x02; // allow 64MB ram, probably better (default is 16MB)
  asm("sync");
  
  volatile MeCom* const meCom = (volatile MeCom* const)(ME_SECTION_END_ADDR);
  while (1) {
    _dcache_writeback_invalid_all();
    if (!meCom->func()) {
      break;
    }
  }
}

void me_init(MeCom* const meCom){
  volatile MeCom* const _meCom = (volatile MeCom* const)(ME_SECTION_END_ADDR);
  _memcpy((void*)_meCom, meCom, sizeof(MeCom));
  _memcpy((void *)ME_HANDLER_BASE, &__start__me_section, ME_SECTION_SIZE);
  sceKernelDcacheWritebackInvalidateAll();
  reg(0xBC10004C) = 0b0100; // reset enable, just the me
  asm("sync");
  reg(0xBC10004C) = 0b0; // disable reset to start the me
  asm("sync");
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
