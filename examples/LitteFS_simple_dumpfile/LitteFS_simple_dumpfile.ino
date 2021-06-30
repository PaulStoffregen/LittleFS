/*
  LittleFS file dump
 
 This example shows how to read a file from Flash using the
 LittleFS library and send it over the serial port.
   
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
    ; // wait for serial port to connect.
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

  //lets print directory
  // After the SD card is initialized, you can access is using the ordinary
  // SD library functions, regardless of whether it was initialized by
  // SD library SD.begin() or SdFat library SD.sdfs.begin().
  //
  Serial.println("Print directory using SD functions");
  File root = myfs.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break; // no more files
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
    } else {
      printSpaces(40 - strlen(entry.name()));
      Serial.print("  ");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
  
  // open the file.
  File dataFile = myfs.open("datalog.txt");

  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  }  
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  } 
}

void loop()
{
}

void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    Serial.print(" ");
  }
}
