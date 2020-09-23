#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

void print_libusb_version(void) {
    const struct libusb_version *version = libusb_get_version();
    printf("libusb v%u.%u.%u.%u%s (%s)\n",
        version->major, version->minor, version->micro, version->nano,
        version->rc, version->describe);
}

// Convert an array of integers to a comma-separated string
// WARNING: Buffer overflow is possible if `str` is too small
char *join(const uint8_t nums[], int nums_len, char str[]) {
    char num[8];
    str[0] = '\0';
    for (int i = 0; i < nums_len; ++i) {
        snprintf(num, sizeof(num), "%u,", nums[i]);
        strcat(str, num);
    }
    if (nums_len > 0) {
        str[strlen(str) - 1] = '\0'; // remove trailing comma
    }
    return str;
}

void print_port_path(libusb_device_handle *devh) {
    uint8_t port_numbers[8];
    libusb_device *dev = libusb_get_device(devh);
    const int ports = libusb_get_port_numbers(dev, port_numbers, sizeof(port_numbers));
    char port_path[20];
    join(port_numbers, ports, port_path);
    printf("Port path: %s\n", port_path);
}

void printhex(const uint8_t data[], int length) {
    for (int i = 0; i < length; ++i) {
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
    uint8_t start : 1;
    uint8_t back : 1;
    uint8_t thumb_left : 1;
    uint8_t thumb_right : 1;
    uint8_t shoulder_left : 1;
    uint8_t shoulder_right : 1;
    uint8_t dpad_up : 1;
    uint8_t dpad_down : 1;
    uint8_t dpad_left : 1;
    uint8_t dpad_right : 1;

    // analog controls
    uint8_t trigger_left; // 0..255
    uint8_t trigger_right; // 0..255
    int16_t lthumb_x; // -32768..32767
    int16_t lthumb_y; // -32768..32767
    int16_t rthumb_x; // -32768..32767
    int16_t rthumb_y; // -32768..32767
};

// The `data` array must be at least 18 bytes in length
void data_to_gamepad(const uint8_t data[], struct gamepad_t *gamepad) {
    gamepad->type = data[0];
    gamepad->const_0 = data[1];
    gamepad->id = data[2];
    gamepad->length = data[3];
    gamepad->a = data[4] & 0x10 ? 1 : 0;
    gamepad->b = data[4] & 0x20 ? 1 : 0;
    gamepad->x = data[4] & 0x40 ? 1 : 0;
    gamepad->y = data[4] & 0x80 ? 1 : 0;
    gamepad->sync = data[4] & 0x01 ? 1 : 0;
    gamepad->start = data[4] & 0x04 ? 1 : 0;
    gamepad->back = data[4] & 0x08 ? 1 : 0;
    gamepad->shoulder_left = data[5] & 0x10 ? 1 : 0;
    gamepad->shoulder_right = data[5] & 0x20 ? 1 : 0;
    gamepad->thumb_left = data[5] & 0x40 ? 1 : 0;
    gamepad->thumb_right = data[5] & 0x80 ? 1 : 0;
    gamepad->dpad_up = data[5] & 0x01 ? 1 : 0;
    gamepad->dpad_down = data[5] & 0x02 ? 1 : 0;
    gamepad->dpad_left = data[5] & 0x04 ? 1 : 0;
    gamepad->dpad_right = data[5] & 0x08 ? 1 : 0;
    gamepad->trigger_left = data[6];
    gamepad->trigger_right = data[8];
    gamepad->lthumb_x = * (int16_t *) &data[10];
    gamepad->lthumb_y = * (int16_t *) &data[12];
    gamepad->rthumb_x = * (int16_t *) &data[14];
    gamepad->rthumb_y = * (int16_t *) &data[16];
}

void print_gamepad(const struct gamepad_t *gamepad) {
    printf("  type: 0x%02X\n", gamepad->type);
    printf("  const_0: 0x%02X\n", gamepad->const_0);
    printf("  id: 0x%02X\n", gamepad->id);
    printf("  length: 0x%02X\n", gamepad->length);
    printf("  a,b,x,y: %u,%u,%u,%u\n", gamepad->a, gamepad->b, gamepad->x, gamepad->y);
    printf("  sync,start,back: %u,%u,%u\n", gamepad->sync, gamepad->start, gamepad->back);
    printf("  shoulder left,right: %u,%u\n", gamepad->shoulder_left, gamepad->shoulder_right);
    printf("  thumb left,right: %u,%u\n", gamepad->thumb_left, gamepad->thumb_right);
    printf("  dpad up,down,left,right: %u,%u,%u,%u\n",
        gamepad->dpad_up, gamepad->dpad_down, gamepad->dpad_left, gamepad->dpad_right);
    printf("  trigger left,right: %u,%u\n", gamepad->trigger_left, gamepad->trigger_right);
    printf("  lthumb x,y: %d,%d\n", gamepad->lthumb_x, gamepad->lthumb_y);
    printf("  rthumb x,y: %d,%d\n", gamepad->rthumb_x, gamepad->rthumb_y);
}

void LIBUSB_CALL transfer_callback(struct libusb_transfer *transfer) {
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        printf("Transfer failed with status = %d\n", transfer->status);
        return;
    }

    printf("Read successful! %d bytes: ", transfer->actual_length);
    printhex(transfer->buffer, transfer->actual_length);
    putchar('\n');

    if (transfer->actual_length == 18 && transfer->buffer[0] == 0x20) {
        // We received button data
        struct gamepad_t gamepad;
        data_to_gamepad(transfer->buffer, &gamepad);
        print_gamepad(&gamepad);

        if (gamepad.x) {
            puts("Button X pressed");
            int *completed = transfer->user_data;
            *completed = 1;
        }
    }
}

void do_async_interrupt_transfer(libusb_device_handle *devh) {
    uint8_t data[18]; // data buffer
    int r;
    int completed = 0;

    printf("In %s()\n", __func__);

    struct libusb_transfer *transfer = libusb_alloc_transfer(0);
    if (transfer == NULL) {
        puts("libusb_alloc_transfer() failed");
        return;
    }
    libusb_fill_interrupt_transfer(transfer, devh, (2 | LIBUSB_ENDPOINT_IN),
        data, sizeof(data), transfer_callback, &completed, 0);

    while (!completed) {
        r = libusb_submit_transfer(transfer);
        if (r != LIBUSB_SUCCESS) {
            printf("libusb_submit_transfer() failed with r = %d\n", r);
            break;
        }
        r = libusb_handle_events_completed(NULL, &completed);
        if (r != LIBUSB_SUCCESS) {
            printf("libusb_handle_events_completed() failed with r = %d\n", r);
            break;
        }
    }

    libusb_free_transfer(transfer);
}

void rumble(libusb_device_handle *devh, uint8_t left, uint8_t right) {
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
    int r __attribute__((unused));

     // My device's out endpoint is 2
    r = libusb_interrupt_transfer(devh, (2 | LIBUSB_ENDPOINT_OUT),
        data, sizeof(data), &actual, 0);
}

void do_sync_interrupt_transfer(libusb_device_handle *devh) {
    uint8_t data[18]; // data buffer
    int actual; // how many bytes were actually transferred
    int r;

    printf("In %s()\n", __func__);

    while (true) {
         // My device's in endpoint is 2
        r = libusb_interrupt_transfer(devh, (2 | LIBUSB_ENDPOINT_IN),
            data, sizeof(data), &actual, 0);
        if (r == LIBUSB_SUCCESS) {
            printf("Read successful! %d bytes: ", actual);
            printhex(data, actual);
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

int main(void) {
    libusb_device_handle *devh = NULL; // device handle
    int r; // for return values

    print_libusb_version();

    r = libusb_init(NULL); // initialize libusb
    if (r != LIBUSB_SUCCESS) {
        printf("Init error %d\n", r);
        return 1;
    }
    libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);

    // These are the vendor_id and product_id I found for my usb device
    devh = libusb_open_device_with_vid_pid(NULL, 0x045e, 0x02ea);
    if (devh == NULL) {
        puts("Cannot open device");
        libusb_exit(NULL);
        return 2;
    }
    puts("Device opened");

    print_port_path(devh);

    // Let libusb handle detaching/attaching kernel driver for us
    r = libusb_set_auto_detach_kernel_driver(devh, 1);
    printf("libusb_set_auto_detach_kernel_driver() returned %d\n", r);

    // Claim interface 0 (the first) of device (mine had just 1)
    r = libusb_claim_interface(devh, 0);
    if (r != LIBUSB_SUCCESS) {
        puts("Cannot claim interface");
        libusb_close(devh);
        libusb_exit(NULL);
        return 3;
    }
    puts("Claimed interface");

    // Read forever until button X is pressed
    do_sync_interrupt_transfer(devh);

    // Shutting down from here onwards
    r = libusb_release_interface(devh, 0); // release the claimed interface
    puts(r == LIBUSB_SUCCESS ? "Released interface" : "Cannot release interface");
    libusb_close(devh); // close the device we opened
    libusb_exit(NULL); // deinitialize libusb
    return 0;
}
