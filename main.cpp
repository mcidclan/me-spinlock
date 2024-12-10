#include "main.h"

PSP_MODULE_INFO("mls", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

volatile u32* mem = nullptr;

#define mutex vrg(0xbc100048) // uncached kernel mutex

// reads processor id from cp0 register $22:
// 0 = main cpu
// 1 = me
#define getCpuId(var) asm volatile( \
  "sync\n" \
  "mfc0 %0, $22\n" \
  "sync" \
  : "=r" (var) \
)

int unlock() {
  asm("sync");
  mutex = 0;
  asm("sync");
  return 0;
}

int lock() {
  volatile u32 unique;
  getCpuId(unique); // get cpu id as a unique id
  unique = (unique & 1) + 1;
  volatile u32 delay = 1;
  do {
    if (mutex == 0) { // check if the mutex is free
      mutex = unique;
      asm("sync");
      if (mutex == unique) {
        return 0; // lock acquired
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
  return -1;
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
static int meLoop() {
  // Wait until mem is ready
  while (!mem) {
    meDCacheWritebackInvalidAll();
  }
  do {
    lock();
    mem[0]++;
    if (mem[1] > 100) {
      mem[1] = 0;
    }
    unlock();
    meDCacheWritebackInvalidAll();
  } while(!_meExit);
  return _meExit;
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section"), noinline, aligned(4)))
void meHandler() {
  vrg(0xbc100050) = 0x7f;       // enable clocks: ME, AW bus RegA, RegB & Edram, DMACPlus, DMAC
  vrg(0xbc100004) = 0xffffffff; // clear NMI
  vrg(0xbc100040) = 1;          // allow 32MB ram
  asm("sync");
  ((FCall)_meLoop)();
}

static int initMe() {
  memcpy((void *)0xbfc00040, (void*)&__start__me_section, me_section_size);
  _meLoop = (u32)&meLoop;
  meDCacheWritebackInvalidAll();
  // reset and start me
  vrg(0xBC10004C) = 0b100;
  asm("sync");
  vrg(0xBC10004C) = 0x0;
  asm("sync");
  return 0;
}

int main() {
  scePowerSetClockFrequency(333, 333, 166);
  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    sceKernelExitGame();
    return 0;
  }

  // Init me before user mem initialisation
  kcall(&initMe);
  
  mem = (u32*)memalign(16, sizeof(u32) * 4);
  memset((void*)mem, 0, sizeof(u32) * 4);
  sceKernelDcacheWritebackInvalidateAll();

  pspDebugScreenInit();
  
  SceCtrlData ctl;
  bool hello = false;
  do {
    if(!kcall(&tryLock)) {
      hello = false;
      if (mem[1] > 50) {
        hello = true;
      }
      mem[2]++;
      mem[1]++;
      sceKernelDelayThread(100000);
      kcall(&unlock);
    }
    sceKernelDcacheWritebackInvalidateAll(); // push cache to mem & invalidate (next read will fill)
    sceCtrlPeekBufferPositive(&ctl, 1);
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("                                                   ");
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("Counters %u; %u; %u;", mem[0], mem[1], mem[2]);
    pspDebugScreenSetXY(0, 2);
    if (hello) {
      pspDebugScreenPrintf("Hello!");
    } else {
      pspDebugScreenPrintf("xxxxxx");
    }
    sceDisplayWaitVblankStart();
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  // exit me
  meExit();
  free((void*)mem);
  
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(1000000);
  sceKernelExitGame();
  return 0;
}
