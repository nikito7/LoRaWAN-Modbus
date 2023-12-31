#include "LMIC-node.h"
#include "ModbusMaster.h"

// Adjust to fit max payload length
const uint8_t payloadBufferLength = 35;

ModbusMaster node;

#include "han.h"

uint16_t txFail = 0;

//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▀ █▀█ █▀▄
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▀ █ █ █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀▀ ▀ ▀ ▀▀ 

uint8_t payloadBuffer[payloadBufferLength];
static osjob_t doWorkJob;

// Change value in platformio.ini
uint32_t doWorkIntervalSeconds = DO_WORK_INTERVAL_SECONDS;

// Note: LoRa module pin mappings are defined in
// the Board Support Files.

// Set LoRaWAN keys defined in lorawan-keys.h.
#ifdef OTAA_ACTIVATION
    static const u1_t PROGMEM DEVEUI[8]  = { OTAA_DEVEUI } ;
    static const u1_t PROGMEM APPEUI[8]  = { OTAA_APPEUI };
    static const u1_t PROGMEM APPKEY[16] = { OTAA_APPKEY };
    // Below callbacks are used by LMIC for reading above values.
    void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
    void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
    void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16); }    
#else
    // ABP activation
    static const u4_t DEVADDR = ABP_DEVADDR ;
    static const PROGMEM u1_t NWKSKEY[16] = { ABP_NWKSKEY };
    static const u1_t PROGMEM APPSKEY[16] = { ABP_APPSKEY };
    // Below callbacks are not used be they must be defined.
    void os_getDevEui (u1_t* buf) { }
    void os_getArtEui (u1_t* buf) { }
    void os_getDevKey (u1_t* buf) { }
#endif


int16_t getSnrTenfold()
{
    // Returns ten times the SNR (dB) value of the last received packet.
    // Ten times to prevent the use of float but keep 1 decimal digit accuracy.
    // Calculation per SX1276 datasheet rev.7 §6.4, SX1276 datasheet rev.4 §6.4.
    // LMIC.snr contains value of PacketSnr, which is 4 times the actual SNR value.
    return (LMIC.snr * 10) / 4;
}


int16_t getRssi(int8_t snr)
{
    // Returns correct RSSI (dBm) value of the last received packet.
    // Calculation per SX1276 datasheet rev.7 §5.5.5, SX1272 datasheet rev.4 §5.5.5.

    #define RSSI_OFFSET            64
    #define SX1276_FREQ_LF_MAX     525000000     // per datasheet 6.3
    #define SX1272_RSSI_ADJUST     -139
    #define SX1276_RSSI_ADJUST_LF  -164
    #define SX1276_RSSI_ADJUST_HF  -157

    int16_t rssi;

    #ifdef MCCI_LMIC

        rssi = LMIC.rssi - RSSI_OFFSET;

    #else
        int16_t rssiAdjust;
        #ifdef CFG_sx1276_radio
            if (LMIC.freq > SX1276_FREQ_LF_MAX)
            {
                rssiAdjust = SX1276_RSSI_ADJUST_HF;
            }
            else
            {
                rssiAdjust = SX1276_RSSI_ADJUST_LF;   
            }
        #else
            // CFG_sx1272_radio    
            rssiAdjust = SX1272_RSSI_ADJUST;
        #endif    
        
        // Revert modification (applied in lmic/radio.c) to get PacketRssi.
        int16_t packetRssi = LMIC.rssi + 125 - RSSI_OFFSET;
        if (snr < 0)
        {
            rssi = rssiAdjust + packetRssi + snr;
        }
        else
        {
            rssi = rssiAdjust + (16 * packetRssi) / 15;
        }
    #endif

    return rssi;
}



#ifdef ABP_ACTIVATION
    void setAbpParameters(dr_t dataRate = DefaultABPDataRate, s1_t txPower = DefaultABPTxPower) 
    {
        // Set static session parameters. Instead of dynamically establishing a session
        // by joining the network, precomputed session parameters are be provided.
        #ifdef PROGMEM
            // On AVR, these values are stored in flash and only copied to RAM
            // once. Copy them to a temporary buffer here, LMIC_setSession will
            // copy them into a buffer of its own again.
            uint8_t appskey[sizeof(APPSKEY)];
            uint8_t nwkskey[sizeof(NWKSKEY)];
            memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
            memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
            LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
        #else
            // If not running an AVR with PROGMEM, just use the arrays directly
            LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
        #endif

        #if defined(CFG_eu868)
            // Set up the channels used by the Things Network, which corresponds
            // to the defaults of most gateways. Without this, only three base
            // channels from the LoRaWAN specification are used, which certainly
            // works, so it is good for debugging, but can overload those
            // frequencies, so be sure to configure the full frequency range of
            // your network here (unless your network autoconfigures them).
            // Setting up channels should happen after LMIC_setSession, as that
            // configures the minimal channel set. The LMIC doesn't let you change
            // the three basic settings, but we show them here.
            LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
            LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
            // TTN defines an additional channel at 869.525Mhz using SF9 for class B
            // devices' ping slots. LMIC does not have an easy way to define set this
            // frequency and support for class B is spotty and untested, so this
            // frequency is not configured here.
        #elif defined(CFG_us915) || defined(CFG_au915)
            // NA-US and AU channels 0-71 are configured automatically
            // but only one group of 8 should (a subband) should be active
            // TTN recommends the second sub band, 1 in a zero based count.
            // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
            LMIC_selectSubBand(1);
        #elif defined(CFG_as923)
            // Set up the channels used in your country. Only two are defined by default,
            // and they cannot be changed.  Use BAND_CENTI to indicate 1% duty cycle.
            // LMIC_setupChannel(0, 923200000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
            // LMIC_setupChannel(1, 923400000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);

            // ... extra definitions for channels 2..n here
        #elif defined(CFG_kr920)
            // Set up the channels used in your country. Three are defined by default,
            // and they cannot be changed. Duty cycle doesn't matter, but is conventionally
            // BAND_MILLI.
            // LMIC_setupChannel(0, 922100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(1, 922300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(2, 922500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);

            // ... extra definitions for channels 3..n here.
        #elif defined(CFG_in866)
            // Set up the channels used in your country. Three are defined by default,
            // and they cannot be changed. Duty cycle doesn't matter, but is conventionally
            // BAND_MILLI.
            // LMIC_setupChannel(0, 865062500, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(1, 865402500, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(2, 865985000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);

            // ... extra definitions for channels 3..n here.
        #endif

        // Disable link check validation
        LMIC_setLinkCheckMode(0);

        // TTN uses SF9 for its RX2 window.
        LMIC.dn2Dr = DR_SF9;

        // Set data rate and transmit power (note: txpow is possibly ignored by the library)
        LMIC_setDrTxpow(dataRate, txPower);    
    }
#endif //ABP_ACTIVATION


void initLmic(bit_t adrEnabled = 1,
              dr_t abpDataRate = DefaultABPDataRate, 
              s1_t abpTxPower = DefaultABPTxPower) 
{
    ostime_t timestamp = os_getTime();

    // Initialize LMIC runtime environment
    os_init();
    // Reset MAC state
    LMIC_reset();

    #ifdef ABP_ACTIVATION
        setAbpParameters(abpDataRate, abpTxPower);
    #endif

    // Enable or disable ADR (data rate adaptation). 
    // Should be turned off if the device is not stationary (mobile).
    // 1 is on, 0 is off.
    LMIC_setAdrMode(adrEnabled);

    if (activationMode == ActivationMode::OTAA)
    {
        #if defined(CFG_us915) || defined(CFG_au915)
            // NA-US and AU channels 0-71 are configured automatically
            // but only one group of 8 should (a subband) should be active
            // TTN recommends the second sub band, 1 in a zero based count.
            // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
            LMIC_selectSubBand(1); 
        #endif
    }

    // Relax LMIC timing if defined
    #if defined(LMIC_CLOCK_ERROR_PPM)
        uint32_t clockError = 0;
        #if LMIC_CLOCK_ERROR_PPM > 0
            #if defined(MCCI_LMIC) && LMIC_CLOCK_ERROR_PPM > 4000
                // Allow clock error percentage to be > 0.4%
                #define LMIC_ENABLE_arbitrary_clock_error 1
            #endif    
            clockError = (LMIC_CLOCK_ERROR_PPM / 100) * (MAX_CLOCK_ERROR / 100) / 100;
            LMIC_setClockError(clockError);
        #endif


    #endif

    #ifdef MCCI_LMIC
        // Register a custom eventhandler and don't use default onEvent() to enable
        // additional features (e.g. make EV_RXSTART available). User data pointer is omitted.
        LMIC_registerEventCb(&onLmicEvent, nullptr);
    #endif
}


#ifdef MCCI_LMIC 
void onLmicEvent(void *pUserData, ev_t ev)
#else
void onEvent(ev_t ev) 
#endif
{
    // LMIC event handler
    ostime_t timestamp = os_getTime(); 

    switch (ev) 
    {
#ifdef MCCI_LMIC
        // Only supported in MCCI LMIC library:
        case EV_RXSTART:
            // Do not print anything for this event or it will mess up timing.
            break;

        case EV_TXSTART:           
            break;               

        case EV_JOIN_TXCOMPLETE:
        case EV_TXCANCELED:

            break;               
#endif
        case EV_JOINED:


            // Disable link check validation.
            // Link check validation is automatically enabled
            // during join, but because slow data rates change
            // max TX size, it is not used in this example.                    
            LMIC_setLinkCheckMode(0);

            // The doWork job has probably run already (while
            // the node was still joining) and have rescheduled itself.
            // Cancel the next scheduled doWork job and re-schedule
            // for immediate execution to prevent that any uplink will
            // have to wait until the current doWork interval ends.
            os_clearCallback(&doWorkJob);
            os_setCallback(&doWorkJob, doWorkCallback);
            break;

        case EV_TXCOMPLETE:
            // Transmit completed, includes waiting for RX windows.
  

            // Check if downlink was received
            if (LMIC.dataLen != 0 || LMIC.dataBeg != 0)
            {
                uint8_t fPort = 0;
                if (LMIC.txrxFlags & TXRX_PORT)
                {
                    fPort = LMIC.frame[LMIC.dataBeg -1];
                }
                processDownlink(timestamp, fPort, LMIC.frame + LMIC.dataBeg, LMIC.dataLen);                
            }
            break;     
          
        // Below events are printed only.
        case EV_SCAN_TIMEOUT:
        case EV_BEACON_FOUND:
        case EV_BEACON_MISSED:
        case EV_BEACON_TRACKED:
        case EV_RFU1:                    // This event is defined but not used in code
        case EV_JOINING:        
        case EV_JOIN_FAILED:           
        case EV_REJOIN_FAILED:
        case EV_LOST_TSYNC:
        case EV_RESET:
        case EV_RXCOMPLETE:
        case EV_LINK_DEAD:
        case EV_LINK_ALIVE:
#ifdef MCCI_LMIC
        // Only supported in MCCI LMIC library:
        case EV_SCAN_FOUND:              // This event is defined but not used in code 
#endif   
            break;

        default:    
            break;
    }
}


static void doWorkCallback(osjob_t* job)
{
    // Event hander for doWorkJob. Gets called by the LMIC scheduler.
    // The actual work is performed in function processWork() which is called below.

    ostime_t timestamp = os_getTime();
   

    // Do the work that needs to be performed.
    processWork(timestamp);

    // This job must explicitly reschedule itself for the next run.
    ostime_t startAt = timestamp + sec2osticks((int64_t)doWorkIntervalSeconds);
    os_setTimedCallback(&doWorkJob, startAt, doWorkCallback);    
}


lmic_tx_error_t scheduleUplink(uint8_t fPort, uint8_t* data, uint8_t dataLength, bool confirmed = false)
{
    // This function is called from the processWork() function to schedule
    // transmission of an uplink message that was prepared by processWork().
    // Transmission will be performed at the next possible time

    ostime_t timestamp = os_getTime();

    lmic_tx_error_t retval = LMIC_setTxData2(fPort, data, dataLength, confirmed ? 1 : 0);
    timestamp = os_getTime();

    if (retval == LMIC_ERROR_SUCCESS)
    {
        #ifdef CLASSIC_LMIC
            // For MCCI_LMIC this will be handled in EV_TXSTART        
        #endif        
    }
    else
    {
        String errmsg; 
        //    
    }
    return retval;    
}


//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▄ █▀▀ █▀▀ ▀█▀ █▀█
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▄ █▀▀ █ █  █  █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀

void hanBlink()
{
    #ifdef ESP8266
    digitalWrite(2, LOW);
    delay(150);
    digitalWrite(2, HIGH);
    #endif
}

void errorBlink()
{
    #ifdef ESP8266
    digitalWrite(2, LOW);
    delay(1000);
    digitalWrite(2, HIGH);
    delay(1000);
    digitalWrite(2, LOW);
    delay(1000);
    digitalWrite(2, HIGH);
    delay(1000);
    digitalWrite(2, LOW);
    delay(1000);
    digitalWrite(2, HIGH);
    delay(1000);
    #endif
}

void processWork(ostime_t doWorkJobTimeStamp)
{
    // This function is called from the doWorkCallback() 
    // callback function when the doWork job is executed.

    // Uses globals: payloadBuffer and LMIC data structure.

    // This is where the main work is performed like
    // reading sensor and GPS data and schedule uplink
    // messages if anything needs to be transmitted.

    // # # # # # # # # # #
    // EASYHAN MODBUS BEGIN
    // # # # # # # # # # #

    uint8_t result;

    // # # # # # # # # # #
    // Clock ( 12 bytes )
    // # # # # # # # # # #

    result = node.readInputRegisters(0x0001, 1);
    if (result == node.ku8MBSuccess)
    {
      hanBlink();
      hanYY = node.getResponseBuffer(0);
      hanMT = node.getResponseBuffer(1) >> 8;
      hanDD = node.getResponseBuffer(1) & 0xFF;
      hanWD = node.getResponseBuffer(2) >> 8;
      hanHH = node.getResponseBuffer(2) & 0xFF;
      hanMM = node.getResponseBuffer(3) >> 8;
      hanSS = node.getResponseBuffer(3) & 0xFF;
    }
    else
    {
      hanERR++;
    }
    delay(1000);

    // # # # # # # # # # #
    // Voltage Current
    // # # # # # # # # # #

    if (hanEB == 3)
    {
      result = node.readInputRegisters(0x006c, 7);
      if (result == node.ku8MBSuccess)
      {
        hanBlink();
        hanVL1 = node.getResponseBuffer(0);
        hanCL1 = node.getResponseBuffer(1);
        hanVL2 = node.getResponseBuffer(2);
        hanCL2 = node.getResponseBuffer(3);
        hanVL3 = node.getResponseBuffer(4);
        hanCL3 = node.getResponseBuffer(5);
        hanCLT = node.getResponseBuffer(6);
      }
      else
      {
      hanERR++;
      }
    }
    else
    {
      result = node.readInputRegisters(0x006c, 2);
      if (result == node.ku8MBSuccess)
      {
        hanBlink();
        hanVL1 = node.getResponseBuffer(0);
        hanCL1 = node.getResponseBuffer(1);
      }
      else
      {
        hanERR++;
      }
    }
    delay(1000);

    // # # # # # # # # # #
    // Active Power Import/Export 73 (tri)
    // Power Factor (mono) (79..)
    // # # # # # # # # # #

    if (hanEB == 3)
    {
      result = node.readInputRegisters(0x0073, 8);
      if (result == node.ku8MBSuccess)
      {
        hanBlink();
        hanAPI1 = node.getResponseBuffer(1) | node.getResponseBuffer(0) << 16;
        hanAPE1 = node.getResponseBuffer(3) | node.getResponseBuffer(2) << 16;
        hanAPI2 = node.getResponseBuffer(5) | node.getResponseBuffer(4) << 16;
        hanAPE2 = node.getResponseBuffer(7) | node.getResponseBuffer(6) << 16;
        hanAPI3 = node.getResponseBuffer(9) | node.getResponseBuffer(8) << 16;
        hanAPE3 = node.getResponseBuffer(11) | node.getResponseBuffer(10) << 16;
        hanAPI = node.getResponseBuffer(13) | node.getResponseBuffer(12) << 16;
        hanAPE = node.getResponseBuffer(15) | node.getResponseBuffer(14) << 16;
      }
      else
      {
        hanERR++;
      }
    }
    else
    {
      result = node.readInputRegisters(0x0079, 3);
      if (result == node.ku8MBSuccess)
      {
        hanBlink();
        hanAPI = node.getResponseBuffer(1) | node.getResponseBuffer(0) << 16;
        hanAPE = node.getResponseBuffer(3) | node.getResponseBuffer(2) << 16;
        hanPF = node.getResponseBuffer(4);
      }
      else
      {
        hanERR++;
      }
    }
    delay(1000);

    // # # # # # # # # # #
    // Power Factor (7B) / Frequency (7F)
    // Power Factor (tri)
    // Frequency (mono)
    // # # # # # # # # # #

    if (hanEB == 3)
    {
      result = node.readInputRegisters(0x007b, 5);
      if (result == node.ku8MBSuccess)
      {
        hanBlink();
        hanPF = node.getResponseBuffer(0);
        hanPF1 = node.getResponseBuffer(1);
        hanPF2 = node.getResponseBuffer(2);
        hanPF3 = node.getResponseBuffer(3);
        hanFreq = node.getResponseBuffer(4);
      }
      else
      {
        hanERR++;
      }
    }
    else
    {
      result = node.readInputRegisters(0x007f, 1);
      if (result == node.ku8MBSuccess)
      {
        hanBlink();
        hanFreq = node.getResponseBuffer(0);
      }
      else
      {
        hanERR++;
      }
    }
    delay(1000); 

    // # # # # # # # # # #
    // Total Energy Tarifas (kWh) 26
    // # # # # # # # # # #

    result = node.readInputRegisters(0x0026, 3);
    if (result == node.ku8MBSuccess)
    {
      hanBlink();
      hanTET1 = node.getResponseBuffer(1) | node.getResponseBuffer(0) << 16;
      hanTET2 = node.getResponseBuffer(3) | node.getResponseBuffer(2) << 16;
      hanTET3 = node.getResponseBuffer(5) | node.getResponseBuffer(4) << 16;
    }
    else
    {
      hanERR++;
    }
    delay(1000);

    // # # # # # # # # # #
    // Total Energy (total) (kWh) 16
    // # # # # # # # # # #

    result = node.readInputRegisters(0x0016, 2);
    if (result == node.ku8MBSuccess)
    {
      hanBlink();
      hanTEI = node.getResponseBuffer(1) | node.getResponseBuffer(0) << 16;
      hanTEE = node.getResponseBuffer(3) | node.getResponseBuffer(2) << 16;
    }
    else
    {
      hanERR++;
    }
    delay(1000);


    // # # # # # # # # # #
    // EASYHAN MODBUS EOF
    // # # # # # # # # # #

    // Skip processWork if using OTAA and still joining.
    if (LMIC.devaddr != 0)
    {
        // Collect input data.

        ostime_t timestamp = os_getTime();

        // For simplicity LMIC-node will try to send an uplink
        // message every time processWork() is executed.

        // Schedule uplink message if possible
        if (LMIC.opmode & OP_TXRXPEND)
        {
            // do nothing
            txFail++;
            errorBlink();
        }
        else
        {
            // # # # # # # # # # #
            // # # # # # # # # # #
            // # # # # # # # # # #
            if (hanCNT == 1)
            {	
              uint8_t fPort = 70;
              payloadBuffer[0] = hanHH;
              payloadBuffer[1] = hanMM;
              payloadBuffer[2] = hanSS;
              // 
              payloadBuffer[3] = hanVL1 >> 8;
              payloadBuffer[4] = hanVL1 & 0xFF;
              payloadBuffer[5] = hanCL1 >> 8;
              payloadBuffer[6] = hanCL1 & 0xFF;
              payloadBuffer[7] = hanVL2 >> 8;
              payloadBuffer[8] = hanVL2 & 0xFF;
              payloadBuffer[9] = hanCL2 >> 8;
              payloadBuffer[10] = hanCL2 & 0xFF;
              payloadBuffer[11] = hanVL3 >> 8;
              payloadBuffer[12] = hanVL3 & 0xFF;
              payloadBuffer[13] = hanCL3 >> 8;
              payloadBuffer[14] = hanCL3 & 0xFF;
              payloadBuffer[15] = hanCLT >> 8;
              payloadBuffer[16] = hanCLT & 0xFF;
              // 
              payloadBuffer[17] = hanFreq >> 8;
              payloadBuffer[18] = hanFreq & 0xFF;
              //
              payloadBuffer[19] = hanPF >> 8;
              payloadBuffer[20] = hanPF & 0xFF;
              payloadBuffer[21] = hanPF1 >> 8;
              payloadBuffer[22] = hanPF1 & 0xFF;
              payloadBuffer[23] = hanPF2 >> 8;
              payloadBuffer[24] = hanPF2 & 0xFF;
              payloadBuffer[25] = hanPF3 >> 8;
              payloadBuffer[26] = hanPF3 & 0xFF;

              uint8_t payloadLength = 27;
              scheduleUplink(fPort, payloadBuffer, payloadLength);
            }
            else if (hanCNT == 2)
            {
              uint8_t fPort = 71;
              payloadBuffer[0] = hanHH;
              payloadBuffer[1] = hanMM;
              payloadBuffer[2] = hanSS;
              // 
              payloadBuffer[3] = (hanAPI & 0xFF000000) >> 24;
              payloadBuffer[4] = (hanAPI & 0x00FF0000) >> 16;
              payloadBuffer[5] = (hanAPI & 0x0000FF00) >> 8;
              payloadBuffer[6] = (hanAPI & 0X000000FF);
              // 
              payloadBuffer[7] = (hanAPE & 0xFF000000) >> 24;
              payloadBuffer[8] = (hanAPE & 0x00FF0000) >> 16;
              payloadBuffer[9] = (hanAPE & 0x0000FF00) >> 8;
              payloadBuffer[10] = (hanAPE & 0X000000FF);
              // 32bits to 16bits
              payloadBuffer[11] = (hanAPI1 & 0x0000FF00) >> 8;
              payloadBuffer[12] = (hanAPI1 & 0X000000FF);
              payloadBuffer[13] = (hanAPE1 & 0x0000FF00) >> 8;
              payloadBuffer[14] = (hanAPE1 & 0X000000FF);
              //
              payloadBuffer[15] = (hanAPI2 & 0x0000FF00) >> 8;
              payloadBuffer[16] = (hanAPI2 & 0X000000FF);
              payloadBuffer[17] = (hanAPE2 & 0x0000FF00) >> 8;
              payloadBuffer[18] = (hanAPE2 & 0X000000FF);
              //
              payloadBuffer[19] = (hanAPI3 & 0x0000FF00) >> 8;
              payloadBuffer[20] = (hanAPI3 & 0X000000FF);
              payloadBuffer[21] = (hanAPE3 & 0x0000FF00) >> 8;
              payloadBuffer[22] = (hanAPE3 & 0X000000FF);

              uint8_t payloadLength = 23;
              scheduleUplink(fPort, payloadBuffer, payloadLength);
            }
            else if (hanCNT == 3)
            {
              uint8_t fPort = 72;
              payloadBuffer[0] = hanHH;
              payloadBuffer[1] = hanMM;
              payloadBuffer[2] = hanSS;
              // 
              payloadBuffer[3] = (hanTET1 & 0xFF000000) >> 24;
              payloadBuffer[4] = (hanTET1 & 0x00FF0000) >> 16;
              payloadBuffer[5] = (hanTET1 & 0x0000FF00) >> 8;
              payloadBuffer[6] = (hanTET1 & 0X000000FF);
              // 
              payloadBuffer[7] = (hanTET2 & 0xFF000000) >> 24;
              payloadBuffer[8] = (hanTET2 & 0x00FF0000) >> 16;
              payloadBuffer[9] = (hanTET2 & 0x0000FF00) >> 8;
              payloadBuffer[10] = (hanTET2 & 0X000000FF);
              // 
              payloadBuffer[11] = (hanTET3 & 0xFF000000) >> 24;
              payloadBuffer[12] = (hanTET3 & 0x00FF0000) >> 16;
              payloadBuffer[13] = (hanTET3 & 0x0000FF00) >> 8;
              payloadBuffer[14] = (hanTET3 & 0X000000FF);
              // 
              payloadBuffer[15] = (hanTEI & 0xFF000000) >> 24;
              payloadBuffer[16] = (hanTEI & 0x00FF0000) >> 16;
              payloadBuffer[17] = (hanTEI & 0x0000FF00) >> 8;
              payloadBuffer[18] = (hanTEI & 0X000000FF);
              //
              payloadBuffer[19] = (hanTEE & 0xFF000000) >> 24;
              payloadBuffer[20] = (hanTEE & 0x00FF0000) >> 16;
              payloadBuffer[21] = (hanTEE & 0x0000FF00) >> 8;
              payloadBuffer[22] = (hanTEE & 0X000000FF);

              uint8_t payloadLength = 23;
              scheduleUplink(fPort, payloadBuffer, payloadLength);
            }
            else if (hanCNT == 4)
            {
              uint8_t fPort = 73;
              payloadBuffer[0] = hanHH;
              payloadBuffer[1] = hanMM;
              payloadBuffer[2] = hanSS;
              // 
              payloadBuffer[3] = hanERR >> 8;
              payloadBuffer[4] = hanERR & 0xFF;
              // 
              payloadBuffer[5] = hanEB;
              payloadBuffer[6] = hanCFG;
              // 
              payloadBuffer[7] = txFail >> 8;
              payloadBuffer[8] = txFail & 0xFF;

              uint8_t payloadLength = 9;
              scheduleUplink(fPort, payloadBuffer, payloadLength);
            }
            // # # # # # # # # # #
            // # # # # # # # # # #
            // # # # # # # # # # #
        // LMIC.opmode EOF
        }
    // LMIC.devaddr if EOF
    }
  // 
  if (hanCNT == 4)
  {
    hanCNT = 1;
  }
  else
  {
    hanCNT++;
  }
  //
  if (hanERR > 900 )
  {
    hanERR = 0;
  }
  //
  if (txFail > 900 )
  {
    txFail = 0;
  }
// EOF processWork
}    
 

void processDownlink(ostime_t txCompleteTimestamp, uint8_t fPort, uint8_t* data, uint8_t dataLength)
{
    if (fPort == 99 && dataLength == 1 && data[0] == 0xC0)
    {
        ostime_t timestamp = os_getTime();
        delay(5000);
        delay(5000);
        ESP.restart();
    }
    else if (fPort == 99 && dataLength == 1 && data[0] == 0xB1)
    {
        ostime_t timestamp = os_getTime();
        // 
        delay(5000);
        delay(5000);
        ESP.restart();
    }
    else if (fPort == 99 && dataLength == 1 && data[0] == 0xB2)
    {
        ostime_t timestamp = os_getTime();
        // 
        delay(5000);
        delay(5000);
        ESP.restart();
    }
}

void setup() 
{
    delay(1000);

    initLmic();

    delay(1000);

    // Place code for initializing sensors etc. here.

    #ifdef ESP8266
    pinMode(2, OUTPUT);
    #endif

    // modbus

    delay(1000);

    Serial.begin(9600, SERIAL_8N1);

    while (!Serial);

    node.begin(1, Serial);

    // detect stop bits

    uint8_t testserial;

    testserial = node.readInputRegisters(0x001, 1);
    if (testserial == node.ku8MBSuccess)
    {
      hanCFG = 1;
    }
    else
    {
      Serial.end();
      delay(500);
      Serial.begin(9600, SERIAL_8N2);
      hanCFG = 2;
    }

    // Detect EB Type

    delay(1000);

    testserial = node.readInputRegisters(0x0070, 2);
    if (testserial == node.ku8MBSuccess)
    {
      // 
      hanDTT = node.getResponseBuffer(0);
      if (hanDTT > 0)
      {
        hanEB = 3;
      }
      else
      {
        hanEB = 1;
      }
      // 
    }
    else
    {
      hanEB = 1;
    }
    delay(1000);

    // # # # # # # # # # #

    // otaa
    if (activationMode == ActivationMode::OTAA)
    {
        LMIC_startJoining();
    }

    // Schedule initial doWork job for immediate execution.
    os_setCallback(&doWorkJob, doWorkCallback);
}

void loop() 
{
    os_runloop_once();
}

// EOF
