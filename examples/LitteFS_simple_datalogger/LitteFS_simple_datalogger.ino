/*
  LittleFS  datalogger
 
 This example shows how to log data from three analog sensors
 to an storage device such as a FLASH.
 
 This example code is in the public domain.
 */

#include <LittleFS.h>

LittleFS_SPINAND  myfs;   //Specifies to use an SPI NOR Flash attached to SPI

const int chipSelect = 4;

void setup()
{

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    // wait for serial port to connect.
  }


  Serial.print("Initializing SPI FLASH...");

  // see if the Flash is present and can be initialized:
 if (!myfs.begin(chipSelect, SPI)) {
    Serial.printf("Error starting %s\n", "SPI FLASH");
    while (1) {
      // Flash error, so don't do anything more - stay stuck here
    }
  }
  Serial.println("Flash initialized.");
}

void loop()
{
  // make a string for assembling the data to log:
  String dataString = "";

  // read three sensors and append to the string:
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 2) {
      dataString += ",";
    }
  }

  // open the file.
  File dataFile = myfs.open("datalog.txt", FILE_WRITE);
  
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  } else {
    // if the file isn't open, pop up an error:
    Serial.println("error opening datalog.txt");
  }
  delay(100); // run at a reasonable not-too-fast speed
}
