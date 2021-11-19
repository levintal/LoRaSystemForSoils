// This is the code for the underground node, which is part of an  underground, wireless, open-source, low-cost system for monitoring oxygen, temperature, and soil moisture
// By Elad Levintal, The University of California, Davis
// Basic troubleshooting when working with the Adafruit Feather M0: double-click on the RST button to get back into the bootloader (e.g., if the Adafruit Feather M0 is not connecting to the computer because it is in sleep mode)
// For additional SDI-12 protocol information -> sdi-12.org

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_ADS1015.h> // for the four KE-25 oxygen sensors, the default address is 0x48 but can be changed to one of 4 different addresses (0x48,0x49,0x4A,0x4B), please read the LICENSE and README files before downloading the library at https://github.com/adafruit/Adafruit_ADS1X15
#include <Adafruit_SleepyDog.h> // for sleep mode between readings, please read the LICENSE and README files before downloading the library at https://github.com/adafruit/Adafruit_SleepyDog
#include "SDI12_ARM.h" // for the 5TM sensors, please read the LICENSE and README files before downloading the library at https://github.com/ManuelJimenezBuendia/Arduino-SDI-12 
#include <RH_RF95.h> // for LoRa commands, please read the LICENSE and README files before downloading the library at https://github.com/PaulStoffregen/RadioHead


float SLEEP_TIME_MIN = 120; // define the initial sleeping time between cycles
int RelayPinOn = 13; // define pin#13 as the power relay pin

//Measuring the node's battery voltage///
#define VBATPIN A7
float measuredvbat = 0;
int batteryIndex = 9;
////

//ADS1015 - for the four KE-25 oxygen sensors (0-~13[mV] equals to 0-~21[%])//
Adafruit_ADS1015 ads1015(0x48);  // construct an ads1015 at address 0x48
float bitValue = 0.125; // define the value of 1 bit 
////

//5TM sensors; we note that in this section are some concepts taken from https://wetlandsandr.wordpress.com/2015/09/12/building-a-soil-moisture-and-conductivity-probe/ so we also recommend looking at this site if additional information is needed
static const uint16_t pin_fiveTM_SDI12 = 5; // set the data pin to which all four 5TM are connected
SDI12 fiveTM_SDI12(pin_fiveTM_SDI12); // create an SDI-12 object and commands
char* samples1;
char* samples2;
char* samples3;
char* samples4;
////

//LoRa//
int i = 1; // define an index for the data packet
String TxdataString;
String TxdataString1;
String TxdataString2;
String TxdataString3;
String TxdataString4;
/* for feather32u4 
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7
*/

// for feather m0  
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

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 20 dBm:
int TxPower = 5; // set transmitter power to 5 dBm
////


void setup() {
  Serial.begin(9600);

  //Lora//
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(100);
  Serial.println("Feather LoRa TX Test!");

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
  rf95.setTxPower(TxPower, false);
  ////

  //Power relay//
  pinMode(RelayPinOn, OUTPUT); // initialize digital pin 13 as an output
  digitalWrite(RelayPinOn, LOW); // turn off the power relay
  ////
}

void loop() {
  int SLEEP_TIME = SLEEP_TIME_MIN*60; // transform to seconds
  
  if (i==1){   
    rf95.sleep();
    for(int q= 0; q < SLEEP_TIME / 8; q++) { // this section was adjusted from "https://tum-gis-sensor-nodes.readthedocs.io/en/latest/adafruit_32u4_lora/README.html"
      Watchdog.sleep(8000); // in milliseconds - the watchdog timer max sleep time is 8 seconds so an adjustment is needed (see the link above for additional details)
    }
    if (SLEEP_TIME % 8) {
      Watchdog.sleep((SLEEP_TIME % 8)*1000);
     }
  }
    
  if (i==1){ // if this is the first data package
    float Oxygen1 = 0; float Oxygen2 = 0; float Oxygen3 = 0; float Oxygen4 = 0;
    float dielectric1 = 0; float soilMoist1 = 0; float degC1 = 0;   
    float dielectric2 = 0; float soilMoist2 = 0; float degC2 = 0;     
    float dielectric3 = 0; float soilMoist3 = 0; float degC3 = 0;    
    float dielectric4 = 0; float soilMoist4 = 0; float degC4 = 0;
 
    digitalWrite(RelayPinOn,HIGH); // turn on the power relay
    
    int countdownMS = Watchdog.enable(20000); // start a 20 seconds timer - this will reset the microcontroller in case it will get stuck in the loop (e.g., a problematic sensor)
    
    delay(100); // delay before the start of readings
    //read from the four KE-25 (oxygen) sensors using the ADS1015
    ads1015.begin();
    delay(20);
    ads1015.setGain(GAIN_SIXTEEN); // 16x gain  +/- 0.256V  1 bit = 0.125mV (see the ADS1015 webpage for additional information)
    delay(20);
    Oxygen1 = ads1015.readADC_SingleEnded(0)*bitValue;
    delay(20);
    Oxygen2 = ads1015.readADC_SingleEnded(1)*bitValue;
    delay(20);
    Oxygen3 = ads1015.readADC_SingleEnded(2)*bitValue;
    delay(20);
    Oxygen4 = ads1015.readADC_SingleEnded(3)*bitValue;
    delay(20);
    //// 
    
    //Read the 5TM sensor at address 1//
    fiveTM_SDI12.begin();
    String dataString1;
    fiveTM_SDI12.sendCommand("1M!"); // take a measurement, returns address code for measurement
    delay(20);
    while (fiveTM_SDI12.available())fiveTM_SDI12.read();
    delay(1000);
    fiveTM_SDI12.sendCommand("1D0!"); // sensor at address 1 - read data, returns "address+dielectric+temp"
    delay(20);
    while (fiveTM_SDI12.available())
    {
    uint8_t data = fiveTM_SDI12.read();
    if (data != '\r' && data != '\n')
    dataString1 += (char) data;
    }
    fiveTM_SDI12.end();
    ////
    
    //Read the 5TM sensor at address 2//
    String dataString2;
    fiveTM_SDI12.begin();
    fiveTM_SDI12.sendCommand("2M!"); // take a measurement, returns address code for measurement
    delay(20);
    while (fiveTM_SDI12.available())fiveTM_SDI12.read();
    delay(1000);
    fiveTM_SDI12.sendCommand("2D0!"); // sensor at address 2 - read data, returns "address+dielectric+temp"
    delay(20);
    while (fiveTM_SDI12.available())
    {
    uint8_t data = fiveTM_SDI12.read();
    if (data != '\r' && data != '\n')
    dataString2 += (char) data;
    }
    fiveTM_SDI12.end();
    ////
    
    //Read the 5TM sensor at address 3//
    String dataString3;
    fiveTM_SDI12.begin();
    fiveTM_SDI12.sendCommand("3M!"); // take a measurement, returns address code for measurement
    delay(20);
    while (fiveTM_SDI12.available())fiveTM_SDI12.read();
    delay(1000); //
    fiveTM_SDI12.sendCommand("3D0!"); // sensor at address 3 - read data, returns "address+dielectric+temp"
    delay(20);
    while (fiveTM_SDI12.available())
    {
    uint8_t data = fiveTM_SDI12.read();
    if (data != '\r' && data != '\n')
    dataString3 += (char) data;
    }
    fiveTM_SDI12.end();
    ////
    
    //Read the 5TM sensor at address 4//
    String dataString4;
    fiveTM_SDI12.begin();
    fiveTM_SDI12.sendCommand("4M!"); // take a measurement, returns address code for measurement
    delay(20);
    while (fiveTM_SDI12.available())fiveTM_SDI12.read();
    delay(1000); 
    fiveTM_SDI12.sendCommand("4D0!"); // sensor at address 4 - read data, returns "address+dielectric+temp"
    delay(20);
    while (fiveTM_SDI12.available())
    {
    uint8_t data = fiveTM_SDI12.read();
    if (data != '\r' && data != '\n')
    dataString4 += (char) data;
    }
    fiveTM_SDI12.end();
    ////  
    
    Watchdog.disable(); // end the 20 seconds timer
    
    digitalWrite(RelayPinOn,LOW); // turn off the power relay
    
    //Processing of 5TM data from address 1//
    if (dataString1.length()< 4)
    {
    Serial.println("fiveTM #1 error");
    }
    int str_len1 = dataString1. length() + 1;
    char samples1[str_len1];
    dataString1. toCharArray(samples1, str_len1);
    char* term1 = strtok(samples1, "+"); // first term is the sensor address (not needed for our objective)
    term1      = strtok(NULL, "+"); // second term is the dielectric conductivity & soil moisture
    dielectric1 = atof(term1);             
    term1 = strtok(NULL, "+");
    degC1 = atof(term1);
    ////

    //Processing of 5TM data from address 2//  
    if (dataString2.length()< 4)
    {
    Serial.println("fiveTM #2 error");
    }
    int str_len2 = dataString2. length() + 1;
    char samples2[str_len2];
    dataString2. toCharArray(samples2, str_len2);
    char* term2 = strtok(samples2, "+");
    term2      = strtok(NULL, "+");
    dielectric2 = atof(term2);             
    term2 = strtok(NULL, "+");
    degC2 = atof(term2);
    ////

    //Processing of 5TM data from address 3//
    if (dataString3.length()< 4)
    {
    Serial.println("fiveTM #3 error");
    }
    int str_len3 = dataString3. length() + 1;
    char samples3[str_len3];
    dataString3. toCharArray(samples3, str_len3);
    char* term3 = strtok(samples3, "+");
    term3      = strtok(NULL, "+");
    dielectric3 = atof(term3);             
    term3 = strtok(NULL, "+");
    degC3 = atof(term3);
    ////

    //Processing of 5TM data from address 4//  
    if (dataString4.length()< 4)
    {
    Serial.println("fiveTM #4 error");
    }
    int str_len4 = dataString4. length() + 1;
    char samples4[str_len4];
    dataString4. toCharArray(samples4, str_len4);
    char* term4 = strtok(samples4, "+");
    term4      = strtok(NULL, "+");
    dielectric4 = atof(term4);             
    term4 = strtok(NULL, "+");
    degC4 = atof(term4);
    ////
    
    //Measuring the node's battery voltage///
    batteryIndex++; // take a measurement once in 10 cycles (instead of every cycle) to save battery life
    if (batteryIndex ==10){
      measuredvbat = analogRead(VBATPIN);
      measuredvbat *= 2;    
      measuredvbat *= 3.3;  // multiply by 3.3V, our reference voltage
      measuredvbat /= 1024; // convert to voltage
      batteryIndex = 1;
    }
    ////
    
    //Construct the first data packet//
    TxdataString1 = "";
    TxdataString1 += String("1"); // packet identifier
    TxdataString1 += ",";
    TxdataString1 += String(Oxygen1);// O2 (K-25 #1) [mV]
    TxdataString1 += ",";
    TxdataString1 += String(Oxygen2);// O2 (K-25 #2) [mV]
    TxdataString1 += ",";
    TxdataString1 += String(Oxygen3);// O2 (K-25 #3) [mV]
    ////
    
    //Construct the second data packet//
    TxdataString2 = "";
    TxdataString2 += String("2"); // packet identifier
    TxdataString2 += ",";
    TxdataString2 += String(Oxygen4); // O2 (K-25 #4) [mV]
    TxdataString2 += ",";
    TxdataString2 += String(dielectric1); // apparent dielectric permittivity (5TM #1) [-]
    TxdataString2 += ",";
    TxdataString2 += String(dielectric2); // apparent dielectric permittivity (5TM #2) [-]
    ////

    //Construct the third data packet//
    TxdataString3 = "";
    TxdataString3 += String("3"); // packet identifier
    TxdataString3 += ",";
    TxdataString3 += String(dielectric3); // apparent dielectric permittivity (5TM #3) [-] 
    TxdataString3 += ",";
    TxdataString3 += String(dielectric4); // apparent dielectric permittivity (5TM #4) [-]
    TxdataString3 += ",";
    TxdataString3 += String(degC1,1); // temperature (5TM #1) [C] ;send the temperature value only with 1 decimal digit 
    ////

    //Construct the fourth  data packet//
    TxdataString4 = "";
    TxdataString4 += String(degC2,1); // temperature (5TM #2) [C] ;send the temperature value only with 1 decimal digit     
    TxdataString4 += ",";
    TxdataString4 += String(degC3,1); // temperature (5TM #3) [C] ;send the temperature value only with 1 decimal digit      
    TxdataString4 += ",";
    TxdataString4 += String(degC4,1); // temperature (5TM #4) [C] ;send the temperature value only with 1 decimal digit 
    TxdataString4 += ",";
    TxdataString4 += String(measuredvbat); // node's battery voltage [V];
    ////
  
//    Serial.print("TxdataString1: "); Serial.println(TxdataString1);
//    Serial.print("TxdataString2: "); Serial.println(TxdataString2);
//    Serial.print("TxdataString3: "); Serial.println(TxdataString3);
//    Serial.print("TxdataString4: "); Serial.println(TxdataString4);
  }
  
  
  if (i==1){ // define which data packet will be transmitted in the current Tx
    TxdataString = TxdataString1; 
  }
    else if (i==2)
    {
    TxdataString = TxdataString2;
  }
    else if (i==3)
    {
    TxdataString = TxdataString3;
  }
    else if (i==4)
    {
    TxdataString = TxdataString4;
  } 
  
  //Transmit data packet from the node//
  int str_lenTx = TxdataString. length() + 1;//Converting String (i.e., 'TxdataString') to Char Array (i.e., 'radiopacket')
  char radiopacket[str_lenTx];
  TxdataString.toCharArray(radiopacket, str_lenTx);
//  Serial.print("Sending "); Serial.println(radiopacket);
  radiopacket[19] = 0; 
//  Serial.println("Sending...");
  delay(10);
  rf95.send((uint8_t *)radiopacket, 20);
//  Serial.println("Waiting for packet to complete..."); 
  delay(10);
  rf95.waitPacketSent();
  ////

  if (i==1){
    i=2;
  }
    else if (i==2)
    {
    i=3;
  }
    else if (i==3)
    {
    i=4;
  }
    else if (i==4)
    {
    i=1;
  
  //Now wait for a reply//
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
//  Serial.println("Waiting for reply...");
  if (rf95.waitAvailableTimeout(1000))
  { 
    // Should be a reply message for us now   
    if (rf95.recv(buf, &len))
   {
    Serial.print("Got reply: ");
    Serial.println((char*)buf);
    char char_input[2];
    char_input[0] = buf [0];
    char_input[1] = buf [1];
    if (atoi(char_input)>=5){
      TxPower = atoi(char_input);
      }
      else if (atoi(char_input)==1){
        SLEEP_TIME_MIN = 1; // set new sleeping interval between two measurements to 1 minute
      }
      else if (atoi(char_input)==2){
        SLEEP_TIME_MIN = 60; // set new sleeping interval between two measurements to 60 minutes
      }
      else if (atoi(char_input)==3){
        SLEEP_TIME_MIN = 120; // set new sleeping interval between two measurements to 120 minutes
      }
      else if (atoi(char_input)==4){
        SLEEP_TIME_MIN = 240; // set new sleeping interval between two measurements to 240 minutes
      }
//      Serial.print("RSSI: ");
//      Serial.println(rf95.lastRssi(), DEC);
      rf95.setTxPower(TxPower, false);
//      Serial.println("New Tx power: ");
//      Serial.println(TxPower);
   }  
   else
   {
    Serial.println("Receive failed");
   }
  }
  else
  {
    Serial.println("No reply, is there a listener around?");
  }
    
  }                 
}
