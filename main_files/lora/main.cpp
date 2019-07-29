#include <stdio.h>
#include <stdarg.h>
#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <ed25519.h>
#include <base64_esp.h>



/*LoRa Config*/

// How often to send a packet. Note that this sketch bypasses the normal
// LMIC duty cycle limiting, so when you change anything in this sketch
// (payload length, frequency, spreading factor), be sure to check if
// this interval should not also be increased.
// See this spreadsheet for an easy airtime and duty cycle calculator:
// https://docs.google.com/spreadsheets/d/1voGAtQAjC1qBmaVuP1ApNKs1ekgUjavHuVQIXyYSvNc
#define TX_INTERVAL 20000

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

const unsigned char pub[32] = { 165 ,0 ,90 ,147 ,13 ,168 ,197 ,121 ,37 ,88 ,106 ,238 ,77 ,211 ,21 ,157 ,182 ,205 ,176 ,209 ,190 ,158 ,202 ,35 ,118 ,91 ,172 ,2 ,38 ,145 ,84 ,181 };
const unsigned char priv[64] = { 120 ,164 ,70 ,198 ,231 ,3 ,19 ,146 ,91 ,72 ,97 ,127 ,2 ,82 ,224 ,182 ,232 ,87 ,97 ,99 ,192 ,91 ,69 ,83 ,212 ,140 ,161 ,93 ,217 ,215 ,94 ,76 ,199 ,64 ,90 ,97 ,101 ,103 ,127 ,105 ,255 ,215 ,163 ,235 ,196 ,109 ,143 ,212 ,60 ,70 ,6 ,21 ,188 ,121 ,128 ,114 ,169 ,148 ,179 ,104 ,195 ,31 ,106 ,33 };
unsigned long startMillis;
unsigned long currentMillis;
unsigned int Pm25 = 0;
unsigned int Pm10 = 0;

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
      int init = analogRead(36);
      int count = 0;
      while (analogRead(36) == init) {
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

// source from: https://github.com/FriskByBergen/SDS011/blob/master/arduino.ino
void readPmData(unsigned char *msg) {
  uint8_t mData = 0;
  uint8_t i = 0;
  uint8_t mPkt[10] = {0};
  uint8_t mCheck = 0;
  while (Serial2.available() > 0) {
    // from www.inovafitness.com
    // packet format: AA C0 PM25_Low PM25_High PM10_Low PM10_High 0 0 CRC AB
    mData = Serial2.read();
    Serial.println("Reading PM data...");
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
          uint16_t Pm10, Pm25;
          Pm25 = (uint16_t)mPkt[2] | (uint16_t)(mPkt[3] << 8);
          Pm10 = (uint16_t)mPkt[4] | (uint16_t)(mPkt[5] << 8);
          Serial.print("Pm2.5 ");
          Serial.print(float(Pm25) / 10.0);
          Serial.print("Pm10 ");
          Serial.print(float(Pm10) / 10.0);
          return;
        } else {
          Serial.println("checksum not valid...");
        }
      }
    }
  }
}

int calculateB64size(unsigned int input_size) {
  unsigned int adjustment = ( (input_size % 3) ? (3 - (input_size % 3)) : 0);
  unsigned int code_padded_size = ( (input_size + adjustment) / 3) * 4;
  unsigned int newline_size = ((code_padded_size) / 72) * 2;
  int total_size = code_padded_size + newline_size;
  return total_size;
}

void sendLoraPacket(const char *str) {
  os_radio(RADIO_RST); // Stop RX first
  delay(1); // Wait a bit, without this os_radio below asserts, apparently because the state hasn't changed yet

  LMIC.dataLen = 0;
  LMIC.frame[LMIC.dataLen++] = '<';
  LMIC.frame[LMIC.dataLen++] = '1';
  LMIC.frame[LMIC.dataLen++] = '>';
  while (*str)
    LMIC.frame[LMIC.dataLen++] = *str++;
  os_radio(RADIO_TX);
}

void printKeys(int inputSize, unsigned char* msg) {
  int bufferSize = calculateB64size(inputSize);
  char *b64buffer = (char*) malloc((sizeof(char) * bufferSize)+1);
  Serial.println("Size of buffer is: ");
  Serial.print(bufferSize);
  bin_to_b64(msg, inputSize, b64buffer, bufferSize);
  b64buffer[bufferSize+1] = '\0';
  Serial.print("Base 64 encoded key: ");
  for(int i=0; i<bufferSize; i++) {
    Serial.print(b64buffer[i]);
  }
  Serial.println();
}

void perform_measurement() {
  unsigned char msg[88];
  //define header and device id
  msg[0] = 0xAA;
  msg[1] = 0;
  msg[2] = 1;
  //add uuid
  Serial.println("UUID is: ");
  unsigned char uuid[16] = {0};
  rand_1(uuid, sizeof(uuid));
  for(int count = 0; count < 16; ++count) {
    msg[count+3] = uuid[count];
    Serial.print(uuid[count]);
  }
  Serial.println();
  Serial.println("Reading PM values...");
  //clear buffer in order to get latest measurements
  int j = 0;
  while (Serial2.available() > 0) {
    Serial2.read();
    j = j+1;
  }
  //ensure that there will be data before reading data
  if(Serial2.available() == 0) {
    int16_t offset = 2500;
    if(j != 0) {
      offset = 60000 / (j/10);
    }
    Serial.print(offset);
    Serial.println(" ms have been waited");
    delay(offset);
  }
  unsigned char *p = msg;
  digitalWrite(21,HIGH);
  delay(2000);
  readPmData(p);
  digitalWrite(21, LOW);
  Serial.print("Signing Message...Size of msg is: ");
  unsigned char msgToSign[23];
  Serial.println(sizeof(msgToSign));
  for(int count = 0; count < 23; ++count) {
    msgToSign[count] = msg[count];
  }
  unsigned char seed[32], sig[64];

  //b64_to_bin((const char*)publicKey, 44, pub, 32);
  //b64_to_bin((const char*)privateKey, 90, priv, 64);

  //if (ed25519_create_seed(seed,32)) {
    //Serial.println("error while generating seed");
    //exit(1);
  //}
  //ed25519_create_keypair(pub, priv, seed);
  Serial.println("Pubkey binary: ");
  for(int k=0; k<32; k++) {
    Serial.printf("%u ,", pub[k]);
  }
  Serial.println();
  //printKeys(32,pub);
  Serial.println("Privkey binary: ");
  for(int m=0; m<64; m++) {
    Serial.printf("%u ,", priv[m]);
  }
  Serial.println();
  //printKeys(64,priv);
  ed25519_sign(sig, msgToSign, sizeof(msgToSign), pub, priv);

  if(ed25519_verify(sig, msgToSign, sizeof(msgToSign), pub)) {
    Serial.println("verification successful");
  } else {
    Serial.println("verification failed");
  }
  for(int count = 0; count < 64; ++count) {
    msg[count+23] = sig[count];
  }
  msg[87] = '\0';
  Serial.println("Measurement successfully performed. Char array is: ");
  for(int count = 0; count < 88; ++count) {
    Serial.print(msg[count]);
  }
  //encode to base64
  int inputSize = 88;
  int bufferSize = calculateB64size(inputSize);
  char *buffer = (char*) malloc(sizeof(char) * bufferSize);
  bin_to_b64(msg, inputSize, buffer, bufferSize);
  const char* p3 = (const char*) buffer;
  sendLoraPacket(p3);
  free(buffer);
}

void run_test(void *pvParameter) {
  Serial.println("Test started...");
  Serial.println("Trying to perform measurements now: ");
  while(1) {
    perform_measurement();
    delay(60000);
  }
}

void app_main()
{
    Serial.begin(115200);
    Serial2.begin(9600,SERIAL_8N1, RXD2, TXD2);
      // initialize runtime env
    os_init();

    // Set up these settings once, and use them for both TX and RX

    #if defined(CFG_eu868)
    // Use a frequency in the g3 which allows 10% duty cycling.
    LMIC.freq = 869525000;
    #elif defined(CFG_us915)
    LMIC.freq = 902300000;
    #endif

    // Maximum TX power
    LMIC.txpow = 27;
    // Use a medium spread factor. This can be increased up to SF12 for
    // better range, but then the interval should be (significantly)
    // lowered to comply with duty cycle limits as well.
    LMIC.datarate = DR_SF9;
    // This sets CR 4/5, BW125 (except for DR_SF7B, which uses BW250)
    LMIC.rps = updr2rps(LMIC.datarate);

    //set pin 21 to output
    pinMode(21, OUTPUT);
    xTaskCreate(&run_test, "run_test", 8094, NULL, 1, NULL);
}
