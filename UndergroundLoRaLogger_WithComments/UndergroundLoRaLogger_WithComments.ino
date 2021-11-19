// This is the code for the aboveground receiver hub, which is part of an  underground, wireless, open-source, low-cost system for monitoring oxygen, temperature, and soil moisture
// By Elad Levintal, The University of California, Davis
// Basic troubleshooting when working with the Adafruit Feather M0: double-click on the RST button to get back into the bootloader (e.g., if the Adafruit Feather M0 is not connecting to the computer because it is in sleep mode)

#include <Wire.h>
#include <SPI.h>
#include "RTClib.h" // the I2C address is 0x68. For the real time clock commands, please read the LICENSE and README files before downloading the library at https://github.com/adafruit/RTClib
#include <SD.h>
#include <RH_RF95.h> // for LoRa commands, please read the LICENSE and README files before downloading the library at https://github.com/PaulStoffregen/RadioHead

//Definitions for the received data packets//
int indexRecv;
String dataRecvFirstString;
String dataRecvSecondString;
String dataRecvThirdString;
String dataRecvFourthString;
////

//Measuring the aboveground hub's battery voltage///
#define VBATPIN A7
float measuredvbat;
////

//RTC//
char buffer [24];
uint8_t secq, minq, hourq, dayq, monthq, yearq ;
RTC_PCF8523 rtc;
////

//SD card//
char FileName[]="10302020.txt";// define the file name (we note that long names can cause an error in SD data logging so we recommend this format and length) 
const int chipSelect = 10;//SD input pin
String LoggerHeader = "TEST"; // insert the column headers for the SD logging (this can also be manually done during the data post-processing stage) 
////

/* for Feather32u4 RFM9x
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7
*/

// for feather m0 RFM9x
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
//

/* for shield 
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 7
*/

/* Feather 32u4 w/wing
#define RFM95_RST     11   // "A"
#define RFM95_CS      10   // "B"
#define RFM95_INT     2    // "SDA" (only SDA/SCL/RX/TX have IRQ!)
*/

/* Feather m0 w/wing 
#define RFM95_RST     11   // "A"
#define RFM95_CS      10   // "B"
#define RFM95_INT     6    // "D"
*/

#if defined(ESP8266)
  /* for ESP w/featherwing */ 
  #define RFM95_CS  2    // "E"
  #define RFM95_RST 16   // "D"
  #define RFM95_INT 15   // "B"

#elif defined(ESP32)  
  /* ESP32 feather w/wing */
  #define RFM95_RST     27   // "A"
  #define RFM95_CS      33   // "B"
  #define RFM95_INT     12   //  next to A

#elif defined(NRF52)  
  /* nRF52832 feather w/wing */
  #define RFM95_RST     7   // "A"
  #define RFM95_CS      11   // "B"
  #define RFM95_INT     31   // "C"
  
#elif defined(TEENSYDUINO)
  /* Teensy 3.x w/wing */
  #define RFM95_RST     9   // "A"
  #define RFM95_CS      10   // "B"
  #define RFM95_INT     4    // "C"
#endif


// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Blinky on receipt
#define LED 13

void setup()
{
  Serial.begin(9600);
  delay(10000);
  
  //RTC//
  #ifndef ESP8266
    //while (!Serial); // wait for serial port to connect. Needed for native USB
  #endif
  if (! rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
    Serial.flush();
    abort();
  }
 ////

 //SD card//
 digitalWrite(RFM95_CS, HIGH); // this line is only for using SD logging with the Adafruit Feather M0 with RFM95 LoRa Radio because the SD and LoRa are both on the same CS pin (#8)
 delay(10);
 Serial.print(F("Initializing SD card..."));
 SD.begin(chipSelect);
 if (!SD.begin(chipSelect)) {  // see if the card is present and can be initialized
   Serial.println(F("Card failed, or not present"));
  // don't do anything more:
  while (1);
  }
  Serial.println(F("card initialized."));
  Serial.print("File name: ");
  Serial.print(FileName);
  File dataFile = SD.open(FileName, FILE_WRITE); // testing if this is a new file then add headers
  Serial.print("File size: ");
  Serial.println(dataFile.size());
  if (dataFile.size()==0){
    dataFile.println(LoggerHeader);
    Serial.println(LoggerHeader);// print to the serial port too
  }
  dataFile.close();
  digitalWrite(RFM95_CS, LOW);
  delay(10);
  ////
 
  pinMode(LED, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.println("Feather LoRa RX Test!");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 20 dBm:
  rf95.setTxPower(20, false);
  
  //For measuring the aboveground hub battery voltage///
  measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;  
  measuredvbat *= 3.3;  // multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  Serial.print("VBat: " ); Serial.println(measuredvbat);
  ////
}

void loop()
{
  //RTC//
  DateTime now = rtc.now(); // get current time
  secq = now.second();
  minq = now.minute();
  hourq = now.hour();
  dayq = now.day();
  monthq = now.month(); 
  sprintf (buffer, "%02u:%02u:%02u %02u/%02u", hourq, minq, secq, dayq, monthq); // this is to zero in single nreadings (e.g., 05 instead of 5 seconds to)  
  String dataString = ""; 
  dataString += String(buffer);dataString += "/";dataString += String(now.year());  // construct the first term (i.e., the timestamp)of the string that will be logged on the SD card
  ////
  
  if (rf95.available())
  {
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len))
    {
      digitalWrite(LED, HIGH);
      RH_RF95::printBuffer("Received: ", buf, len);
      Serial.print("Got: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);
      Serial.print("SNR: ");
      Serial.println(rf95.lastSNR(), DEC);

      String dataRecv = ((char*)buf);
//      Serial.println("start test");
//      Serial.println(dataRecv);
      indexRecv = dataRecv.toInt();
      Serial.println(indexRecv); // check the packet identifier of the current Rx
      if (indexRecv==1) {
      }
      else if (indexRecv==2) {
      }
      else if (indexRecv==3) {
      }
      else { // the last Rx data packet (packet #4 in our case) doesn't need an identifier at the beginning of the data packet
        // Send a reply
        ////////if no reply is needed you can comment from here////////
        uint8_t data[] = "2"; // this value will change the Tx power of the underground node (range between 5 and 20 dBm) or set a new sleeping interval between two measurements(1-1 minute, 2-60 minutes, 3-120 minutes, 4-240 minutes)
        rf95.send(data, sizeof(data));
        rf95.waitPacketSent();
        Serial.println("Sent a reply");
        digitalWrite(LED, LOW);
        ////////end of comment////////
      }
    }
    else
    {
      Serial.println("Receive failed");
    }
    
    String dataRecv = ((char*)buf);
    Serial.println("start test");
    Serial.println(dataRecv);
    indexRecv = dataRecv.toInt();
    Serial.println(indexRecv);
    if (indexRecv==1) {
       dataRecvFirstString = dataRecv; 
       dataRecvFirstString.remove(0, 2); // remove the identifier from the beginning of the data packet
    }
    else if (indexRecv==2) {
       dataRecvSecondString = dataRecv;//
    dataRecvSecondString.remove(0, 2); // remove the identifier from the beginning of the data packet
    
    }
    else if (indexRecv==3) {
       dataRecvThirdString = dataRecv;//
    dataRecvThirdString.remove(0, 2); // remove the identifier from the beginning of the data packet
    
    }
    else { 
     dataRecvFourthString = dataRecv; // the last Rx packet packet doesn't need an identifier at the beginning of the data packet
//    dataRecvFourthString.remove(0, 2);
    
    //Measuring the aboveground hub's battery voltage///
    measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    
    measuredvbat *= 3.3;  // multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    Serial.print("VBat: " ); Serial.println(measuredvbat);
    ////
    
    //SD logging (only after the last data packet was received (packet #4 in our case))
    dataString += ","; // construct the final string that will be logged on the SD card
    dataString += String(rf95.lastRssi(), DEC);
    dataString += ",";
    dataString += String(rf95.lastSNR(), DEC);
    dataString += ",";
    dataString += dataRecvFirstString;//
    dataString += ",";
    dataString += dataRecvSecondString;//
    dataString += ",";
    dataString += dataRecvThirdString;//
    dataString += ",";
    dataString += dataRecvFourthString;//
             
    // open the file
    File dataFile = SD.open(FileName, FILE_WRITE);
    delay(1000);
    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
      // print to the serial port too:
       Serial.print("Logged dataString is: ");
       Serial.println(dataString);
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println(F("error opening datalog.txt"));
    }
    }  
  }
}
