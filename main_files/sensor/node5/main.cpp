#include <stdio.h>
#include <stdarg.h>
#include <Arduino.h>
#include <lmic.h>
#include <hal/arduino_hal.h>
#include <SPI.h>
#include <ed25519.h>
#include <base64_esp.h>


// Pin mapping
const lmic_pinmap lmic_pins = {
        .mosi = 27,
        .miso = 19,
        .sck = 5,
        .nss = 18,
        .rxtx = LMIC_UNUSED_PIN,
        .rst = LMIC_UNUSED_PIN,
        .dio = {23, 23, 23}, //workaround to use 1 pin for all 3 radio dio pins
};

/*General Config*/
#define RXD2 15
#define TXD2 4
#define DHTPIN 13
#define PMVCC 21
#define MAXTIMINGS 85
#define MAXTRIES 5
#define GPIO_DEEP_SLEEP_DURATION 232        //Time ESP32 will go to sleep (in seconds)



const unsigned char pub[32] = { 9 ,6 ,173 ,177 ,235 ,16 ,42 ,159 ,170 ,69 ,52 ,15 ,176 ,5 ,83 ,65 ,216 ,58 ,36 ,16 ,236 ,32 ,138 ,125 ,137 ,160 ,160 ,177 ,99 ,241 ,61 ,74 };
const unsigned char priv[64] = { 16 ,120 ,71 ,38 ,69 ,115 ,202 ,23 ,209 ,225 ,244 ,198 ,172 ,25 ,2 ,246 ,139 ,166 ,79 ,255 ,40 ,194 ,181 ,101 ,82 ,72 ,37 ,160 ,209 ,71 ,220 ,88 ,99 ,153 ,219 ,108 ,108 ,185 ,23 ,183 ,245 ,207 ,35 ,53 ,229 ,112 ,34 ,222 ,103 ,219 ,103 ,96 ,179 ,174 ,209 ,48 ,10 ,133 ,71 ,124 ,156 ,97 ,214 ,147  };
unsigned int Pm25 = 0;
unsigned int Pm10 = 0;
uint8_t dht22_dat[5] = {0,0,0,0,0};
char timestamp[6] = "";
char lng[12] = "";
char lat[11] = "";

extern "C" {
void app_main(void);
}

// randombytes
static bool rand_1(void *dest, size_t size) {
        // Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of
        // random noise). This can take a long time to generate random data if the result of analogRead(0)
        // doesn't change very frequently.
        unsigned char *p = (unsigned char*) dest;
        while (size) {
                uint8_t val = 0;
                for (unsigned i = 0; i < 8; ++i) {
                        int init = analogRead(32);
                        int count = 0;
                        while (analogRead(32) == init) {
                                ++count;
                        }

                        if (count == 0) {
                                val = (val << 1) | (init & 0x01);
                        } else {
                                val = (val << 1) | (count & 0x01);
                        }
                }
                *p = val;
                ++p;
                --size;
        }
        // NOTE: it would be a good idea to hash the resulting random data using SHA-256 or similar.
        return true;
}

static uint8_t sizecvt(const int read)
{
        /* digitalRead() and friends from wiringpi are defined as returning a value
           < 256. However, they are returned as int() types. This is a safety function */

        if (read > 255 || read < 0)
        {
                exit(EXIT_FAILURE);
        }
        return (uint8_t)read;
}

int read_dht22_dat(unsigned char *msg)
{
        uint8_t laststate = HIGH;
        uint8_t counter = 0;
        uint8_t j = 0, i;

        dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

        // pull pin down for 18 milliseconds
        pinMode(DHTPIN, OUTPUT);
        digitalWrite(DHTPIN, HIGH);
        delay(500);
        digitalWrite(DHTPIN, LOW);
        delay(20);
        // prepare to read the pin
        pinMode(DHTPIN, INPUT);

        // detect change and read data
        for ( i=0; i< MAXTIMINGS; i++) {
                counter = 0;
                while (sizecvt(digitalRead(DHTPIN)) == laststate) {
                        counter++;
                        delayMicroseconds(2);
                        if (counter == 255) {
                                break;
                        }
                }
                laststate = sizecvt(digitalRead(DHTPIN));

                if (counter == 255) break;

                // ignore first 3 transitions
                if ((i >= 4) && (i%2 == 0)) {
                        // shove each bit into the storage bytes
                        dht22_dat[j/8] <<= 1;
                        if (counter > 16)
                                dht22_dat[j/8] |= 1;
                        j++;
                }
        }

        // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
        // print it out if data is good
        if ((j >= 40) &&
            (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
                msg[23] = (unsigned char) dht22_dat[1];
                msg[24] = (unsigned char) dht22_dat[0];
                msg[25] = (unsigned char) dht22_dat[3];
                msg[26] = (unsigned char) dht22_dat[2];
                return 1;
        }
        else
        {
                return 0;
        }
}

void resetGPSData() {
        memset(lng,'0',sizeof(lng));
        memset(lat,'0',sizeof(lat));
        memset(timestamp,'0',sizeof(timestamp));
}

// source from: https://github.com/FriskByBergen/SDS011/blob/master/arduino.ino
int readPmData(unsigned char *msg) {
        uint8_t mData = 0;
        uint8_t i = 0;
        uint8_t mPkt[10] = {0};
        uint8_t mCheck = 0;
        while (Serial2.available() > 0) {
                // from www.inovafitness.com
                // packet format: AA C0 PM25_Low PM25_High PM10_Low PM10_High 0 0 CRC AB
                mData = Serial2.read();
                delay(2);//wait until packet is received
                if (mData == 0xAA) {
                        mPkt[0] =  mData;
                        mData = Serial2.read();
                        if (mData == 0xc0) {
                                mPkt[1] =  mData;
                                mCheck = 0;
                                for (i = 0; i < 6; i++) //data recv and crc calc
                                {
                                        mPkt[i + 2] = Serial2.read();
                                        delay(2);
                                        mCheck += mPkt[i + 2];
                                }
                                mPkt[8] = Serial2.read();
                                delay(1);
                                mPkt[9] = Serial2.read();
                                //checksum validation
                                if (mCheck == mPkt[8]) {
                                        msg[19] = (unsigned char) mPkt[4];
                                        msg[20] = (unsigned char) mPkt[5];
                                        msg[21] = (unsigned char) mPkt[2];
                                        msg[22] = (unsigned char) mPkt[3];
                                        return 1;
                                } else {
                                        return 0;
                                }
                        }
                }
        }
        return 0;
}

void readGPSData() {
        unsigned long start = millis();
        unsigned long stop = millis();
        while((stop-start) < 60000) {
                if(Serial.available()) {
                        char c = Serial.read();
                        if (c=='$') {
                                char c2 = Serial.read();
                                if(c2=='G') {
                                        char c3 = Serial.read();
                                        if(c3=='P') {
                                                char c4 = Serial.read();
                                                if(c4=='G') {
                                                        char c5 = Serial.read();
                                                        if(c5=='G') {
                                                                Serial.read();
                                                                Serial.read();
                                                                //reading the timestamp
                                                                for(int i=0; i<6; i++) {
                                                                        timestamp[i] = Serial.read();
                                                                        if(timestamp[i]==',') {
                                                                                resetGPSData();
                                                                                stop = millis();
                                                                                break;
                                                                        }
                                                                }
                                                                Serial.read();
                                                                Serial.read();
                                                                Serial.read();
                                                                Serial.read();
                                                                //reading the latitude
                                                                char c6 = Serial.read();
                                                                if(c6==',') {
                                                                        stop = millis();
                                                                        break;
                                                                }
                                                                for(int i=0; i<5; i++) {
                                                                        if(c6=='-' && i==0) {
                                                                                lat[i] = '-';
                                                                        } else if(i==0) {
                                                                                lat[i] = '0';
                                                                                lat[i+1] = c6;
                                                                                i++;
                                                                        } else {
                                                                                lat[i] = Serial.read();
                                                                        }
                                                                }
                                                                Serial.read();
                                                                for(int i=5; i<10; i++) {
                                                                        lat[i] = Serial.read();
                                                                }
                                                                Serial.read();
                                                                lat[10] = Serial.read();
                                                                Serial.read();
                                                                //reading the longtitude
                                                                char c7 = Serial.read();
                                                                for(int i=0; i<6; i++) {
                                                                        if(c7=='-' && i==0) {
                                                                                lng[i] = '-';
                                                                        } else if(i==0) {
                                                                                lng[i] = '0';
                                                                                lng[i+1] = c7;
                                                                                i++;
                                                                        } else {
                                                                                lng[i] = Serial.read();
                                                                        }
                                                                }
                                                                Serial.read();
                                                                for(int i=6; i<11; i++) {
                                                                        lng[i] = Serial.read();
                                                                }
                                                                Serial.read();
                                                                lng[11] = Serial.read();
                                                                Serial.read();
                                                                char c8 = Serial.read();
                                                                if(c8=='1') {
                                                                        stop = millis();
                                                                        if((stop-start) > 60000) {
                                                                                return;
                                                                        }
                                                                        delay(60000-(stop-start));
                                                                        return;
                                                                }
                                                        }
                                                }
                                        }
                                }
                        }
                }
                stop = millis();
        }
}

int calculateB64size(unsigned int input_size) {
        unsigned int adjustment = ( (input_size % 3) ? (3 - (input_size % 3)) : 0);
        unsigned int code_padded_size = ( (input_size + adjustment) / 3) * 4;
        unsigned int newline_size = ((code_padded_size) / 72) * 2;
        int total_size = code_padded_size + newline_size;
        return total_size;
}

void sendLoraPacket(const char *str, char flag) {
        os_radio(RADIO_RST); // Stop RX first
        delay(1); // Wait a bit, without this os_radio below asserts, apparently because the state hasn't changed yet

        LMIC.dataLen = 0;
        LMIC.frame[LMIC.dataLen++] = '<';
        if(flag=='x') {
          LMIC.frame[LMIC.dataLen++] = '1';
          LMIC.frame[LMIC.dataLen++] = '0';
        } else {
          LMIC.frame[LMIC.dataLen++] = flag;
        }
        LMIC.frame[LMIC.dataLen++] = '>';
        while (*str)
                LMIC.frame[LMIC.dataLen++] = *str++;
        os_radio(RADIO_TX);
}

void perform_measurement() {
        unsigned char msg[56] = {0};
        //define header and device id
        msg[0] = 0xAA;
        msg[1] = 0;
        msg[2] = 5;
        //add uuid
        unsigned char uuid[16] = {0};
        rand_1(uuid, sizeof(uuid));
        for(int count = 0; count < 16; ++count) {
                msg[count+3] = uuid[count];
        }
        digitalWrite(PMVCC,HIGH);
        delay(8000);
        unsigned char *p = msg;
        int countRounds = 0;
        while (readPmData(p) == 0 && countRounds <= MAXTRIES) {
                countRounds++;
        }
        digitalWrite(PMVCC, LOW);
        int res = read_dht22_dat(p);
        if(res == 0) {
          msg[23] = 0;
          msg[24] = 0;
          msg[25] = 0;
          msg[26] = 0;
        }
        readGPSData();
        //write gps data
        for(int i=27; i<33; i++) {
                msg[i] = timestamp[i-27];
        }
        for(int i=33; i<44; i++) {
                msg[i] = lat[i-33];
        }
        for(int i=44; i<56; i++) {
                msg[i] = lng[i-44];
        }
        unsigned char sig[64];
        ed25519_sign(sig, msg, sizeof(msg), pub, priv);

        //encode to base64 msg
        int bufferSize = calculateB64size(sizeof(msg));
        char *buffer = (char*) malloc(sizeof(char) * bufferSize);
        bin_to_b64(msg, sizeof(msg), buffer, bufferSize);
        const char* p3 = (const char*) buffer;
        sendLoraPacket(p3,'9');
        delay(1000);
        //encode to base64 sig
        int bufferSize1 = calculateB64size(sizeof(sig));
        char *buffer1 = (char*) malloc(sizeof(char) * bufferSize1);
        bin_to_b64(sig, sizeof(sig), buffer1, bufferSize1);
        const char* p4 = (const char*) buffer1;
        sendLoraPacket(p4,'x');
        delay(1000);
        free(buffer);
        free(buffer1);
}


void app_main()
{
        Serial.begin(9600, SERIAL_8N1);
        Serial2.begin(9600,SERIAL_8N1, RXD2, TXD2);
        // initialize runtime env
        os_init();

        // Use a frequency in the g3 which allows 10% duty cycling.
        LMIC.freq = 869525000;

        // Maximum TX power
        LMIC.txpow = 27;
        // Use a medium spread factor. This can be increased up to SF12 for
        // better range, but then the interval should be (significantly)
        // lowered to comply with duty cycle limits as well.
        LMIC.datarate = DR_SF9;
        // This sets CR 4/5, BW125 (except for DR_SF7B, which uses BW250)
        LMIC.rps = updr2rps(LMIC.datarate);

        //set control pins to output
        pinMode(PMVCC, OUTPUT);
        perform_measurement();
        esp_deep_sleep(1000000LL * GPIO_DEEP_SLEEP_DURATION);
}
