all: gamepad hotplug

gamepad: gamepad.c common.c common.h
	gcc gamepad.c common.c -o gamepad -lusb-1.0 -Wall

hotplug: hotplug.c common.c common.h
	gcc hotplug.c common.c -o hotplug -lusb-1.0 -Wall

clean:
	rm -f gamepad hotplug
