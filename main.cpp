#include "main.h"

PSP_MODULE_INFO("mls", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

// make sure to align cached shared variables to 64
volatile u32* mem    __attribute__((aligned(64))) = nullptr;
volatile u32* shared __attribute__((aligned(64))) = nullptr;
#define meStart           (shared[0])
#define meExit            (shared[1])

 // define the non-cached kernel mutex
#define mutex hw(0xbc100048)

// kernel function to unlock the mutex
__attribute__((noinline, aligned(4)))
int unlock() {
  // if acquired, briefly holds the lock with a pipeline delay,
  // allowing cache operations to complete, could be useful
  delayPipeline();
  mutex = 0;
  asm volatile("sync");
  // provides opportunities for others with a pipeline delay
  delayPipeline();
  return 0;
}

// kernel function that waits and attempts to lock and acquire the mutex
__attribute__((noinline, aligned(4)))
int lock() {
  const u32 unique = getlocalUID();
  do {
    mutex = unique; // the main CPU can affect only bit[0] (0b01), while the Me can only affect bit[1] (0b10)
    asm volatile("sync");
    if (!(((mutex & 3) ^ unique))) { // see note
      return 0; // lock acquired
    }
    // gives a breath with a pipeline delay (7 stages)
    delayPipeline();
  } while (1);
  return 1;
}

// kernel function to attempt locking and acquiring the mutex
__attribute__((noinline, aligned(4)))
int tryLock() {
  const u32 unique = getlocalUID();
  mutex = unique;
  asm volatile("sync");
  if (!(((mutex & 3) ^ unique))) { // see note
    return 0; // lock acquired
  }
  asm volatile("sync"); // make sure to be sync before leaving kernel mode
  return 1;
}

// note:
// it appears that the main CPU can read the mutex and only set bit[0],
// while the Me can read the mutex and only set bit[1]
//
// mutex    unique
// 11  xor  01 =>   not 10 = 0
// 11  xor  10 =>   not 01 = 0
// 10  xor  01 =>   not 11 = 0
// 10  xor  10 =>   not 00 = 1
// 01  xor  01 =>   not 00 = 1
// 01  xor  10 =>   not 11 = 0

__attribute__((noinline, aligned(4)))
static void meLoop() {
  // ensure that the shared mem is ready
  do {
    meDCacheWritebackInvalidAll();
  } while(!mem || !shared);

  // wait until the start signal is receive from the main CPU
  do {
    delayPipeline();
  } while (!meStart);
  
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
  } while (meExit == 0);
  
  meExit = 2;
  meHalt();
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section"), noinline, aligned(4)))
void meHandler() {
  hw(0xbc100040) = 1;          // allow 32MB ram
  hw(0xbc100050) = 0x06;       // enable AW RegA & RegB bus clocks
  hw(0xbc100004) = 0xffffffff; // clear NMI
  asm("sync");
  
  asm volatile(
    "li          $k0, 0x30000000     \n"
    "mtc0        $k0, $12            \n"
    "sync                            \n"
    "la          $k0, %0             \n"
    "li          $k1, 0x80000000     \n"
    "or          $k0, $k0, $k1       \n"
    "cache       0x8, 0($k0)         \n"
    "sync                            \n"
    "jr          $k0                 \n"
    "nop\n"
    :
    : "i" (meLoop)
    : "k0"
  );
}

static int initMe() {
  #define me_section_size (&__stop__me_section - &__start__me_section)
  memcpy((void *)ME_HANDLER_BASE, (void*)&__start__me_section, me_section_size);  
  sceKernelDcacheWritebackInvalidateAll();
  // reset and start me
  hw(0xBC10004C) = 0x04;
  hw(0xBC10004C) = 0x00;
  asm volatile("sync");
  return 0;
}

// function used to hold the mutex in the main loop as a proof
bool holdMutex() {
  static u32 hold = 100;
  if (hold-- > 0) {
    return false;
  }
  hold = 100;
  return true;
}

void exitSample(const char* const str) {
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf(str);
  sceKernelDelayThread(500000);
  sceKernelExitGame();
}

static void meWaitExit() {
  // make sure the mutex is unlocked
  kcall((FCall)(CACHED_KERNEL_MASK | (u32)&unlock));
  // wait the me to exit
  meExit = 1;
  do {
    asm volatile("sync");
  } while (meExit < 2);
}

int main() {
  scePowerSetClockFrequency(333, 333, 166);
  pspDebugScreenInit();

  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    exitSample("Can't load the PRX, exiting...");
    return 0;
  }
  
  // allocate shared variables
  meGetUncached32(&shared, 2);
  
  // init me before user mem initialisation
  kcall(&initMe);
  
  // to use DCWBInv Range, 64-byte alignment is required (not necessary while using DCWBInv All)
  mem = (u32*)memalign(64, (sizeof(u32) * 4 + 63) & ~63);
  memset((void*)mem, 0, sizeof(u32) * 4);
  sceKernelDcacheWritebackAll();
  
  SceCtrlData ctl;
  u32 counter = 0;
  bool switchMessage = false;

  // start the process on the Me just before the main loop
  meStart = true;
  do {

    // invalidate cache, forcing next read to fetch from memory
    sceKernelDcacheInvalidateRange((void*)mem, sizeof(u32) * 4);
    
    // functions that use spinlock, seem to need to be invoked with a kernel mask
    if(!kcall((FCall)(CACHED_KERNEL_MASK | (u32)&tryLock))) {
      switchMessage = false;
      if (mem[1] > 50) {
        switchMessage = true;
      }
      mem[2]++;
      mem[1]++;
      // sceKernelDelayThread(10000);
      
      // proof to visualize the release of the mutex and its effect on the counter (mem[0]) running on the Me
      if (!holdMutex()) {
        kcall((FCall)(CACHED_KERNEL_MASK | (u32)&unlock));
      }
    }
    
    // push cache to memory and invalidate it, refill cache during the next access
    sceKernelDcacheWritebackInvalidateRange((void*)mem, sizeof(u32) * 4);

    sceCtrlPeekBufferPositive(&ctl, 1);
    
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("Sc counter %u   ", counter++);
    pspDebugScreenSetXY(0, 2);
    pspDebugScreenPrintf("Me counter %u   ", mem[0]);
    pspDebugScreenSetXY(0, 3);
    pspDebugScreenPrintf("Shared counters %u, %u  ", mem[1], mem[2]);
    
    pspDebugScreenSetXY(0, 4);
    if (switchMessage) {
      pspDebugScreenPrintf("Hello!");
    } else {
      pspDebugScreenPrintf("xxxxxx");
    }
    sceDisplayWaitVblankStart();
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  // exit me, clean memory
  meWaitExit();
  meGetUncached32(&shared, 0);
  free((void*)mem);
  
  exitSample("Exiting...");
  return 0;
}
