#include "kcall.h"
#include <pspsdk.h>

/*
 * NOTE: psp-fixup-imports is not included in Makefile for this minimal PRX.
 * If adding syscalls/library functions, enable it to avoid any issues!
 */

PSP_MODULE_INFO("kcall", 0x1006, 1, 1);
PSP_NO_CREATE_MAIN_THREAD();

int kcall(FCall const f) {
  return f();
}

int module_start(SceSize args, void *argp){
  return 0;
}

int module_stop() {
  return 0;
}
