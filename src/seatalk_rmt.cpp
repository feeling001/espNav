#include "seatalk_rmt.h"
#include "functions.h"
#include <string.h>

SeatalkRMT::SeatalkRMT(LogManager* logManager) {
    _logManager = logManager;
}

void SeatalkRMT::init(gpio_num_t rxPin, gpio_num_t txPin, rmt_channel_t rxChannel, rmt_channel_t txChannel, bool invertRx, bool invertTx) {
    _rxPin = rxPin;
    _txPin = txPin;
    _rxChannel = rxChannel;
    _txChannel = txChannel;

    _invertRx = invertRx;
    _invertTx = invertTx;


    serialPrintf("[SeaTalk] Initializing RMT RX on pin %d, channel %d (invert = %i) \n",_rxPin,_rxChannel,_invertRx);

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
    esp_rom_gpio_connect_in_signal(_rxPin, RMT_SIG_IN0_IDX, _invertRx);
    rmt_driver_install(rmt_rx.channel, 2048, 0);
    rmt_rx_start(_rxChannel, true);

    _bitpos = 0;
    _inframe = 0;
    _lasttransition = 0;

    serialPrintf("[SeaTalk] Initializing RMT TX on pin %d, channel %d (invert = %i)\n",_txPin,_txChannel,_invertTx);

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
    rmt_tx.tx_config.idle_level = _invertTx ? RMT_IDLE_LEVEL_HIGH : RMT_IDLE_LEVEL_LOW;
    
    rmt_config(&rmt_tx);

    rmt_driver_install(_txChannel, 0, 0);

    // On force la liaison pour que le RMT reprenne le PIN après le digitalWrite
    // rmt_set_gpio(_txChannel, RMT_MODE_TX, _txPin, false);
    rmt_set_idle_level(_txChannel, true, _invertTx ? RMT_IDLE_LEVEL_HIGH : RMT_IDLE_LEVEL_LOW);
    rmt_tx_stop(_txChannel);

    if (!_rxMutex) _rxMutex = xSemaphoreCreateMutex();
}
    

uint8_t SeatalkRMT::reverse8(uint8_t x) {
    x = (x >> 4) | (x << 4);
    x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
    x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1);
    return x;
}

void SeatalkRMT::handleframe() {
    serialPrintf("FRAME READ : [ ");
    for(uint8_t i = 0; i < _framelen; i++) {
        serialPrintf("0x%02X ",_frame[i]);
    }
    serialPrintf("]\n");

    // Check this frame against sendDatagram()'s armed echo expectation *right
    // now*, while _frame still holds this exact frame — a later frame
    // decoded within the same processIncoming() call would otherwise
    // overwrite _frame before the wait loop gets a chance to compare it.
    if (_expectEcho && _framelen == _expectEchoLen &&
        memcmp(_frame, _expectEcho, _framelen) == 0) {
        _echoMatched = true;
    }

    _logManager->logSeatalk(_frame, _framelen);

    // Push into the FIFO queue. If the queue is full (consumer too slow),
    // drop the oldest queued frame to make room rather than silently
    // dropping this newly-completed one — either way a frame is lost, but
    // this keeps the queue full of the most recent data.
    if (_frameQueueCount == kFrameQueueSize) {
        _frameQueueHead = (_frameQueueHead + 1) % kFrameQueueSize;
        _frameQueueCount--;
        serialPrintf("[SeaTalk] RX queue full, dropping oldest frame\n");
    }
    uint8_t len = (_framelen > sizeof(_frame)) ? sizeof(_frame) : _framelen;
    memcpy(_frameQueue[_frameQueueTail], _frame, len);
    _frameQueueLen[_frameQueueTail] = len;
    _frameQueueTail = (_frameQueueTail + 1) % kFrameQueueSize;
    _frameQueueCount++;

    // Signal to sendDatagram()'s collision check that a new frame has just
    // been decoded into _frame/_framelen.
    _frameSeq++;

    // A frame is now complete. Force re-sync on the next start bit instead
    // of leaving _inframe set — otherwise any further bit transitions before
    // the SEATALK_FRAME_TIMOUT silence window (bus noise, a following
    // character sent by another device, etc.) would keep calling addchar()
    // and writing past the end of this now-"finished" frame with no bounds
    // check, corrupting adjacent memory (buffer overflow -> random crashes).
    _inframe = 0;
    _charpos = 0;
}

bool SeatalkRMT::getFrame(uint8_t* outFrame, uint8_t& outLen) {
    bool has = false;
    if (_rxMutex && xSemaphoreTake(_rxMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (_frameQueueCount > 0) {
            outLen = _frameQueueLen[_frameQueueHead];
            memcpy(outFrame, _frameQueue[_frameQueueHead], outLen);
            _frameQueueHead = (_frameQueueHead + 1) % kFrameQueueSize;
            _frameQueueCount--;
            has = true;
        }
        xSemaphoreGive(_rxMutex);
    }
    return has;
}

void SeatalkRMT::addchar() {

    uint8_t reg = (~(_shiftreg >> 2)) & 0xFF ;
    uint8_t newchar = reverse8( reg );

    // serialPrintf("Ajout caractère %d [",_charpos);
    // for (int i = 10; i >= 0; --i) {
    //     serialPrintf("%d", (_shiftreg >> i) & 1);
    // }
    // serialPrintf("] = %02X \n",newchar);

    if(_charpos==1) {
        // We read the second character, we can compute the frame size (4 least significant bits)
        _framelen += newchar & 0x0F;
    }

    // serialPrintf(" [start = %d][cd = %d][stop = %d][len = %d] \n",((_shiftreg>>10) & 1),((_shiftreg>>1) & 1),(_shiftreg & 1),_framelen);

    // Defensive bounds check: never write past the _frame buffer. This can
    // only be reached if the state machine somehow desyncs (e.g. bus noise
    // producing more characters than a properly terminated frame would),
    // since _framelen is otherwise capped at 3 + 0x0F = 18 == sizeof(_frame).
    if (_charpos >= sizeof(_frame)) {
        _inframe = 0;
        _charpos = 0;
        _shiftreg = 0;
        _bitpos = 0;
        return;
    }

    _frame[_charpos] = newchar;
    _charpos++;

    if(_framelen == _charpos) { 
        handleframe();
    }

    _shiftreg=0;
    _bitpos=0;
}

void SeatalkRMT::addbit(uint8_t level, uint8_t count) {
    // serialPrintf("Add %d bit %d \n",count,level);

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

void SeatalkRMT::processIncoming(uint32_t rbTimeoutMs) {
    // Serialize access to the RX ring buffer + decode state machine: task()
    // (SeaTalk FreeRTOS task) and sendDatagram()'s wait loop (which may run
    // on a different task, e.g. the web server thread issuing an autopilot
    // command) can both call this concurrently. Without this mutex the two
    // callers race on _rb / _frame / _bitpos / ... corrupting RX decoding.
    if (_rxMutex && xSemaphoreTake(_rxMutex, pdMS_TO_TICKS(rbTimeoutMs)) != pdTRUE) {
        return; // another caller currently owns RX processing — try again later
    }

    size_t item_num = 0;
    rmt_item32_t* items = NULL;
    _rb = NULL;
    if( _inframe && ( micros() - _lasttransition ) > SEATALK_FRAME_TIMOUT ) {
        // on envoie des zero pour finaliser la frame ...
        addbit(0, (11 -_bitpos) );
        _inframe = 0;
    }
    if (rmt_get_ringbuf_handle(_rxChannel, &_rb) == ESP_OK && _rb != NULL) {
        // On récupère les données (timeout configurable)
        items = (rmt_item32_t*) xRingbufferReceive(_rb, &item_num, pdMS_TO_TICKS(rbTimeoutMs));

        if (items != NULL) {
            // item_num est en octets, on divise par la taille d'un item (4 octets)
            int num_items = item_num / sizeof(rmt_item32_t);
            //  serialPrintf("\n>>> CAPTURE (%d transitions)\n", num_items * 2);
            for (int i = 0; i < num_items; i++) {
                // --- FRONT A ---
                if (items[i].duration0 > 0) {
                    // serialPrintf("Ordre %d | Niveau: %d | Durée: %4d us | %d bit %d\n", (i*2),   items[i].level0, items[i].duration0 , (items[i].duration0 + HALF_BIT_US) / SEATALK_BIT_US , items[i].level0);
                    addbit(1,(items[i].duration0 + HALF_BIT_US) / SEATALK_BIT_US);
                }
                // --- FRONT B ---
                if (items[i].duration1 > 0) {
                    // serialPrintf("Ordre %d | Niveau: %d | Durée: %4d us | %d bit %d\n", (i*2)+1, items[i].level1, items[i].duration1 , (items[i].duration1 + HALF_BIT_US) / SEATALK_BIT_US , items[i].level1);
                    addbit(0,(items[i].duration1 + HALF_BIT_US) / SEATALK_BIT_US);
                }
            _lasttransition = micros();
            }
            // Toujours rendre l'item au buffer pour libérer la mémoire
            vRingbufferReturnItem(_rb, (void*)items);
        }
    }

    if (_rxMutex) xSemaphoreGive(_rxMutex);
}

void SeatalkRMT::task() {
    processIncoming(100);
}

void SeatalkRMT::addItemBit(uint8_t bit, uint8_t closeframe) {
    if( ( _itemlastlevel == 0 && bit == 1 ) | closeframe==1 ) {
            // NEW Transition to 1
            _items[_itemtransitions].level0    = _invertTx ? 0 : 1;  
            _items[_itemtransitions].level1    = _invertTx ? 1 : 0;
            _items[_itemtransitions].duration0 = SEATALK_BIT_US * _itemcount1;
            _items[_itemtransitions].duration1 = SEATALK_BIT_US * _itemcount0;
            // serialPrintf("new transition %d [%d(%d)-%d(%d)]\n", _itemtransitions, _items[_itemtransitions].duration0, _itemcount1, _items[_itemtransitions].duration1,_itemcount0);
            _itemtransitions ++ ;
            _itemcount1      = 0;
            _itemcount0      = 0;
            if(closeframe==1) return;
            }
    // serialPrintf("%d\n",bit);
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
            // NOTE: yield() alone does not guarantee the IDLE0 task gets
            // scheduled — if the bus stays LOW continuously this becomes a
            // tight busy-loop on this priority-5 task and starves IDLE0,
            // tripping the task watchdog (abort/reboot). A real vTaskDelay
            // forces the scheduler to run lower-priority tasks (IDLE0).
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        

        // Arm the echo detector *before* transmitting: handleframe() will
        // compare every frame it decodes against `buffer` the instant it
        // completes (see _expectEcho doc in seatalk_rmt.h). This must happen
        // synchronously inside handleframe(), not by re-reading `_frame`
        // here afterward — a single processIncoming() call below can drain
        // a backlog of several frames at once (e.g. built up during the
        // "wait for bus silence" loop above, which never drains the RX ring
        // buffer), and our own echo could be overwritten by a later,
        // unrelated frame before we get a chance to look at `_frame` again.
        // That caused false "collision" detections and needless real
        // retransmissions of the same command (e.g. a single "+1" keystroke
        // reaching the autopilot 3-5 times).
        _echoMatched   = false;
        _expectEchoLen = len;
        _expectEcho    = buffer;

        // Send the datagram
        sendDatagramNoCD(buffer, len);

        // Check for collisions
        // Wait ( len*11*SEATALK_BIT_US ) us for the frame sending.  ~ len*3ms
        // NOTE: we must keep draining the RX ring buffer (processIncoming)
        // during this wait — this task is single-threaded with RX decoding,
        // so a blind delay() here would starve frame decoding.
        compareok = false;
        uint32_t waitMs = ( len * 3 ) + 100;
        uint32_t waitStart = millis();
        while (millis() - waitStart < waitMs) {
            processIncoming(5);
            if (_echoMatched) { compareok = true; break; }
        }
        _expectEcho = nullptr;
        if(compareok) { return true; }
        serialPrintf("Collision detectee, retry %d...\n", attempt + 1);

        uint32_t backoffMs = random(5, 50);
        uint32_t backoffStart = millis();
        while (millis() - backoffStart < backoffMs) {
            processIncoming(5);
        }
    }
    return false;
}

void SeatalkRMT::sendDatagramNoCD(uint8_t* buffer, uint8_t len) {
    if (len < 3 || len > 18) {
        // serialPrintf("Datagram non conforme \n");
        return;
    }
    else {
        // serialPrintf("WRITE datagram of %d char\n",len);
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
        // serialPrintf("TX Error: %s\n", esp_err_to_name(err));
        return;
    }

    rmt_wait_tx_done(_txChannel, pdMS_TO_TICKS(50));
    delayMicroseconds(SEATALK_BIT_US * 2);
}