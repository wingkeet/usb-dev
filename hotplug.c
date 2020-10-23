#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <libusb-1.0/libusb.h>

enum {
    EVENT_QUIT = 1,
    EVENT_DEVICE_ARRIVED = 2,
    EVENT_DEVICE_LEFT = 3,
    EVENT_TRANSFER_COMPLETED = 4,
    EVENT_BUTTON_A_PRESSED = 5,
    EVENT_BUTTON_X_PRESSED = 6
};

static const uint16_t VENDOR_ID = 0x045e;
static const uint16_t PRODUCT_ID = 0x02ea;
static libusb_device_handle *devh = NULL;
static struct libusb_transfer *transfer = NULL;
static libusb_hotplug_callback_handle hotplug_callback_handle = 0;
static int completed = 0;

static void print_libusb_version(void)
{
    const struct libusb_version *version = libusb_get_version();
    printf("libusb v%u.%u.%u.%u%s (%s)\n",
        version->major, version->minor, version->micro, version->nano,
        version->rc, version->describe);
}

// Convert an array of integers to a comma-separated string
// WARNING: Buffer overflow is possible if `str` is too small
static char *join(int len, const uint8_t nums[len], char str[])
{
    char num[8];
    str[0] = '\0';
    for (int i = 0; i < len; i++) {
        snprintf(num, sizeof(num), "%u,", nums[i]);
        strcat(str, num);
    }
    if (len > 0) {
        str[strlen(str) - 1] = '\0'; // remove trailing comma
    }
    return str;
}

static void print_port_path(libusb_device_handle *devh)
{
    uint8_t port_numbers[8];
    libusb_device *dev = libusb_get_device(devh);
    const int ports = libusb_get_port_numbers(dev, port_numbers, sizeof(port_numbers));
    char port_path[20];
    join(ports, port_numbers, port_path);
    printf("Port path: %s\n", port_path);
}

static void printhex(int len, const uint8_t data[len])
{
    for (int i = 0; i < len; i++) {
        printf("%02X", data[i]);
    }
}

struct gamepad_t {
    uint8_t type; // 0x20
    uint8_t const_0; // 0x00
    uint8_t id;
    uint8_t length; // 0x0E

    // digital buttons
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t x : 1;
    uint8_t y : 1;
    uint8_t sync : 1;
    uint8_t menu : 1;
    uint8_t view : 1;
    uint8_t lstick : 1;
    uint8_t rstick : 1;
    uint8_t lbumper : 1;
    uint8_t rbumper : 1;
    uint8_t dpad_up : 1;
    uint8_t dpad_down : 1;
    uint8_t dpad_left : 1;
    uint8_t dpad_right : 1;

    // analog controls
    uint8_t ltrigger; // [0,255]
    uint8_t rtrigger; // [0,255]
    int16_t lstick_x; // [-32768,32767]
    int16_t lstick_y; // [-32768,32767]
    int16_t rstick_x; // [-32768,32767]
    int16_t rstick_y; // [-32768,32767]
};

// The `data` array must be at least 18 bytes in length
static void data_to_gamepad(const uint8_t data[18], struct gamepad_t *gamepad)
{
    gamepad->type = data[0];
    gamepad->const_0 = data[1];
    gamepad->id = data[2];
    gamepad->length = data[3];
    gamepad->a = data[4] & 0x10 ? 1 : 0;
    gamepad->b = data[4] & 0x20 ? 1 : 0;
    gamepad->x = data[4] & 0x40 ? 1 : 0;
    gamepad->y = data[4] & 0x80 ? 1 : 0;
    gamepad->sync = data[4] & 0x01 ? 1 : 0;
    gamepad->menu = data[4] & 0x04 ? 1 : 0;
    gamepad->view = data[4] & 0x08 ? 1 : 0;
    gamepad->lbumper = data[5] & 0x10 ? 1 : 0;
    gamepad->rbumper = data[5] & 0x20 ? 1 : 0;
    gamepad->lstick = data[5] & 0x40 ? 1 : 0;
    gamepad->rstick = data[5] & 0x80 ? 1 : 0;
    gamepad->dpad_up = data[5] & 0x01 ? 1 : 0;
    gamepad->dpad_down = data[5] & 0x02 ? 1 : 0;
    gamepad->dpad_left = data[5] & 0x04 ? 1 : 0;
    gamepad->dpad_right = data[5] & 0x08 ? 1 : 0;
    gamepad->ltrigger = data[6];
    gamepad->rtrigger = data[8];
    gamepad->lstick_x = * (int16_t *) &data[10];
    gamepad->lstick_y = * (int16_t *) &data[12];
    gamepad->rstick_x = * (int16_t *) &data[14];
    gamepad->rstick_y = * (int16_t *) &data[16];
}

// Initialize controller (with input)
static int init_device(libusb_device_handle *devh)
{
    uint8_t data[] = { 0x05, 0x20, 0x00, 0x01, 0x00 };
    int actual; // how many bytes were actually transferred

    // My device's out endpoint is 2
    return libusb_interrupt_transfer(devh, (2 | LIBUSB_ENDPOINT_OUT),
        data, sizeof(data), &actual, 0);
}

static int rumble(libusb_device_handle *devh, uint8_t left, uint8_t right)
{
    uint8_t data[] = {
        0x09, // activate rumble
        0x00,
        0x00,
        0x09, // length
        0x00,
        0x0F,
        0x00,
        0x00,
        left, // low-frequency motor
        right, // high-frequency motor
        0x10, // on period
        0x00, // off period
        0x01  // repeat count
    };
    int actual; // how many bytes were actually transferred

    // My device's out endpoint is 2
    return libusb_interrupt_transfer(devh, (2 | LIBUSB_ENDPOINT_OUT),
        data, sizeof(data), &actual, 0);
}

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
            puts("Could not open USB device");
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
        printf("Unhandled event %d\n", event);
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
        printf("Transfer failed with status = %s\n", libusb_error_name(transfer->status));
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
        puts("Hotplug is not supported");
        libusb_exit(NULL);
        return EXIT_FAILURE;
    }

    rc = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0, VENDOR_ID, PRODUCT_ID,
        LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, &completed,
        &hotplug_callback_handle);
    if (rc != LIBUSB_SUCCESS) {
        puts("Error registering hotplug callback");
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
