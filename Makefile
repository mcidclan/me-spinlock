BINOUT = ./bin/
PATHSRC = ./
PATHOBJS = $(BINOUT)
TARGET = $(BINOUT)mls

CC = psp-gcc
CXX = psp-gcc
AS = psp-as

CPP_FILES = $(wildcard $(PATHSRC)*.cpp)
S_FILES = $(wildcard $(PATHSRC)*.S)
PATHFILES = $(CPP_FILES) $(S_FILES)

OBJS = $(notdir $(patsubst %.cpp, %.o, $(patsubst %.S, %.o, $(PATHFILES))))
OBJS := $(sort $(OBJS:%.o=$(PATHOBJS)%.o))

PSPSDK = $(shell psp-config --pspsdk-path)

CFLAGS = -I. -I$(PSPSDK)/include -I/usr/local/pspdev/psp/sdk/include -Ofast -G0 -Wall -fno-pic \
         -I./kernel/src -Wextra -Werror -D_PSP_FW_VERSION=360
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -std=c++11
ASFLAGS = $(CFLAGS) -x assembler-with-cpp
LDFLAGS = -L. -L$(PSPSDK)/lib -L/usr/local/pspdev/psp/sdk/lib \
          -lpsppower -lpspdebug -lpspdisplay -lpspge -lpspctrl \
          -lpspsdk -lc -lpspnet -lpspnet_inet -lpspnet_apctl \
          -lpspnet_resolver -lpsputility -lpspuser -lpspkernel

PSP_EBOOT_SFO = $(BINOUT)PARAM.SFO
PSP_EBOOT_TITLE = Me Lock Stuff

all: $(TARGET).elf $(BINOUT)EBOOT.PBP

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(PATHOBJS)%.o: $(PATHSRC)%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(PATHOBJS)%.o: $(PATHSRC)%.S
	$(CC) $(ASFLAGS) -c $< -o $@

$(BINOUT)EBOOT.PBP: $(TARGET).elf
	psp-fixup-imports $(TARGET).elf
	mksfo "$(PSP_EBOOT_TITLE)" $(PSP_EBOOT_SFO)
	psp-strip $(TARGET).elf -o $(TARGET)_strip.elf
	pack-pbp $(BINOUT)EBOOT.PBP $(PSP_EBOOT_SFO) NULL \
	NULL NULL NULL \
	NULL $(TARGET)_strip.elf NULL
	rm -f $(TARGET)_strip.elf

clean:
	rm -f $(TARGET).elf $(OBJS) $(PSP_EBOOT_SFO) $(BINOUT)EBOOT.PBP
