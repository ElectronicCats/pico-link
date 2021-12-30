/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <get_serial.h>
#include <hardware/clocks.h>
#include <DAP.h>
#include "main.h"
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "usb_descriptors.h"
#include "virtual_UART.h"
#include "DAP_queue.h"

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);

void hid_task(void);

void cdc_task();

static volatile uint8_t USB_ResponseIdle;
static DAP_queue DAP_Cmd_queue;

int main() {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    usb_serial_init(); //读取芯片序列号作为USB序列号
    tusb_init();

    DAP_Setup();

//    USB_ResponseIdle = 1;
//    DAP_queue_init(&DAP_Cmd_queue);

    while (true) {
        tud_task();
//        hid_task();
        led_blinking_task();
    }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
    blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
    blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
    blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(
        uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
        uint16_t reqlen) {
    // TODO not Implemented
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

uint8_t dealing_data;
uint8_t MYUSB_Request[64 + 1];    // Request  Buffer
uint8_t MYUSB_Response[64 + 1]; // Response Buffer
// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(
        uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *RxDataBuffer,
        uint16_t bufsize) {
//    (void) instance;

    static uint8_t TxDataBuffer[CFG_TUD_HID_EP_BUFSIZE];
    uint32_t response_size = TU_MIN(CFG_TUD_HID_EP_BUFSIZE, bufsize);

    DAP_ProcessCommand(RxDataBuffer, TxDataBuffer);

    tud_hid_report(0, TxDataBuffer, response_size);

//    if (RxDataBuffer[0] == ID_DAP_TransferAbort) {
//        DAP_TransferAbort = 1;
//        return;
//    }
//    if (dealing_data)
//        return; // Discard packet when buffer is full
//    memcpy(MYUSB_Request, RxDataBuffer, bufsize);
//    dealing_data = 1;
}

void hid_task(void) {
    if (dealing_data) {
        DAP_ProcessCommand(MYUSB_Request, MYUSB_Response);
        tud_hid_report(0, MYUSB_Response, 64);
        dealing_data = 0;
    }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void) {
    static uint32_t start_ms = 0;
    static bool led_state = false;

    // blink is disabled
    if (!blink_interval_ms) return;

    // Blink every interval ms
    if (board_millis() - start_ms < blink_interval_ms) return; // not enough time
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle

//    uint32_t clock = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
//    char ch[30];
//    sprintf(ch,"clock:%d\r\n", clock);
//    VCOM_SendString(ch);
}

