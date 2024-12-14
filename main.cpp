#include "main.h"

PSP_MODULE_INFO("mls", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

// make sure to align cached shared variables to 64
volatile u32* mem __attribute__((aligned(64))) = nullptr;
volatile bool meStart __attribute__((aligned(64))) = false;

#define mutex vrg(0xbc100048) // non-cached kernel mutex

// kernel function to unlock the mutex
__attribute__((noinline, aligned(4)))
int unlock() {
  // if acquired, briefly holds the lock with a pipeline delay,
  // allowing cache operations to complete, could be useful
  asm volatile("nop; nop; nop; nop; nop; nop; nop;");
  mutex = 0;
  asm volatile("sync");
  // provides opportunities for others with a pipeline delay
  asm volatile("nop; nop; nop; nop; nop; nop; nop;");
  return 0;
}

// kernel function that waits and attempts to lock and acquire the mutex
__attribute__((noinline, aligned(4)))
int lock() {
  const u32 unique = getlocalUID();
  do {
    mutex = unique; // the main CPU can affect only bit[0] (0b01), while the Me can only affect bit[1] (0b10)
    asm volatile("sync");
    if (!(((mutex & 3) ^ unique))) { // if mutex == 0b11, there is a conflict, and it can't be acquired
      return 0; // lock acquired
    }
    // gives a breath with a pipeline delay (7 stages)
    asm volatile("nop; nop; nop; nop; nop; nop; nop;");
  } while (1);
  return 1;
}

// kernel function to attempt locking and acquiring the mutex
__attribute__((noinline, aligned(4)))
int tryLock() {
  const u32 unique = getlocalUID();
  mutex = unique;
  asm volatile("sync");
  if (!(((mutex & 3) ^ unique))) {
    return 0; // lock acquired
  }
  asm volatile("sync"); // make sure to be sync before leaving kernel mode
  return 1;
}

// note:
// it appears that the main CPU can read the mutex and only set bit[0],
// while the Me can read the mutex and only set bit[1]

__attribute__((noinline, aligned(4)))
static int meLoop() {
  // read meStart using the uncached mask, wait until the signal is received
  // from the main CPU and ensure that the shared mem is ready
  do {
    meDCacheWritebackInvalidAll();
  } while(!vrg(0x40000000 | (u32)&meStart) || !mem);
  
  do {
    // invalidate cache, forcing next read to fetch from memory
    meDCacheInvalidRange((u32)mem, sizeof(u32)*4);

    lock();
    mem[0]++;
    if (mem[1] > 100) {
      mem[1] = 0;
    }
    unlock();
    
    // write modified cache data back to memory
    meDCacheWritebackRange((u32)mem, sizeof(u32)*4);
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
  asm volatile("sync");
  ((FCall)_meLoop)();
}

static int initMe() {
  memcpy((void *)0xbfc00040, (void*)&__start__me_section, me_section_size);
  // Call meLoop, it is safer to invoke it with a kernel mask when using interrupts or spinlocks
  _meLoop = 0x80000000 | (u32)&meLoop;
  meDCacheWritebackInvalidAll();
  // reset and start me
  vrg(0xBC10004C) = 0b100;
  asm volatile("sync");
  vrg(0xBC10004C) = 0x0;
  asm volatile("sync");
  return 0;
}

// function used to hold the mutex in the main loop as a proof
bool releaseMutex() {
  static u32 hold = 100;
  if (hold-- > 0) {
    return false;
  }
  hold = 100;
  return true;
}

int main() {
  scePowerSetClockFrequency(333, 333, 166);
  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    sceKernelExitGame();
    return 0;
  }
  
  // Init me before user mem initialisation
  kcall(&initMe);
  
  // to use DCWBInv Range, 64-byte alignment is required (not necessary while using DCWBInv All)
  mem = (u32*)memalign(64, (sizeof(u32) * 4 + 63) & ~63);
  memset((void*)mem, 0, sizeof(u32) * 4);
  sceKernelDcacheWritebackAll();

  pspDebugScreenInit();
  
  SceCtrlData ctl;
  u32 counter = 0;
  bool switchMessage = false;

  // start the process on the Me just before the main loop
  vrg(0x40000000 | (u32)&meStart) = true;
  do {

    // invalidate cache, forcing next read to fetch from memory
    sceKernelDcacheInvalidateRange((void*)mem, sizeof(u32) * 4);
    
    // functions that use spinlock, seem to need to be invoked with a kernel mask
    if(!kcall((FCall)(0x80000000 | (u32)&tryLock))) {
      switchMessage = false;
      if (mem[1] > 50) {
        switchMessage = true;
      }
      mem[2]++;
      mem[1]++;
      // sceKernelDelayThread(10000);
      
      // proof to visualize the release of the mutex and its effect on the counter (mem[0]) running on the Me
      if (releaseMutex()) {
        kcall((FCall)(0x80000000 | (u32)&unlock));
      }
    }
    
    // push cache to memory and invalidate it, refill cache during the next access
    sceKernelDcacheWritebackInvalidateRange((void*)mem, sizeof(u32) * 4);

    sceCtrlPeekBufferPositive(&ctl, 1);
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("                                                   ");
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("Counters %u; %u; %u; %u", mem[0], mem[1], mem[2], counter++);
    pspDebugScreenSetXY(0, 2);
    if (switchMessage) {
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
