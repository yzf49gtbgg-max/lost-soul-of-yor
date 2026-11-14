TARGET = lost_soul_of_yor
OBJS = main.o
INCDIR =
CFLAGS = -O2 -G0 -Wall -std=gnu99
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBDIR =
LDFLAGS =
LIBS = -lpspgu -lpspgum -lpspdisplay -lpspge -lpspctrl -lpspaudio -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Lost Soul of Yor

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
