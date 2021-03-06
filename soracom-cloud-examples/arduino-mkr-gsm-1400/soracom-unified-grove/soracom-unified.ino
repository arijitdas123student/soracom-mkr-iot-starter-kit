/**
 * Soracom MKR GSM 1400 demo with Grove Sensor Kit
 * By Felix and Alexis @ Soracom
 */

#include <Arduino.h>            // Base Arduino Library
#include <MKRGSM.h>              // Arduino MKRNB library
#include <DHT.h>                // Grove DHT Sensor library
#include <Ultrasonic.h>         // Grove Ultrasonic Sensor library
#include <ADXL345.h>            // Grove 3-axis digital Accelerometer (1g6) library
#include <MicroNMEA.h>          // Arduino MicroNMEA library to work with Grove GPS Sensor



// Sensor options
#define ENABLE_DHT      false   // Grove Temperature and Humidity Sensor | Connect to D0
#define ENABLE_ULTRA    false   // Grove Ultrasonic Range Sensor         | Connect to D1
#define ENABLE_MAGNETIC false   // Grove Magnetic Sensor                 | Connect to D2
#define ENABLE_BUZZER   false   // Grove Buzzer                          | Connect to D3
#define ENABLE_ACCEL    false   // Grove Accelerometer                   | Connect to TWI
#define ENABLE_GPS      false   // Grove GPS                             | Connect to Serial


#define DHT_PIN         0       // MKR Connector Carrier pin D0
#define DHT_TYPE        DHT11   // DHT11 included in Soracom Grove Sensor kit
#define DHT_UNIT        0       // Set the temperature unit: 0 == Celcius, 1 == Fahrenheit
#define ULTRA_PIN       1       // MKR Connector Carrier pin D1
#define ULTRA_UNIT      0       // Set the distance unit: 0 == centimeters, 1 == inches
#define MAGNETIC_PIN    2       // MKR Connector Carrier pin D2

// Server options
#define HTTP_HOST       "unified.soracom.io" // Send data to Soracom Unified Endpoint.
#define HTTP_PATH       "/"                  // https://developers.soracom.io/en/docs/unified-endpoint/
#define HTTP_PORT       80
#define SEND_INTERVAL   300     // Duration (in seconds) to wait between sending data

// Debug options
#define PRINT_AT        true   // Show or hide AT command output from the modem



// Modem instances
// https://www.arduino.cc/en/Reference/MKRNB
GSM gsmAccess;    // GSM access: include a 'true' parameter for debug enabled
GPRS gprs;  // GPRS access
GSMClient client; // Client service for TCP connection
GSMModem modem;   // Object to send modem commands
GSMScanner scannerNetworks; // Object for network information

// Cellular connection variables
#define APN            "soracom.io"
#define APN_LOGIN      "sora"
#define APN_PASSWORD   "sora"

// Base variables
const unsigned long baud = 115200;
int SoraLoopCounter = 0;



// Grove Temperature and Humidity sensor instance
DHT dht(DHT_PIN, DHT_TYPE);

String getTemperatureAndHumidity(String &separator) {
    // Read temperature or humidity data, it takes about 250 milliseconds
    float h = dht.readHumidity();
    float t = DHT_UNIT == 0 ? dht.readTemperature() : dht.readTemperature(true);

    // Output sensor data to serial
    if (Serial) {
      Serial.println("  Humidity: " + String(h) + " %");
      Serial.println("  Temperature: " + String(t) + (DHT_UNIT == 0 ? " C" : " F"));
    }

    String data = separator + "\"temperature\":" + String(t) + ",\"humidity\":\"" + String(h) +"\"";
    separator = ",";
    return data;
}



// Grove Ultrasonic Range sensor instance
Ultrasonic ultrasonic(ULTRA_PIN);

String getDistance(String &separator) {
  // Read Ultrasonic Sensor data
  long d = ULTRA_UNIT == 0 ? ultrasonic.MeasureInCentimeters() : ultrasonic.MeasureInInches();

  // Output sensor data to serial
  if (Serial) Serial.println("  Distance: " + String(d) + (ULTRA_UNIT == 0 ? " cm" : " inches"));

  String data = separator + "\"distance\":" + String(d);
  separator = ",";
  return data;
}



// Grove Magnetic Switch instance
String getMagnetic(String &separator) {
  // Read Magnetic Switch data
  int m = digitalRead(MAGNETIC_PIN) == HIGH ? 1 : 0;

  // Output sensor data to serial
  if (Serial) Serial.println("  Magnetic Switch: " + String(m));

  String data = separator + "\"magnetic\":" + String(m);
  separator = ",";
  return data;
}



// Grove Accelerometer instance
ADXL345 adxl;

String getAccelerometer(String &separator) {
  // Read Accelerometer data
  double xyz[3];
  adxl.getAcceleration(xyz);

  // Output sensor data to serial
  if (Serial) Serial.println("  Accelerometer: X=" + String(xyz[0]) + " g, Y=" + String(xyz[1]) + " g, Z=" + String(xyz[2]) + " g");

  String data = separator + "\"accel_x\":" + String(xyz[0]) + ",\"accel_y\":" + String(xyz[1]) + ",\"accel_z\":" + String(xyz[2]);
  separator = ",";
  return data;
}



// Grove GPS instances
char nmeaBuffer[100];
bool gpsOnline = false;
HardwareSerial& gps = Serial1; // GPS is connected using the Serial1 interface (pins 13/14 on MKR NB 1500)
MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer)); // Initialize MicroNMEA

String getGps(String &separator) {
  if (Serial) Serial.print(">>> Reading GPS data...");

  // Read the GPS serial interface
  while (gps.available()) {
    char c = gps.read();
    nmea.process(c);
  }

  String data = separator;

  if (!nmea.isValid()) {
    if (Serial) Serial.println(" no valid GPS data found");
    data = data + "\"gps_data\":0";
  } else {
    if (Serial) Serial.println(" valid GPS data found!");

    String system = "none";
    switch (nmea.getNavSystem()) {
      case 'N':
        system = "GNSS";
        break;
      case 'P':
        system = "GPS";
        break;
      case 'L':
        system = "GLONASS";
        break;
      case 'A':
        system = "Galileo";
        break;
    }
    int satellites = nmea.getNumSatellites();
    float hdop = nmea.getHDOP() / 10.;
    char datetime[30];
    sprintf(datetime, "%4d-%02d-%02dT%02d:%02d:%02d", nmea.getYear(), nmea.getMonth(), nmea.getDay(), nmea.getHour(), nmea.getMinute(), nmea.getSecond());
    long latitude = nmea.getLatitude() / 1000000.;
    long longitude = nmea.getLongitude() / 1000000.;
    long altitude = 0;
    bool validAltitude = nmea.getAltitude(altitude);
    long speed = nmea.getSpeed() / 1000.;
    long course = nmea.getCourse() / 1000.;

    // Output sensor data to serial
    if (Serial) {
      Serial.println("  System: " + system);
      Serial.println("  Satellites: " + String(satellites));
      Serial.println("  HDOP: " + String(hdop, 1));
      Serial.println("  Date/time: " + String(datetime));
      Serial.println("  Latitude: " + String(latitude, 6) + " deg");
      Serial.println("  Longitude: " + String(longitude, 6) + " deg");
      Serial.println("  Altitude: " + (validAltitude ? String(altitude / 1000., 3) : "not available"));
      Serial.println("  Speed: " + String(speed, 3));
      Serial.println("  Course: " + String(course, 3));
    }

    data = data + "\"gps_data\":1";
    data = data + ",\"gps_system\":\"" + String(system) +"\"";
    if(satellites != 0) data = data + ",\"gps_satellites\":" + String(satellites);
    if(hdop != 0) data = data + ",\"gps_hdop\":" + String(hdop, 1);
    if(latitude != 0) data = data + ",\"latitude\":" + String(latitude, 6);
    if(longitude != 0) data = data + ",\"longitude\":" + String(longitude, 6);
    if(altitude != 0) data = data + ",\"gps_altitude\":" + (validAltitude ? String(altitude / 1000., 3) : "\"not available\"");
    if(speed != 0) data = data + ",\"gps_speed\":" + String(speed, 3);
    if(course != 0) data = data + ",\"gps_course\":" + String(course, 3);
  }

  // Cleanup GPS data buffer
  if (Serial) Serial.print(">>> Cleaning up GPS data buffer...");
  nmea.clear();
  if (Serial) Serial.println(" done!");

  separator = ",";
  return data;
}

// Helper function to send AT commands to the u-blox R410M modem
void SoraAtCommand(String command, unsigned long timeout) {
  SerialGSM.println(command);

  unsigned long start = millis();
  unsigned char buffer[64];
  int count = 0;

  while (millis() < start + timeout) {
    if (SerialGSM.available()) {
      while (SerialGSM.available()) {
        buffer[count++] = SerialGSM.read();
        if (count == 64) break;
      }
      if (Serial) Serial.write(buffer, count);
    }
  }
}


// Helper function to connect to a data network
void SoraGprsConnect() {
  while (true) {
    if (gsmAccess.begin() == GSM_READY && gprs.attachGPRS(APN, APN_LOGIN, APN_PASSWORD) == GPRS_READY) {
      if (Serial) {
        Serial.println(" connected!");
      }
      return;
    } else {
      if (Serial) Serial.print(".");
      delay(1000);
    }
  }
}

// Helper function to wait <counter> seconds and display a . as timer
void SoraTime(int counter) {
  for (int i = 0; i <= counter; i++) {
    if (Serial) Serial.print(".");
    delay(1000);
  }
}



// Device setup
void setup() {
  // Start the USB serial interface
  Serial.begin(baud);

  // Check to see if the USB serial interface is connected, otherwise continue without it after 1 minute
  for (int i = 0; i < 60; i++) {
    if (Serial) break;
    delay(1000);
  }
  if (Serial) Serial.println(">>> Starting Soracom MKR GSM 1400 demo with Grove Sensor Kit!");

  // Initialize Grove Accelerometer
  if (ENABLE_ACCEL) {
    if (Serial) Serial.print(">>> Initializing Accelerometer...");
    adxl.powerOn();

    // Configure Accelerometer sensitivity and thresholds
    // More information: https://github.com/Seeed-Studio/Accelerometer_ADXL345
    adxl.setActivityThreshold(75);
    adxl.setInactivityThreshold(75);
    adxl.setTimeInactivity(10);
    adxl.setActivityX(1);
    adxl.setActivityY(1);
    adxl.setActivityZ(1);
    adxl.setInactivityX(1);
    adxl.setInactivityY(1);
    adxl.setInactivityZ(1);
    adxl.setTapDetectionOnX(0);
    adxl.setTapDetectionOnY(0);
    adxl.setTapDetectionOnZ(1);
    adxl.setTapThreshold(50);
    adxl.setTapDuration(15);
    adxl.setDoubleTapLatency(80);
    adxl.setDoubleTapWindow(200);
    adxl.setFreeFallThreshold(7);
    adxl.setFreeFallDuration(45);
    adxl.setInterruptMapping(ADXL345_INT_SINGLE_TAP_BIT, ADXL345_INT1_PIN);
    adxl.setInterruptMapping(ADXL345_INT_DOUBLE_TAP_BIT, ADXL345_INT1_PIN);
    adxl.setInterruptMapping(ADXL345_INT_FREE_FALL_BIT,  ADXL345_INT1_PIN);
    adxl.setInterruptMapping(ADXL345_INT_ACTIVITY_BIT,   ADXL345_INT1_PIN);
    adxl.setInterruptMapping(ADXL345_INT_INACTIVITY_BIT, ADXL345_INT1_PIN);
    adxl.setInterrupt(ADXL345_INT_SINGLE_TAP_BIT, 1);
    adxl.setInterrupt(ADXL345_INT_DOUBLE_TAP_BIT, 1);
    adxl.setInterrupt(ADXL345_INT_FREE_FALL_BIT,  1);
    adxl.setInterrupt(ADXL345_INT_ACTIVITY_BIT,   1);
    adxl.setInterrupt(ADXL345_INT_INACTIVITY_BIT, 1);

    if (Serial) Serial.println(" done!");
  }

  // Initialize Grove GPS
  if (ENABLE_GPS) {
    if (Serial) Serial.print(">>> Initializing GPS...");

	  gps.begin(9600); // Start the GPS serial interface

    // Check to see if the GPS serial interface is connected, otherwise continue without it after 30 seconds
    for (int i = 0; i < 30; i++) {
      if (gps) break;
      delay(1000);
    }

    if (!gps) {
      if (Serial) Serial.println(" error, could not start Serial1 interface!");
    } else {
      while (gps.available()) gps.read(); // Clear the buffer

      // Reset the GPS module
      unsigned long timeout = millis() + 10000;
      while (millis() < timeout) {
        while (gps.available()) {
          char c = gps.read();
          if (nmea.process(c)) gpsOnline = true; // Reset is complete when the first valid message is received
        }
        if (gpsOnline) break;
      }

      if (!gpsOnline) {
        if (Serial) Serial.println(" error, could not reset the GPS module!");
      } else {
        // Change the echoing messages to the ones recognized by the MicroNMEA library
        MicroNMEA::sendSentence(gps, "$PSTMSETPAR,1201,0x00000042");
        MicroNMEA::sendSentence(gps, "$PSTMSAVEPAR");

        // Reset the device so that the changes take effect
        MicroNMEA::sendSentence(gps, "$PSTMSRR");
        delay(4000);

        // Clear the buffer
        while (gps.available()) gps.read();

        if (Serial) Serial.println(" done!");
      }
    }
  }

  // Reset the uBlox module
  if (Serial) Serial.print(">>> Resetting modem...");
  pinMode(GSM_RESETN, OUTPUT);
  digitalWrite(GSM_RESETN, HIGH);
  SoraTime(2);
  digitalWrite(GSM_RESETN, LOW);
  SoraTime(5);
  if (Serial) Serial.println(" done!");

  // Turn on the modem
  if (Serial) Serial.print(">>> Turning on modem...");
  if (modem.begin()) {
    if (Serial) Serial.println(" done!");
  } else {
    if (Serial) Serial.println(" error, could not turn on modem! Try power-cycling.");
    return;
  }

  // Connect to the modem's serial interface
  if (Serial) Serial.print(">>> Connecting to modem...");
  SerialGSM.begin(baud);
  while (!SerialGSM) { ; } // Wait until the modem's serial interface is ready
  if (Serial) Serial.println(" connected!");

  // Show modem firmware version
  if (Serial && PRINT_AT) {
    Serial.println(">>> Modem firmware:");
    SoraAtCommand("ATI9", 250);
  }

  // Connect to 3G Network
  if (Serial) Serial.print(">>> Connecting to 3G network...");
  SoraGprsConnect(); // Attach to the data network

  if (Serial) Serial.println(">>> Startup complete! Now reading sensors and sending data to " + String(HTTP_HOST));
}

// Running looping function
void loop() {
  if (Serial) Serial.println("-----------------------");

  // Make sure the device is still connected to 3G network
  if (gsmAccess.isAccessAlive()) {
    if (Serial) Serial.println(">>> Modem is still connected to 3G network!");

    // On program start and then every 12th loop
    if (Serial && SoraLoopCounter % 12 == 0) {
      // print the network info
      Serial.println(">>> Current carrier: " + scannerNetworks.getCurrentCarrier());
      Serial.println(">>> Signal Strength: " + scannerNetworks.getSignalStrength() + " [0-31]");
    }
  } else {
    if (Serial) Serial.print(">>> Disconnected from 3G network. Reconnecting...");
    gsmAccess.shutdown(); // Reset the modem
    SoraGprsConnect(); // Attach to the data network
  }

  if (ENABLE_DHT || ENABLE_ULTRA || ENABLE_MAGNETIC || ENABLE_ACCEL || ENABLE_DHT) {
    if (Serial) Serial.println(">>> Reading sensors:");
  }

  // Read from enabled Sensors and build JSON data
  String separator = ""; // A separator to add commas in between sensor data
  String data = "{";
  if (ENABLE_DHT)
    data = data + getTemperatureAndHumidity(separator);
  if (ENABLE_ULTRA)
    data = data + getDistance(separator);
  if (ENABLE_MAGNETIC)
    data = data + getMagnetic(separator);
  if (ENABLE_ACCEL)
    data = data + getAccelerometer(separator);
  if (ENABLE_GPS && gpsOnline)
    data = data + getGps(separator);
  data = data + "}";

  // Connect to server
  if (Serial) Serial.print(">>> Connecting to " + String(HTTP_HOST) + "...");
  if (client.connect(HTTP_HOST, HTTP_PORT)) {
    if (Serial) Serial.println(" connected!");

    // Make an HTTP POST request
    if (Serial) Serial.print(">>> Sending data to server...");
    client.println("POST / HTTP/1.1");
    client.println("Host: " + String(HTTP_HOST));
    client.println("User-Agent: Soracom/1.0");
    client.println("Connection: close");
    client.println("Content-Length: " + String(data.length()));
    client.println();
    client.println(data);

    if (Serial) Serial.println(" done!");

    if (Serial) Serial.print(">>> Waiting for server response...");

    unsigned long timeout = millis() + 10000;
    String response = "";
    while (millis() < timeout) {
      if (client.available()) {
        while (client.available()) {
          response.concat((char)client.read());
        }
        break;
      }
    }
    if (response != "") {
      if (Serial) {
        Serial.println(" received response from server:");
        Serial.println();
        Serial.println(response);
        Serial.println();
      }
    } else {
      if (Serial) Serial.println(" timeout, no response from server");
    }

    if (client.connected()) {
      if (Serial) Serial.print(">>> Disconnecting from server...");
      client.stop();
      if (Serial) Serial.println(" done!");
    } else {
      if (Serial) Serial.println(">>> Disconnected from server");
    }
    client.flush();
  } else {
    // ifyou didn't get a connection to the server:
    if (Serial) Serial.println(" error, connection failed!");
  }

  // Increase loop counter and wait for next iteration
  SoraLoopCounter++;
  if (Serial) Serial.println(">>> Waiting " + String(SEND_INTERVAL) + " seconds until next reading");
  SoraTime(SEND_INTERVAL);
  if (Serial) Serial.println();
}