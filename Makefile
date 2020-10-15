all: gamepad hotplug

gamepad: gamepad.c
	gcc gamepad.c -o gamepad -lusb-1.0 -Wall

hotplug: hotplug.c
	gcc hotplug.c -o hotplug -lusb-1.0 -Wall

clean:
	rm -f gamepad hotplug
