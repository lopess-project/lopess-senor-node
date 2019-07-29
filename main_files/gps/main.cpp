#include <stdio.h>
#include <stdarg.h>
#include <Arduino.h>
#include <SPI.h>


char timestamp[6] = "";
char lng[12] = "";
char lat[11] = "";

/*General Config*/
#define RXD2 15
#define TXD2 4

extern "C" {
void app_main(void);
}

void readGPSData(char *gpggaInput) {
        // Pointer to struct containing the parsed data
}

void resetData() {
        memset(lng,'0',sizeof(lng));
        memset(lat,'0',sizeof(lat));
        memset(timestamp,'0',sizeof(timestamp));
}

void perform_measurement() {
        while(Serial2.available()) {
                char c = Serial2.read();
                if (c=='$') {
                        char c2 = Serial2.read();
                        if(c2=='G') {
                                char c3 = Serial2.read();
                                if(c3=='P') {
                                        char c4 = Serial2.read();
                                        if(c4=='G') {
                                                char c5 = Serial2.read();
                                                if(c5=='G') {
                                                        Serial2.read();
                                                        Serial2.read();
                                                        //reading the timestamp
                                                        for(int i=0; i<6; i++) {
                                                                timestamp[i] = Serial2.read();
                                                                if(timestamp[i]==',') {
                                                                        resetData();
                                                                        break;
                                                                }
                                                        }
                                                        Serial2.read();
                                                        Serial2.read();
                                                        Serial2.read();
                                                        Serial2.read();
                                                        //reading the latitude
                                                        char c6 = Serial2.read();
                                                        if(c6==',') {
                                                                Serial.println(timestamp);
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
                                                                        lat[i] = Serial2.read();
                                                                }
                                                        }
                                                        Serial2.read();
                                                        for(int i=5; i<10; i++) {
                                                                lat[i] = Serial2.read();
                                                        }
                                                        Serial2.read();
                                                        lat[10] = Serial2.read();
                                                        Serial2.read();
                                                        //reading the longtitude
                                                        char c7 = Serial2.read();
                                                        for(int i=0; i<6; i++) {
                                                                if(c7=='-' && i==0) {
                                                                        lng[i] = '-';
                                                                } else if(i==0) {
                                                                        lng[i] = '0';
                                                                        lng[i+1] = c7;
                                                                        i++;
                                                                } else {
                                                                        lng[i] = Serial2.read();
                                                                }
                                                        }
                                                        Serial2.read();
                                                        for(int i=6; i<11; i++) {
                                                                lng[i] = Serial2.read();
                                                        }
                                                        Serial2.read();
                                                        lng[11] = Serial2.read();
                                                        Serial2.read();
                                                        char c8 = Serial2.read();
                                                        if(c8=='1') {
                                                                return;
                                                        }
                                                        resetData();
                                                }
                                        }
                                }
                        }

                }
        }
}

void run_test(void *pvParameter) {
        Serial.println("GPS started...");
        Serial.println("Trying to perform measurements now: ");
        while(1) {
                perform_measurement();
                for(int i=0;i<6;i++) {
                  Serial.print(timestamp[i]);
                }
                Serial.println();
                for(int i=0;i<11;i++) {
                  Serial.print(lat[i]);
                }
                Serial.println();
                for(int i=0;i<12;i++) {
                  Serial.print(lng[i]);
                }
                Serial.println();
                delay(30000);
        }
}

void app_main()
{
        Serial.begin(115200);
        Serial2.begin(9600,SERIAL_8N1, RXD2, TXD2);
        xTaskCreate(&run_test, "run_test", 8094, NULL, 1, NULL);
}
