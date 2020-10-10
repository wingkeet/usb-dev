#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

void print_libusb_version(void)
{
    const struct libusb_version *version = libusb_get_version();
    printf("libusb v%u.%u.%u.%u%s (%s)\n",
        version->major, version->minor, version->micro, version->nano,
        version->rc, version->describe);
}

// Convert an array of integers to a comma-separated string
// WARNING: Buffer overflow is possible if `str` is too small
char *join(int len, const uint8_t nums[len], char str[])
{
    char num[8];
    str[0] = '\0';
    for (int i = 0; i < len; ++i) {
        snprintf(num, sizeof(num), "%u,", nums[i]);
        strcat(str, num);
    }
    if (len > 0) {
        str[strlen(str) - 1] = '\0'; // remove trailing comma
    }
    return str;
}

void print_port_path(libusb_device_handle *devh)
{
    uint8_t port_numbers[8];
    libusb_device *dev = libusb_get_device(devh);
    const int ports = libusb_get_port_numbers(dev, port_numbers, sizeof(port_numbers));
    char port_path[20];
    join(ports, port_numbers, port_path);
    printf("Port path: %s\n", port_path);
}

void printhex(int len, const uint8_t data[len])
{
    for (int i = 0; i < len; ++i) {
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
void data_to_gamepad(const uint8_t data[18], struct gamepad_t *gamepad)
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

void print_gamepad(const struct gamepad_t *gamepad)
{
    printf("  type: 0x%02X\n", gamepad->type);
    printf("  const_0: 0x%02X\n", gamepad->const_0);
    printf("  id: 0x%02X\n", gamepad->id);
    printf("  length: 0x%02X\n", gamepad->length);
    printf("  a,b,x,y: %u,%u,%u,%u\n", gamepad->a, gamepad->b, gamepad->x, gamepad->y);
    printf("  sync,menu,view: %u,%u,%u\n", gamepad->sync, gamepad->menu, gamepad->view);
    printf("  bumper left,right: %u,%u\n", gamepad->lbumper, gamepad->rbumper);
    printf("  stick left,right: %u,%u\n", gamepad->lstick, gamepad->rstick);
    printf("  dpad up,down,left,right: %u,%u,%u,%u\n",
        gamepad->dpad_up, gamepad->dpad_down, gamepad->dpad_left, gamepad->dpad_right);
    printf("  trigger left,right: %u,%u\n", gamepad->ltrigger, gamepad->rtrigger);
    printf("  lstick x,y: %d,%d\n", gamepad->lstick_x, gamepad->lstick_y);
    printf("  rstick x,y: %d,%d\n", gamepad->rstick_x, gamepad->rstick_y);
}

// Initialize controller (with input)
int init_device(libusb_device_handle *devh)
{
    uint8_t data[] = { 0x05, 0x20, 0x00, 0x01, 0x00 };
    int actual; // how many bytes were actually transferred

    // My device's out endpoint is 2
    return libusb_interrupt_transfer(devh, (2 | LIBUSB_ENDPOINT_OUT),
        data, sizeof(data), &actual, 0);
}

int rumble(libusb_device_handle *devh, uint8_t left, uint8_t right)
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

void do_sync_interrupt_transfer(libusb_device_handle *devh)
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
            puts("Read error");
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
        printf("Init error %d\n", rc);
        return EXIT_FAILURE;
    }
    libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);

    // These are the vendor_id and product_id I found for my usb device
    devh = libusb_open_device_with_vid_pid(NULL, 0x045e, 0x02ea);
    if (devh == NULL) {
        puts("Cannot open device");
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
        puts("Cannot claim interface");
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
    puts(rc == LIBUSB_SUCCESS ? "Released interface" : "Cannot release interface");
    libusb_close(devh); // close the device we opened
    libusb_exit(NULL); // deinitialize libusb
    return EXIT_SUCCESS;
}
