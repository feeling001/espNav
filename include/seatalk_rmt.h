#pragma once

#include <Arduino.h>
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"


// A METTRE DANS CONFIG !!!
#define SEATALK_BIT_US          208
#define HALF_BIT_US             104
#define SEATALK_FRAME_TIMOUT    2080
#define IDLE_THRESHOLD_US       3000 // 3ms de silence pour séparer les messages


typedef void (*seatalk_rx_callback_t)(uint16_t data);

class SeatalkRMT {
public:
    SeatalkRMT();

    void init(gpio_num_t rxPin, gpio_num_t txPin,
               rmt_channel_t rxChannel = RMT_CHANNEL_4,
               rmt_channel_t txChannel = RMT_CHANNEL_1);

    void task();  // à appeler dans loop()
    bool sendDatagram(uint8_t* buffer, uint8_t len);
    

private:
    gpio_num_t          _rxPin;
    gpio_num_t          _txPin;

    rmt_config_t        rmt_rx;
    rmt_config_t        rmt_tx;

    rmt_channel_t       _rxChannel;
    rmt_channel_t       _txChannel;

    RingbufHandle_t     _rb = nullptr;

    seatalk_rx_callback_t _callback = nullptr;

    uint32_t            _lasttransition;
    uint8_t             _inframe;
    uint8_t             _bitpos;
    uint8_t             _charpos;
    uint16_t            _shiftreg;
    uint8_t             _framelen;
    uint8_t             _frame[18];

    rmt_item32_t        _items[128];
    uint8_t             _itemcount1;
    uint8_t             _itemcount0;
    uint8_t             _itemtransitions;
    uint8_t             _itemlastlevel;

    /*TX METHODS*/
    void addItemBit(uint8_t bit, uint8_t closeframe=0);
    void sendDatagramNoCD(uint8_t* buffer, uint8_t len);
    
    /*RX METHODS*/
    void addbit(uint8_t level, uint8_t count);
    void addchar();
    void handleframe();

    uint8_t reverse8(uint8_t x);
};