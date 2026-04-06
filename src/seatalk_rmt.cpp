#include "seatalk_rmt.h"

SeatalkRMT::SeatalkRMT() {}

void SeatalkRMT::init(gpio_num_t rxPin, gpio_num_t txPin, rmt_channel_t rxChannel, rmt_channel_t txChannel) {
    _rxPin = rxPin;
    _txPin = txPin;
    _rxChannel = rxChannel;
    _txChannel = txChannel;


    Serial.print("Initializing RMT RX on pin ");
    Serial.print(_rxPin);
    Serial.print(" channel ");
    Serial.println(_rxChannel);

    pinMode(_rxPin, INPUT_PULLUP);

    rmt_rx = {};
    rmt_rx.channel = _rxChannel;
    rmt_rx.gpio_num = _rxPin;
    rmt_rx.clk_div = 80; 
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = true;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = IDLE_THRESHOLD_US;
    rmt_config(&rmt_rx);
    // INVERSION MATERIELLE (GPIO MATRIX)
    esp_rom_gpio_connect_in_signal(_rxPin, RMT_SIG_IN0_IDX, true);
    rmt_driver_install(rmt_rx.channel, 2048, 0);
    rmt_rx_start(_rxChannel, true);

    _bitpos = 0;
    _inframe = 0;
    _lasttransition = 0;

    Serial.println("RX initialized successfully");



    Serial.printf("Initializing RMT TX on pin ... TODO\n");

    pinMode(_txPin, OUTPUT);
    digitalWrite(_txPin, HIGH); // BUS à 12V (Repos) selon ta logique

    
    rmt_tx = {};
    rmt_tx.rmt_mode = RMT_MODE_TX;
    rmt_tx.channel = _txChannel;
    rmt_tx.gpio_num = _txPin;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = 80; 
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;
    
    rmt_config(&rmt_tx);

    rmt_driver_install(_txChannel, 0, 0);

    // On force la liaison pour que le RMT reprenne le PIN après le digitalWrite
    // rmt_set_gpio(_txChannel, RMT_MODE_TX, _txPin, false);
    rmt_set_idle_level(_txChannel, true, RMT_IDLE_LEVEL_HIGH);
    rmt_tx_stop(_txChannel);
    
}
    

uint8_t SeatalkRMT::reverse8(uint8_t x) {
    x = (x >> 4) | (x << 4);
    x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
    x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1);
    return x;
}

void SeatalkRMT::handleframe() {
    Serial.printf("FRAME READ : [ ");
    for(uint8_t i = 0; i < _framelen; i++) {
        Serial.printf("0x%02X ",_frame[i]);
    }
    Serial.println("]");
}

void SeatalkRMT::addchar() {

    uint8_t reg = (~(_shiftreg >> 2)) & 0xFF ;
    uint8_t newchar = reverse8( reg );

    // Serial.printf("Ajout caractère %d [",_charpos);
    // for (int i = 10; i >= 0; --i) {
    //     Serial.printf("%d", (_shiftreg >> i) & 1);
    // }
    // Serial.printf("] = %02X \n",newchar);

    if(_charpos==1) {
        // We read the second character, we can compute the frame size (4 least significant bits)
        _framelen += newchar & 0x0F;
    }

    // Serial.printf(" [start = %d][cd = %d][stop = %d][len = %d] \n",((_shiftreg>>10) & 1),((_shiftreg>>1) & 1),(_shiftreg & 1),_framelen);

    _frame[_charpos] = newchar;
    _charpos++;

    if(_framelen == _charpos) { 
        handleframe();
    }

    _shiftreg=0;
    _bitpos=0;
}

void SeatalkRMT::addbit(uint8_t level, uint8_t count) {
    // Serial.printf("Add %d bit %d \n",count,level);

    if(_inframe == false) {
        if(level==0) { // every new character must start with a startbit=1
            return;
        }
        _inframe  = true;
        _framelen = 3;
        _charpos  = 0;
        _bitpos   = 0;
        _shiftreg = 0x00;
    } 

    for(uint8_t i=count; i>0; i--) {
        _shiftreg = ( _shiftreg << 1 ) + level;
        _bitpos++;
        if(_bitpos==11) {
            addchar();
            if(level==0) { return; } // we cant's start a new character with a 0;
        }
    }
}

void SeatalkRMT::task() {
    size_t item_num = 0;
    rmt_item32_t* items = NULL;
    _rb = NULL;
    if( _inframe && ( micros() - _lasttransition ) > SEATALK_FRAME_TIMOUT ) {
        // on envoie des zero pour finaliser la frame ...
        addbit(0, (11 -_bitpos) );
        _inframe = 0;
    }
    if (rmt_get_ringbuf_handle(_rxChannel, &_rb) == ESP_OK && _rb != NULL) {
        // On récupère les données (timeout 100ms)
        items = (rmt_item32_t*) xRingbufferReceive(_rb, &item_num, pdMS_TO_TICKS(100));

        if (items != NULL) {
            // item_num est en octets, on divise par la taille d'un item (4 octets)
            int num_items = item_num / sizeof(rmt_item32_t);
            //  Serial.printf("\n>>> CAPTURE (%d transitions)\n", num_items * 2);
            for (int i = 0; i < num_items; i++) {
                // --- FRONT A ---
                if (items[i].duration0 > 0) {
                    // Serial.printf("Ordre %d | Niveau: %d | Durée: %4d us | %d bit %d\n", (i*2),   items[i].level0, items[i].duration0 , (items[i].duration0 + HALF_BIT_US) / SEATALK_BIT_US , items[i].level0);
                    addbit(1,(items[i].duration0 + HALF_BIT_US) / SEATALK_BIT_US);
                }
                // --- FRONT B ---
                if (items[i].duration1 > 0) {
                    // Serial.printf("Ordre %d | Niveau: %d | Durée: %4d us | %d bit %d\n", (i*2)+1, items[i].level1, items[i].duration1 , (items[i].duration1 + HALF_BIT_US) / SEATALK_BIT_US , items[i].level1);
                    addbit(0,(items[i].duration1 + HALF_BIT_US) / SEATALK_BIT_US);
                }
            _lasttransition = micros();
            }
            // Toujours rendre l'item au buffer pour libérer la mémoire
            vRingbufferReturnItem(_rb, (void*)items);
        }
    }
}

void SeatalkRMT::addItemBit(uint8_t bit, uint8_t closeframe) {
    if( ( _itemlastlevel == 0 && bit == 1 ) | closeframe==1 ) {
            // NEW Transition to 1
            _items[_itemtransitions].level0    = 0;  
            _items[_itemtransitions].level1    = 1; 
            _items[_itemtransitions].duration0 = SEATALK_BIT_US * _itemcount1;
            _items[_itemtransitions].duration1 = SEATALK_BIT_US * _itemcount0;
            // Serial.printf("new transition %d [%d(%d)-%d(%d)]\n", _itemtransitions, _items[_itemtransitions].duration0, _itemcount1, _items[_itemtransitions].duration1,_itemcount0);
            _itemtransitions ++ ;
            _itemcount1      = 0;
            _itemcount0      = 0;
            if(closeframe==1) return;
            }
    // Serial.printf("%d\n",bit);
    if(bit == 1) {  _itemcount1++;  }
    else         {  _itemcount0++;  }
    _itemlastlevel = bit;
}


bool SeatalkRMT::sendDatagram(uint8_t* buffer, uint8_t len) {

    for (int attempt = 0; attempt < 5; attempt++) {
        // Wait for silence
        uint32_t silenceStart = millis();
        bool busBusy          = true;
        bool compareok        = true;
        
        
        while (busBusy && (millis() - silenceStart < 100)) {
            // Si le bus est à 0V (LOW sur ESP selon ta logique), il est occupé
            if (digitalRead(_rxPin) == LOW) { 
                silenceStart = millis(); // On reset le chrono
            }
            if (millis() - silenceStart > 10) busBusy = false; // 10ms de silence
            yield();
        }
        

        // Send the datagram
        sendDatagramNoCD(buffer, len);

        // Check for collisions
        // Wait ( len*11*SEATALK_BIT_US ) us for the frame sending.  ~ len*3ms
                
        delay( ( len * 3 ) + 100 );

        // Serial.printf("compare : ");
        for(int i=0;i<len;i++) {
            // Serial.printf("[ %02X - %02X  = %d]",buffer[i],_frame[i], (buffer[i] == _frame[i]) );
            if (buffer[i] != _frame[i]) compareok = false;

        }
        if(compareok) { return true; }
        Serial.printf("Collision detectee, retry %d...\n", attempt + 1);
        delay(random(5, 50));
    }
    return false;
}

void SeatalkRMT::sendDatagramNoCD(uint8_t* buffer, uint8_t len) {
    if (len < 3 || len > 18) {
        // Serial.printf("Datagram non conforme \n");
        return;
    }
    else {
        // Serial.printf("WRITE datagram of %d char\n",len);
    }

    rmt_tx_stop(_txChannel);

    _itemtransitions = 0;
    _itemlastlevel   = 1;
    _itemcount1      = 0;
    _itemcount0      = 0;

    for(int8_t character_n = 0; character_n < len; character_n++ ) {
        // START BIT
        addItemBit(1);
        // ADD CHAR BITS LSB AND INVERTED
        for(int8_t bpos=0; bpos < 8; bpos++) {
            addItemBit( ( ( buffer[character_n] >> bpos ) + 1 ) & 1 );
        }
        // ADD CMD BIT
        if(character_n == 0 ) addItemBit(0);
        else                  addItemBit(1);
        // ADD STOP BIT
        addItemBit(0);
       }
    // Send a last bit to close the frame
    addItemBit(0,1);
    // Send the transitions
    esp_err_t err = rmt_write_items(_txChannel, _items, _itemtransitions, true);
    if (err != ESP_OK) {
        // Serial.printf("TX Error: %s\n", esp_err_to_name(err));
        return;
    }

    rmt_wait_tx_done(_txChannel, pdMS_TO_TICKS(50));
    delayMicroseconds(SEATALK_BIT_US * 2);
}