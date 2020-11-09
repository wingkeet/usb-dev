#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <libusb-1.0/libusb.h>
#include "common.h"

enum {
    EVENT_QUIT               = 1,
    EVENT_DEVICE_ARRIVED     = 2,
    EVENT_DEVICE_LEFT        = 3,
    EVENT_TRANSFER_COMPLETED = 4,
    EVENT_BUTTON_A_PRESSED   = 5,
    EVENT_BUTTON_X_PRESSED   = 6
};

static libusb_device_handle *devh = NULL;
static struct libusb_transfer *transfer = NULL;
static libusb_hotplug_callback_handle hotplug_callback_handle = 0;
static int completed = 0;

// http://libusb.sourceforge.net/api-1.0/libusb_hotplug.html
static int LIBUSB_CALL hotplug_callback(struct libusb_context *ctx,
    struct libusb_device *dev, libusb_hotplug_event event, void *user_data)
{
    struct libusb_device_descriptor desc;
    int rc;
    int * const completed = user_data;

    libusb_get_device_descriptor(dev, &desc);

    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        rc = libusb_open(dev, &devh);
        if (rc != LIBUSB_SUCCESS) {
            fputs("Could not open USB device\n", stderr);
        }
        *completed = EVENT_DEVICE_ARRIVED;
    }
    else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        if (devh != NULL) {
            libusb_close(devh);
            devh = NULL;
        }
        *completed = EVENT_DEVICE_LEFT;
    }
    else {
        fprintf(stderr, "Unhandled event %d\n", event);
    }

    return 0; // rearm this callback
}

// http://libusb.sourceforge.net/api-1.0/group__libusb__asyncio.html
static void LIBUSB_CALL transfer_callback(struct libusb_transfer *transfer)
{
    int * const completed = transfer->user_data;

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        printf("Read successful! %d bytes: ", transfer->actual_length);
        printhex(transfer->actual_length, transfer->buffer);
        putchar('\n');

        *completed = EVENT_TRANSFER_COMPLETED;

        if (transfer->actual_length == 18 && transfer->buffer[0] == 0x20) {
            // We received button data
            struct gamepad_t gamepad;
            data_to_gamepad(transfer->buffer, &gamepad);

            if (gamepad.a) {
                *completed = EVENT_BUTTON_A_PRESSED;
            }
            else if (gamepad.x) {
                *completed = EVENT_BUTTON_X_PRESSED;
            }
        }
    }
    else if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
        // User pressed CTRL+C
        *completed = EVENT_QUIT;
    }
    else {
        fprintf(stderr, "Transfer failed with status = %s\n",
            libusb_error_name(transfer->status));
    }
}

static void signal_handler(int signum)
{
    if (signum == SIGINT) {
        if (devh != NULL) {
            libusb_cancel_transfer(transfer);
        }
        else {
            completed = EVENT_QUIT;
            libusb_interrupt_event_handler(NULL);
        }
    }
}

static int register_signal_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    return sigaction(SIGINT, &sa, NULL);
}

// Error handling code has been omitted for clarity
int main(void)
{
    uint8_t data[64]; // data buffer
    int rc; // return code

    if (register_signal_handler() == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    print_libusb_version();

    rc = libusb_init(NULL);
    libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);

    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        fputs("Hotplug is not supported\n", stderr);
        libusb_exit(NULL);
        return EXIT_FAILURE;
    }

    rc = libusb_hotplug_register_callback(NULL,
        LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
        0, VENDOR_ID, PRODUCT_ID,
        LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, &completed,
        &hotplug_callback_handle);
    if (rc != LIBUSB_SUCCESS) {
        fputs("Error registering hotplug callback\n", stderr);
        libusb_exit(NULL);
        return EXIT_FAILURE;
    }

    // We use a single transfer structure throughout the whole program
    transfer = libusb_alloc_transfer(0);

    // Open device and submit an asynchronous transfer
    devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
    if (devh != NULL) {
        puts("Device opened");
        print_port_path(devh);
        rc = libusb_set_auto_detach_kernel_driver(devh, 1);
        rc = libusb_claim_interface(devh, 0);
        rc = init_device(devh);
        libusb_fill_interrupt_transfer(transfer, devh, (2 | LIBUSB_ENDPOINT_IN),
            data, sizeof(data), transfer_callback, &completed, 0);
        rc = libusb_submit_transfer(transfer);
    }

    // Read forever until button X is pressed
    while (1) {
        completed = 0;

        // Block forever until an event is received
        rc = libusb_handle_events_completed(NULL, &completed);
        //nanosleep(&(struct timespec){0, 100000000UL}, NULL); // 100 ms

        if (completed == EVENT_QUIT) {
            putchar('\n');
            break;
        }
        else if (completed == EVENT_DEVICE_ARRIVED) {
            puts("Device arrived");
            print_port_path(devh);
            rc = libusb_set_auto_detach_kernel_driver(devh, 1);
            rc = libusb_claim_interface(devh, 0);
            rc = init_device(devh);
            libusb_fill_interrupt_transfer(transfer, devh, (2 | LIBUSB_ENDPOINT_IN),
                data, sizeof(data), transfer_callback, &completed, 0);
            rc = libusb_submit_transfer(transfer);
        }
        else if (completed == EVENT_DEVICE_LEFT) {
            puts("Device left");
        }
        else if (completed == EVENT_TRANSFER_COMPLETED) {
            rc = libusb_submit_transfer(transfer);
        }
        else if (completed == EVENT_BUTTON_A_PRESSED) {
            rumble(devh, 0x20, 0x20);
            rc = libusb_submit_transfer(transfer);
        }
        else if (completed == EVENT_BUTTON_X_PRESSED) {
            break;
        }
    }

    // Shutting down from here onwards
    if (devh != NULL) {
        libusb_release_interface(devh, 0); // release the claimed interface
        libusb_close(devh); // close the device we opened
    }
    libusb_free_transfer(transfer);
    libusb_hotplug_deregister_callback(NULL, hotplug_callback_handle);
    libusb_exit(NULL); // deinitialize libusb
    puts("bye");
    return EXIT_SUCCESS;
}
