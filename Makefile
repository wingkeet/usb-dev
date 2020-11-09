CC=gcc
CFLAGS=-Wall
LIBS=-lusb-1.0
TARGETS=gamepad hotplug

all: $(TARGETS)

gamepad: gamepad.c usbcommon.c usbcommon.h
	$(CC) $< usbcommon.c -o $@ $(CFLAGS) $(LIBS)

hotplug: hotplug.c usbcommon.c usbcommon.h
	$(CC) $< usbcommon.c -o $@ $(CFLAGS) $(LIBS)

clean:
	$(RM) $(TARGETS)
