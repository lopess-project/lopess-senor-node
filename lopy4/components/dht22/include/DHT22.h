/*

	DHT22 temperature sensor driver

*/

#ifndef DHT22_H_
#define DHT22_H_

#define DHT_OK 0
#define DHT_CHECKSUM_ERROR -1
#define DHT_TIMEOUT_ERROR -2
#define DHT_TIMEOUT_ERROR2 -3
#define DHT_TIMEOUT_ERROR3 -4
#define DHT_TIMEOUT_ERROR4 -5

#ifdef __cplusplus
extern "C" {
#endif

// == function prototypes =======================================

void 	setDHTgpio(int gpio);
void 	errorHandler(int response);
int 	readDHT();
float 	getHumidity();
float 	getTemperature();
int 	getSignalLevel( int usTimeOut, bool state );

#ifdef __cplusplus
}
#endif

#endif
