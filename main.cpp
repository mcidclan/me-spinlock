#include "kernel/src/melib.h"
#include <pspsdk.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <malloc.h>
#include <cstring>

PSP_MODULE_INFO("mls", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

int* mem = nullptr;
bool stop = false;

#define mutex reg(0xbc100048) // uncached kernel mutex
#define getCpuId(var) asm volatile( \
  "sync\n" \
  "mfc0 %0, $22\n" \
  "sync" \
  : "=r" (var) \
)

// reads processor id from cp0 register $22:
// 0 = main cpu
// 1 = me

int unlock() {
  asm("sync");
  mutex = 0;
  asm("sync");
  return 0;
}

void lock() {
  volatile u32 unique;
  getCpuId(unique); // get cpu id as a unique id
  unique = (unique & 1) + 1;
  volatile u32 delay = 1;
  do {
    if (mutex == 0) { // check if the mutex is free
      mutex = unique;
      asm("sync");
      if (mutex == unique) {
        return; // lock acquired
      }
    }
    asm("sync");
    // exponential backoff
    for (volatile u32 i = 0; i < delay; i++) {
      asm("nop; nop; nop; nop; nop; nop; nop;"); // pipeline delay (7 stages)
    }
    if (delay < 1024) {
      delay *= 2;
    }
  } while (1);
}


int tryLock() {
  volatile u32 unique;
  getCpuId(unique);
  unique = (unique & 1) + 1;
  asm("sync");
  if (mutex == 0) {
    mutex = unique;
    asm("sync");
    if (mutex == unique) {
      return 0;
    }
  }
  return 1;
}

__attribute__((noinline, aligned(4)))
int meLoop() {
  lock();
  mem[0]++;
  if (mem[1] > 100) {
    mem[1] = 0;
  }
  unlock();
  return !stop;
}

int main(int argc, char **argv) {
  scePowerSetClockFrequency(333, 333, 166);
  
  mem = (int*)memalign(16, sizeof(int) * 4);
  memset((void*)mem, 0, sizeof(int) * 4);
  
  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/mls_klib.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    sceKernelExitGame();
    return 0;
  }
  
  MeCom meCom = {
    nullptr,
    meLoop,
  };
  me_init(&meCom);

  pspDebugScreenInit();
  SceCtrlData ctl;
  bool hello = false;
  do {
    if(!kernel_callback(&tryLock)) {
      hello = false;
      if (mem[1] > 50) {
        hello = true;
      }
      mem[2]++;
      mem[1]++;
      sceKernelDelayThread(100000);
      kernel_callback(&unlock);
    }
    sceKernelDcacheWritebackInvalidateAll(); // push cache to mem & invalidate (next read will fill)
    sceCtrlPeekBufferPositive(&ctl, 1);
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("                                                   ");
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("Counters %i; %i; %i;", mem[0], mem[1], mem[2]);
    pspDebugScreenSetXY(0, 2);
    if (hello) {
      pspDebugScreenPrintf("Hello!");
    } else {
      pspDebugScreenPrintf("xxxxxx");
    }
    sceDisplayWaitVblankStart();
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  stop = true;
  sceKernelDcacheWritebackInvalidateAll();
  
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(1000000);
  free(mem);
  sceKernelExitGame();
  return 0;
}
