/**
 * Program to read Geeney outside temperature gauge.
 */

#include <Arduino.h>

#include <WiFi.h>
#include "lwip/apps/sntp.h"

#include "config.h"
#include "CC1101_RFx.h"

static bool enableWiFi(void);
static void disableWiFi(void);
static void ntpTime(void);
static void update_local_time(void);
static void radio_setup();

#define _DEBUG_

// Variables
char time_buffer[10];

typedef enum { // Keep track of current state of the device
    OT_ERROR,
    OT_INITIALISING,
    OT_READY
} ot_state_t;
ot_state_t state = OT_INITIALISING;

// CC1101 Setup
// ESP32: SCK_PIN = 18; MISO_PIN = 19; MOSI_PIN = 23; SS_PIN = 5; GDO0 = 2;
#define SS_PIN 5
#define MISO_PIN 19
CC1101 radio(SS_PIN,  MISO_PIN);

void setup() {
#if defined(_DEBUG_)
    Serial.begin(115200);
    delay(5000); // delay for serial to begin, can be very slow to start serial output!

    Serial.println("\n##################################");
    Serial.println(F("ESP32 Information:"));
    Serial.printf("Internal Total Heap %d, Internal Used Heap %d, Internal Free Heap %d\n", ESP.getHeapSize(),
    ESP.getHeapSize()-ESP.getFreeHeap(), ESP.getFreeHeap()); Serial.printf("Sketch Size %d, Free Sketch Space %d\n",
    ESP.getSketchSize(), ESP.getFreeSketchSpace()); Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n",
    ESP.getPsramSize(), ESP.getFreePsram()); Serial.printf("Chip Model %s, ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion()); 
    Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
    Serial.println("##################################\n");
#endif

    // Enable wifi so we can retrieve the real time and update our local time.
    if (enableWiFi() == true) {
        // ipAddress = WiFi.localIP().toString();
        // Serial.printf("IP Address: %s\n", ipAddress);
        #if defined(_DEBUG_) 
            Serial.printf("\nIP Address: ");
            Serial.println(WiFi.localIP());
        #endif

        //ntpTime();
        configTime(0, 3600, SNTP_TIME_SERVER);
        //printLocalTime();
        update_local_time();

        #if defined(_DEBUG_)
            Serial.print("Time setup complete - ");
            Serial.println(time_buffer);
        #endif

        delay(500);

        state = OT_READY;
    } else {
        state = OT_ERROR;
        #if defined(_DEBUG_)
            Serial.print("Unable to connect to WiFi: ");
            Serial.println(SSID);
        #endif
    }

    // Set up the radio
    //radio_setup();    

    uint32_t freq = 433.920e6;
    #define CC1101_CRYSTAL_FREQUENCY 26000000ul

    // We use uint64_t as the <<16 overflows uint32_t
    // however the division with 26000000 allows the final
    // result to be uint32 again
    uint32_t reg_freq = ((uint64_t)freq<<16) / CC1101_CRYSTAL_FREQUENCY;
    //
    // this is split into 3 bytes that are written to 3 different registers on the CC1101
    uint8_t FREQ2 = (reg_freq>>16) & 0xFF;   // high byte, bits 7..6 are always 0 for this register
    uint8_t FREQ1 = (reg_freq>>8) & 0xFF;    // middle byte
    uint8_t FREQ0 = reg_freq & 0xFF;         // low byte

    Serial.print("FREQ2=");Serial.println(FREQ2, HEX);
    Serial.print("FREQ1=");Serial.println(FREQ1, HEX);
    Serial.print("FREQ0=");Serial.println(FREQ0,HEX);
    
    uint32_t realfreq=((uint32_t)FREQ2<<16)+((uint32_t)FREQ1<<8)+(uint32_t)FREQ0;
    realfreq=((uint64_t)realfreq*CC1101_CRYSTAL_FREQUENCY)>>16;
    Serial.print("Real frequency = ");
    Serial.println(realfreq);

    /*
        Output from  above frequency setting code:
            FREQ2=10
            FREQ1=B0
            FREQ0=71
            Real frequency = 433919830        
    */
}


void loop() {

}


/**
 * @brief Enable WiFi
 *
 * @return true if connected, else false
 */
static bool enableWiFi(void) {
    bool connected = true;

    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA); // switch off AP
    WiFi.setAutoReconnect(true);

    WiFi.begin(SSID, WIFI_PASSWORD);
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        #if defined (_DEBUG_)
            Serial.print(".");
        #endif
    }

    if (WiFi.status() != WL_CONNECTED) {
        connected = false;
    }

    return connected;
}

/**
 * @brief Disable WiFi
 *
 */
static void disableWiFi(void) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    #if defined(_DEBUG_)
        Serial.println("WiFi mode set to WIFI_OFF");
    #endif
}

/**
 * @brief Connect to NTP time server for accurate time.
 *
 */
static void ntpTime(void) {
    // According to various forums configTime on the ESP32 does not honor the TZ env
    // configTime(0, 0, SNTP_TIME_SERVER);

    // Set timezone - London for us
    setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0", 1);
    tzset();

    // configTime on the ESP32 does not honor the TZ env, unlike the ESP8266

    sntp_stop();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    sntp_setservername(0, SNTP_TIME_SERVER);

    sntp_init();

    //update_local_time();

    #if defined(_DEBUG_)
        Serial.println("Time set...");
    #endif
}


static void update_local_time(void) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        #if defined(_DEBUG_)
            Serial.println("Failed to obtain time");
        #endif
        return;
    }

    // Update buffer with current time
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &timeinfo);
}


/**
 * @brief Set up the CC1101 for receiving geevon (Nexus-TH) packets 
 * 
 */
void radio_setup() {
    radio.reset();
    radio.begin(433.920e6); // Freq, do not forget the "e6"
    radio.setMaxPktSize(61);
    radio.writeRegister(CC1101_FREQ2, 0x10); 
    radio.writeRegister(CC1101_FREQ1, 0xB0);   // 10B071 = real freq. 433919830
    radio.writeRegister(CC1101_FREQ0, 0x71);
    radio.writeRegister(CC1101_FSCTRL1, 0x08); // fif=203.125kHz
    radio.writeRegister(CC1101_FSCTRL0, 0x00); // No offset
    radio.writeRegister(CC1101_MDMCFG4, 0x5B); // CHANBW_E = 1 CHANBW_M=1 BWchannel =325kHz   DRATE_E=11
    radio.writeRegister(CC1101_MDMCFG3, 0xF8); // DRATE_M=248 RDATA=99.975kBaud
    radio.writeRegister(CC1101_MDMCFG2, 0x03); // Disable digital DC blocking filter before demodulator enabled. MOD_FORMAT=000 (2-FSK) Manchester Coding disabled Combined sync-word qualifier mode = 30/32 sync word bits detected
    radio.writeRegister(CC1101_MDMCFG1, 0x22); // Forward error correction disabled 4 preamble bytes transmitted CHANSPC_E=2
    radio.writeRegister(CC1101_MDMCFG0, 0xF8); // CHANSPC_M=248 200kHz channel spacing
    radio.writeRegister(CC1101_CHANNR, 0x00); // The 8-bit unsigned channel number, which is multiplied by the channel spacing setting and added to the base frequency.
    radio.writeRegister(CC1101_DEVIATN, 0x47); // DEVIATION_E=4 DEVIATION_M=7 ±47.607 kHz deviation
    radio.writeRegister(CC1101_FREND1, 0xB6); // Adjusts RX RF device
    radio.writeRegister(CC1101_FREND0, 0x10); // Adjusts TX RF device
    radio.writeRegister(CC1101_MCSM0, 0x18); // Calibrates whngoing from IDLE to RX or TX (or FSTXON) PO_TIMEOUT 149-155uS Pin control disabled XOSC off in sleep mode
    //radio.writeRegister(CC1101_MCSM1, 0x00); // Channel clear = always Return to idle after packet reception Return to idle after transmission
    radio.writeRegister(CC1101_FOCCFG, 0x1D); // The frequency compensation loop gain to be used before a sync word is detected = 4K The frequency compensation loop gain to be used after a sync word is Detected = K/2 The saturation point for the frequency offset compensation algorithm = ±BWchannel /8
    radio.writeRegister(CC1101_BSCFG, 0x1C); // The clock recovery feedback loop integral gain to be used before a sync word is detected = KI The clock recovery feedback loop proportional gain to be used before a sync word is detected = 2KP The clock recovery feedback loop integral gain to be used after a sync word is Detected = KI/2 The clock recovery feedback loop proportional gain to be used after a sync word is detected = KP The saturation point for the data rate offset compensation algorithm = ±0 (No data rate offset compensation performed)

    // Mode = ASK_OOK
    radio.writeRegister(CC1101_AGCCTRL2, 0x07); // MAGN_TARGET (DN022: 0x03-0x07)
    radio.writeRegister(CC1101_AGCCTRL1, 0x00); // AGC_LNA_PRIORITY = 0
    radio.writeRegister(CC1101_AGCCTRL0, 0x91); // 8 dB ASK decision boundary
    // radio.writeRegister(CC1101_AGCCTRL2, 0xC7); // The 3 highest DVGA gain settings can not be used. Maximum allowable LNA + LNA 2 gain relative to the maximum possible gain. Target value for the averaged amplitude from the digital channel filter = 42dB
    // radio.writeRegister(CC1101_AGCCTRL1, 0x00); // LNA 2 gain is decreased to minimum before decreasing LNA gain Relative carrier sense threshold disabled Sets the absolute RSSI threshold for asserting carrier sense to MAGN_TARGET
    // radio.writeRegister(CC1101_AGCCTRL0, 0xB2); // Sets the level of hysteresis on the magnitude deviation (internal AGC signal that determine gain changes) to Medium hysteresis, medium asymmetric dead zone, medium gain Sets the number of channel filter samples from a gain adjustment has been made until the AGC algorithm starts accumulating new samples to 32 samples AGC gain never frozen

    radio.writeRegister(CC1101_FSCAL3, 0xEA); // Detailed calibration
    radio.writeRegister(CC1101_FSCAL2, 0x2A); //
    radio.writeRegister(CC1101_FSCAL1, 0x00); //
    radio.writeRegister(CC1101_FSCAL0, 0x1F); //
    radio.writeRegister(CC1101_FSTEST, 0x59); // Test register
    radio.writeRegister(CC1101_TEST2, 0x81); // Values to be used from SmartRF software
    radio.writeRegister(CC1101_TEST1, 0x35); //
    radio.writeRegister(CC1101_TEST0, 0x09); //
    radio.writeRegister(CC1101_IOCFG2, 0x0B); // Active High Serial Clock
    radio.writeRegister(CC1101_IOCFG0, 0x46); // Analog temperature sensor disabled Active High Asserts when sync word has been sent / received, and de-asserts at the end of the packet
    radio.writeRegister(CC1101_PKTCTRL1, 0x04); // Sync word is always accepted Automatic flush of RX FIFO when CRC is not OK disabled Two status bytes will be appended to the payload of the packet. The status bytes contain RSSI and LQI values, as well as CRC OK. No address checkof received packages.
    radio.writeRegister(CC1101_PKTCTRL0, 0x05); // Data whitening off Normal mode, use FIFOs for RX and TX CRC calculation in TX and CRC check in RX enabled Variable packet length mode. Packet length configured by the first byte after sync word
    radio.writeRegister(CC1101_ADDR, 0x00); // Address used for packet filtration. Optional broadcast addresses are 0 (0x00) and 255 (0xFF).

    static uint8_t paTable[] = {0xC6, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F};
    radio.writeBurstRegister(CC1101_PATABLE, paTable, sizeof(paTable));

    radio.strobe(CC1101_SIDLE); 
    radio.strobe(CC1101_SPWD); 

    radio.setIDLEstate();                 // Was set to receive, moved so set when all setup of program is finished

    ESP_LOGI(TAG, "CC1101 set up complete, radio set to idle state");
}
