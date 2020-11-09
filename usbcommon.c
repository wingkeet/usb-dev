#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "usbcommon.h"

const uint16_t VENDOR_ID = 0x045e;
const uint16_t PRODUCT_ID = 0x02ea;

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
    for (int i = 0; i < len; i++) {
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
    for (int i = 0; i < len; i++) {
        printf("%02X", data[i]);
    }
}

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
