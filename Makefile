all: gamepad hotplug

gamepad: gamepad.c usbcommon.c usbcommon.h
	gcc gamepad.c usbcommon.c -o gamepad -lusb-1.0 -Wall

hotplug: hotplug.c usbcommon.c usbcommon.h
	gcc hotplug.c usbcommon.c -o hotplug -lusb-1.0 -Wall

clean:
	rm -f gamepad hotplug
