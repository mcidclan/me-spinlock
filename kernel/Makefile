BINOUT = ./bin/
PATHSRC = ./src/
TARGET = $(BINOUT)ms_klib

CC = psp-gcc
LD = psp-gcc
FIXUP = psp-fixup-imports

PSPSDK = $(shell psp-config --pspsdk-path)
PSPDEV = $(shell psp-config --pspdev-path)
INCDIR = -I. -I$(PSPDEV)/psp/include -I$(PSPSDK)/include
LIBDIR = -L. -L$(PSPDEV)/psp/lib -L$(PSPSDK)/lib

CFLAGS = -fsingle-precision-constant -Ofast -G0 -Wall $(INCDIR)
LDFLAGS = $(LIBDIR) -Wl,-q,-T$(PSPSDK)/lib/linkfile.prx -nostartfiles -Wl,-zmax-page-size=128

LIBS = -lpspkernel -lpsppower

release: all
	psp-build-exports -s $(PATHSRC)exports.exp && \
	mv ms_klib.S ../ && \
	mv bin/ms_klib.prx ../bin
  
C_FILES = $(shell ls $(PATHSRC)*.c)
OBJS = $(notdir $(patsubst %.c, %.o, $(C_FILES)))
OBJS := $(sort $(OBJS:%.o=$(BINOUT)%.o))
OBJS += $(BINOUT)exports.o

FINAL_TARGET = $(TARGET).prx

all: $(FINAL_TARGET)

$(TARGET).elf: $(OBJS)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@
	$(FIXUP) $@

%.prx: %.elf
	psp-prxgen $< $@

$(BINOUT)%.o: $(PATHSRC)%.c
	$(CC) -o $@ -c $< $(CFLAGS)

%.c: %.exp
	psp-build-exports -b $< > $@

clean:
	-rm -f $(FINAL_TARGET) $(TARGET).elf $(OBJS)

rebuild: clean all