#ifndef USB_COMMON_H
#define USB_COMMON_H

#include <libusb-1.0/libusb.h>

extern const uint16_t VENDOR_ID;
extern const uint16_t PRODUCT_ID;

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

void print_libusb_version(void);
char *join(int len, const uint8_t nums[], char str[]);
void print_port_path(libusb_device_handle *devh);
void printhex(int len, const uint8_t data[]);
void data_to_gamepad(const uint8_t data[18], struct gamepad_t *gamepad);
void print_gamepad(const struct gamepad_t *gamepad);
int init_device(libusb_device_handle *devh);
int rumble(libusb_device_handle *devh, uint8_t left, uint8_t right);

#endif // USB_COMMON_H
