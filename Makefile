CC=gcc
CFLAGS=-Wall -Werror -Wextra -pedantic
LDLIBS=-lusb-1.0
TARGETS=gamepad hotplug

all: $(TARGETS)

gamepad: gamepad.c usbcommon.c usbcommon.h
	$(CC) $< usbcommon.c -o $@ $(CFLAGS) $(LDLIBS)

hotplug: hotplug.c usbcommon.c usbcommon.h
	$(CC) $< usbcommon.c -o $@ $(CFLAGS) $(LDLIBS)

clean:
	$(RM) $(TARGETS)
