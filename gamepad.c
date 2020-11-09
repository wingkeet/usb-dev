#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include "common.h"

static void do_sync_interrupt_transfer(libusb_device_handle *devh)
{
    uint8_t data[64]; // data buffer
    int actual; // how many bytes were actually transferred
    int rc;

    while (1) {
        // Block forever until an event is received
        rc = libusb_interrupt_transfer(devh, (2 | LIBUSB_ENDPOINT_IN),
            data, sizeof(data), &actual, 0);
        if (rc == LIBUSB_SUCCESS) {
            printf("Read successful! %d bytes: ", actual);
            printhex(actual, data);
            putchar('\n');

            if (actual == 18 && data[0] == 0x20) {
                // We received button data
                struct gamepad_t gamepad;
                data_to_gamepad(data, &gamepad);
                print_gamepad(&gamepad);

                if (gamepad.a) {
                    rumble(devh, 0x20, 0x20);
                }
                if (gamepad.x) {
                    break;
                }
            }
        }
        else {
            fputs("Read error\n", stderr);
        }
    }
}

int main(void)
{
    libusb_device_handle *devh = NULL; // device handle
    int rc; // return code

    print_libusb_version();

    rc = libusb_init(NULL); // initialize libusb
    if (rc != LIBUSB_SUCCESS) {
        fprintf(stderr, "Init error %d\n", rc);
        return EXIT_FAILURE;
    }
    libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);

    // These are the vendor_id and product_id I found for my usb device
    devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
    if (devh == NULL) {
        fputs("Cannot open device\n", stderr);
        libusb_exit(NULL);
        return EXIT_FAILURE;
    }
    puts("Device opened");

    print_port_path(devh);

    // Let libusb handle detaching/attaching kernel driver for us
    rc = libusb_set_auto_detach_kernel_driver(devh, 1);
    printf("libusb_set_auto_detach_kernel_driver() returned %d\n", rc);

    // Claim interface 0 (the first) of device (mine had just 1)
    rc = libusb_claim_interface(devh, 0);
    if (rc != LIBUSB_SUCCESS) {
        fputs("Cannot claim interface\n", stderr);
        libusb_close(devh);
        libusb_exit(NULL);
        return EXIT_FAILURE;
    }
    puts("Claimed interface");

    rc = init_device(devh);

    // Read forever until button X is pressed
    do_sync_interrupt_transfer(devh);

    // Shutting down from here onwards
    rc = libusb_release_interface(devh, 0); // release the claimed interface
    rc == LIBUSB_SUCCESS ? puts("Released interface") :
        fputs("Cannot release interface\n", stderr);
    libusb_close(devh); // close the device we opened
    libusb_exit(NULL); // deinitialize libusb
    return EXIT_SUCCESS;
}
