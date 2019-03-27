/*
 * project: DHT_logger
 * description: reads from DHT sensor and prints to serial
 * author: drew bell
 * date: 3/26/2019
 * 
 * Work heavily based on the PietteTech_DHT library, info below: 
 * 
 * FILE:        DHT_2sensor.cpp
 * VERSION:     0.4
 * PURPOSE:     Example that uses DHT library with two sensors
 * LICENSE:     GPL v3 (http://www.gnu.org/licenses/gpl.html)
 *
 * Calls acquire on two sensors and monitors the results for long term
 * analysis.  It uses DHT.acquire and DHT.acquiring
 * Also keeps track of the time to complete the acquire and tracks errors
 *
 * Scott Piette (Piette Technologies) scott.piette@gmail.com
 *      January 2014        Original Spark Port
 *      October 2014        Added support for DHT21/22 sensors
 *                          Improved timing, moved FP math out of ISR
 *      September 2016      Updated for Particle and removed dependency
 *                          on callback_wrapper.  Use of callback_wrapper
 *                          is still for backward compatibility but not used
 * ScruffR
 *      February 2017       Migrated for Libraries 2.0
 *                          Fixed blocking acquireAndWait()
 *                          and previously ignored timeout setting
 *                          Added timeout when waiting for Serial input
 *                          Fixed possible cloud drop with faulty sensor
 *
 * With this library connect the DHT sensor to the following pins
 * Spark Core: D0, D1, D2, D3, D4, A0, A1, A3, A5, A6, A7
 * Particle  : any Pin but D0 & A5
 * See docs for more background
 *   https://docs.particle.io/reference/firmware/photon/#attachinterrupt-
 */

 // NOTE DHT_REPORT_TIMING requires DHT_DEBUG_TIMING in PietteTech_DHT.h for debugging edge->edge timings
 //#define DHT_REPORT_TIMING

#include "PietteTech_DHT.h"

#define DHTTYPEA  DHT11       // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPINA   D2          // Digital pin for comunications
#define DHTTYPEB  DHT11       // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPINB   D3          // Digital pin for comunications

// Instantiate two class objects
PietteTech_DHT DHTA(DHTPINA, DHTTYPEA);
PietteTech_DHT DHTB(DHTPINB, DHTTYPEB);
int n;      // counter

// cloud publish vars
bool cloudPub = false;
String lastTempF = "";
String lastRH = "";
String lastDP = "";
String lastMeasureTime = "";

#define LOOP_DELAY 5000 // 5s intervals
int _sensorA_error_count;
int _sensorB_error_count;
int _spark_error_count;
unsigned long _lastTimeInLoop;

int cloudPrint(String ignored){
  cloudPub = true;
}

void setup()
{
  Serial.begin(9600);
  while (!Serial.available() && millis() < 5000){
    Serial.println("Press any key to start.");
    Particle.process();
    delay(1000);
  }

  bool success = Particle.function("pubData", cloudPrint);

  Serial.println("DHT 2 Sensor program using DHT.acquire and DHT.aquiring");
  Serial.print("LIB version: ");
  Serial.println(DHTLIB_VERSION);
  Serial.println("---------------");
  
  DHTA.begin();
  //DHTB.begin();

  delay(1000);        // Delay 1s to let the sensors settle
  _lastTimeInLoop = millis();
}

#if defined(DHT_REPORT_TIMING)
// This function will report the timings collected
void printEdgeTiming(class PietteTech_DHT *_d) {
  byte n;
  volatile uint8_t *_e = &_d->_edges[0];

  Serial.print("Edge timing = ");
  for (n = 0; n < 41; n++) {
    Serial.print(*_e++);
    if (n < 40)
      Serial.print(".");
  }
  Serial.print("\n\r");
}
#endif


void publishToCloud(void){
  static unsigned lastTime = 0;
  unsigned now = millis();
  if( cloudPub || (now - lastTime) >= 30 * 60 * 1000 || !lastTime){     //publish every 20 min
    if(cloudPub) cloudPub = false;
    lastTime = now;
    String packet = lastMeasureTime + String("|T=") 
        + String(lastTempF + String(" degF|RH="))
        + String(lastRH + String("%|DP="))
        + String(lastDP + String(";"));
    bool message_send_status = Particle.publish("R&D", packet, 60, PRIVATE);
  }
}

void printSensorData(class PietteTech_DHT *_d) {
  int result = _d->getStatus();

  if (result != DHTLIB_OK)
    if (_d == &DHTA)
      _sensorA_error_count++;
    else
      _sensorB_error_count++;

  switch (result) {
  case DHTLIB_OK:
    Serial.println("OK");
    break;
  case DHTLIB_ERROR_CHECKSUM:
    Serial.println("Error\n\r\tChecksum error");
    break;
  case DHTLIB_ERROR_ISR_TIMEOUT:
    Serial.println("Error\n\r\tISR time out error");
    break;
  case DHTLIB_ERROR_RESPONSE_TIMEOUT:
    Serial.println("Error\n\r\tResponse time out error");
    break;
  case DHTLIB_ERROR_DATA_TIMEOUT:
    Serial.println("Error\n\r\tData time out error");
    break;
  case DHTLIB_ERROR_ACQUIRING:
    Serial.println("Error\n\r\tAcquiring");
    break;
  case DHTLIB_ERROR_DELTA:
    Serial.println("Error\n\r\tDelta time to small");
    break;
  case DHTLIB_ERROR_NOTSTARTED:
    Serial.println("Error\n\r\tNot started");
    break;
  default:
    Serial.println("Unknown error");
    break;
  }

#if defined(DHT_REPORT_TIMING)
  // print debug timing information
  printEdgeTiming(_d);
#endif

  if(result == DHTLIB_OK){
    lastTempF = String(_d->getFahrenheit(), 2);
    lastRH = String(_d->getHumidity(), 2);
    lastDP = String(_d->getDewPoint() * 9 / 5 + 32, 2); 
  } else {
    lastTempF = "err";
    lastRH = "err";
    lastDP = "err";
  }
  lastMeasureTime = Time.timeStr();
  
  Serial.println("Time: " + lastMeasureTime); 
  
  Serial.print("Temperature (oF): ");
  Serial.println(_d->getFahrenheit(), 2);

  Serial.print("Humidity (%): ");
  Serial.println(_d->getHumidity(), 2);

  Serial.print("Dew Point (F): ");
  Serial.println(_d->getDewPoint() * 9 / 5 + 32);

  publishToCloud();

}

void loop()
{
  unsigned long _us = millis();
  unsigned long _delta = (_us - _lastTimeInLoop);

  if (_delta > (1.05 * LOOP_DELAY))
    _spark_error_count++;

  // Launch the acquisition on the two sensors
  DHTA.acquire();
  //DHTB.acquire();

  // Print information for Sensor A
  Serial.print("\n");
  Serial.print(n);
  Serial.print(" : ");
  Serial.print((float)(_delta / 1000.0));
  Serial.print("s");
  if (_sensorA_error_count > 0 || _spark_error_count > 0) {
    Serial.print(" : E=");
    Serial.print(_sensorA_error_count);
    Serial.print("/");
    Serial.print(_spark_error_count);
  }
  Serial.print(", Retrieving information from sensor: ");
  Serial.print("Read sensor A: ");

  while (DHTA.acquiring()) Particle.process();
  printSensorData(&DHTA);

  /* Print information for Sensor B
  Serial.print("\n");
  Serial.print(n);
  Serial.print(" : ");
  Serial.print((float)(_delta / 1000.0));
  Serial.print("s");
  if (_sensorB_error_count > 0 || _spark_error_count > 0) {
    Serial.print(" : E=");
    Serial.print(_sensorB_error_count);
    Serial.print("/");
    Serial.print(_spark_error_count);
  }
  Serial.print(", Retrieving information from sensor: ");
  Serial.print("Read sensor B: ");

  while (DHTB.acquiring()) Particle.process();
  printSensorData(&DHTB);
  */

  n++;
  _lastTimeInLoop = _us;

  delay(LOOP_DELAY);
}
