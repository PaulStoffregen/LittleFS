#include <LittleFS.h>
#include <stdarg.h>
uint64_t fTot, totSize1;

extern "C" void debug_print(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  for (; *format != 0; format++) { // no-frills stand-alone printf
    if (*format == '%') {
      ++format;
      if (*format == '%') goto out;
      if (*format == '-') format++; // ignore size
      while (*format >= '0' && *format <= '9') format++; // ignore size
      if (*format == 'l') format++; // ignore long
      if (*format == '\0') break;
      if (*format == 's') {
        debug_print((char *)va_arg(args, int));
      } else if (*format == 'd') {
        Serial.print(va_arg(args, int));
      } else if (*format == 'u') {
        Serial.print(va_arg(args, unsigned int));
      } else if (*format == 'x' || *format == 'X') {
        Serial.print(va_arg(args, int), HEX);
      } else if (*format == 'c' ) {
        Serial.print((char)va_arg(args, int));
      } else if (*format == 'p' ) { Serial.print(va_arg(args, int), HEX);}
    } else {
out:
      if (*format == '\n') Serial.print('\r');
      Serial.print(*format);
    }
  }
  va_end(args);
}
//LittleFS_QSPIFlash myfs;
//LittleFS_Program myfs;
//LittleFS_SPIFlash myfs;
//LittleFS_SPIFram myfs;
//LittleFS_SPINAND myfs;
LittleFS_QPINAND myfs;
#define chipSelect 3

File file, file1, file3;


#include <ctime>


void setup() {
  //pinMode(13, OUTPUT);
  pinMode(chipSelect, OUTPUT);
  digitalWrite(chipSelect, HIGH);
  while (!Serial) ; // wait

  delay(1000);
  Serial.println("LittleFS Test"); delay(5);
  if (!myfs.begin()) {
  //if (!myfs.begin(chipSelect)) {
    Serial.println("Error starting spidisk");
    while (1) ;
  }


  Serial.printf("TotalSize (Bytes): %d\n", myfs.totalSize());
  //myfs.deviceErase();

  delay(1000);
  Serial.println("started");
  //printDirectory();
  delay(10);
  Serial.println("MAKE files");
  myfs.mkdir("structureData1");
  printDirectory();
  file = myfs.open("structureData1/temp_test.txt", FILE_WRITE);
  delay(10);
  file.println("SOME DATA TO TEST");
  file.close();
  file = myfs.open("temp_test1.txt", FILE_WRITE);
  delay(10);
  file.println("SOME DATA TO TEST");
  file.close();
  file = myfs.open("temp_test2.txt", FILE_WRITE);
  delay(10);
  file.println("SOME DATA TO TEST");
  file.close();

  uint8_t buffer[] = "Test for SOME DATA TO TEST";
  Serial.println("--------------");
  file = myfs.open("temp_test3.txt", FILE_WRITE);
  uint16_t writeSize = sizeof(buffer);
  uint16_t write2Buffer = 512 / writeSize;
  //Serial.printf("%d, %d\n", writeSize, write2Buffer);

  uint8_t tempBuffer[512];
  memset(tempBuffer, 0xFF, 512);
  for (uint16_t j = 0; j < write2Buffer; j++) {
    for (uint16_t i = 0; i < writeSize; i++) {
      tempBuffer[j * writeSize + i] = buffer[i];
    }
  }
  for (uint16_t j = 0; j < 2; j++)
    file.write(tempBuffer, sizeof(tempBuffer));
  delay(10);

  //file.println("SOME DATA TO TEST");
  file.close();
  //file = myfs.open("temp_test3.txt", FILE_WRITE);
  //delay(10);
  //file.write(buffer, sizeof(buffer));
  //file.close();



  delay(100);
  printDirectory();
  delay(10);

  Serial.printf("Disk Usuage:\n");
  Serial.printf("Bytes Used: %llu, Bytes Total:%llu\n", myfs.usedSize(), myfs.totalSize());

  uint8_t buf[2048];
  memset(buf, 0, 2048);
  file = myfs.open("temp_test3.txt", FILE_READ);
  file.read(buf, 2048);
  for (uint16_t i = 0; i < 15; i++) {
    for (uint16_t j = 0; j < sizeof(buffer); j++) {
      Serial.printf("%c", buf[j + sizeof(buffer)*i]);
    } Serial.println();
  }
  file.close();
//Serial.println("Running Big File");
  bigFile();
  time_t t = rtc_get();
  Serial.print(std::ctime(&t));

  //myfs.rename("file10", "file10.txt");
  //myfs.rename("file20", "file20.txt");
  //printDirectory();

}

void bigFile() {
  char someData[2048];
  memset( someData, 'z', 2048 );
  file = myfs.open("bigfile.txt  ", FILE_WRITE);
  file.write(someData, sizeof(someData));

  for (uint16_t j = 0; j < 300; j++)
    file.write(someData, sizeof(someData));
  file.close();
  printDirectory();
}

void loop() {
  if ( Serial.available() ) {
    char rr;
    rr = Serial.read();
    if (rr == 'B') bigFile2MB(1);
    if (rr == 'b') bigFile2MB(0);
    if (rr == 'q') myfs.quickFormat();
    if (rr == 'F') myfs.lowLevelFormat('.');
    if (rr == 'f') myfs.formatUnused( 0 , 0 );

    printDirectory();
  time_t t = rtc_get();
  Serial.print(std::ctime(&t));
  }
}

void printDirectory() {

  Serial.println("printDirectory\n--------------");
  printDirectory(myfs.open("/"), 0);
  Serial.println();
}


void printDirectory(File dir, int numTabs) {
  //dir.whoami();
  uint64_t fSize = 0;
  uint32_t dCnt = 0, fCnt = 0;
  if ( 0 == dir ) {
    Serial.printf( "\t>>>\t>>>>> No Dir\n" );
    return;
  }
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      Serial.printf("\n %u dirs with %u files of Size %u Bytes\n", dCnt, fCnt, fSize);
      fTot += fCnt;
      totSize1 += fSize;
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }

    if (entry.isDirectory()) {
      Serial.print("DIR\t");
      dCnt++;
    } else {
      Serial.print("FILE\t");
      fCnt++;
      fSize += entry.size();
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println(" / ");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
    //Serial.flush();
  }
}

void bigFile2MB( int doThis ) {
  char myFile[] = "/0_2MBfile.txt";
  char fileID = '0' - 1;

  if ( 0 == doThis ) {  // delete File
    Serial.printf( "\nDelete with read verify all #bigfile's\n");
    do {
      fileID++;
      myFile[1] = fileID;
      if ( myfs.exists(myFile) && bigVerify( myFile, fileID) ) {
        //filecount--;
        myfs.remove(myFile);
      }
      else break; // no more of these
    } while ( 1 );
  }
  else {  // FILL DISK
    lfs_ssize_t resW = 1;
    char someData[2048];
    uint32_t xx, toWrite;
    toWrite = 2048 * 1000;
    if ( toWrite > (65535 + (myfs.totalSize() - myfs.usedSize()) ) ) {
      Serial.print( "Disk too full! DO :: q or F");
      return;
    }
    xx = toWrite;
    Serial.printf( "\nStart Big write of %u Bytes", xx);
    uint32_t timeMe = micros();
    file3 = nullptr;
    do {
      if ( file3 ) file3.close();
      fileID++;
      myFile[1] = fileID;
      file3 = myfs.open(myFile, FILE_WRITE);
    } while ( fileID < '9' && file3.size() > 0);
    if ( fileID == '9' ) {
      Serial.print( "Disk has 9 FILES 0-8! DO :: b or q or F");
      return;
    }
    memset( someData, fileID, 2048 );
    int hh = 0;
    while ( toWrite >= 2048 && resW > 0 ) {
      resW = file3.write( someData , 2048 );
      hh++;
      if ( !(hh % 40) ) Serial.print('.');
      toWrite -= 2048;
    }
    xx -= toWrite;
    file3.close();
    timeMe = micros() - timeMe;
    file3 = myfs.open(myFile, FILE_WRITE);
    if ( file3.size() > 0 ) {
      //filecount++;
      Serial.printf( "\nBig write %s took %5.2f Sec for %lu Bytes : file3.size()=%llu", myFile , timeMe / 1000000.0, xx, file3.size() );
    }
    if ( file3 != 0 ) file3.close();
    Serial.printf( "\n\tBig write KBytes per second %5.2f \n", xx / (timeMe / 1000.0) );
    Serial.printf("\nBytes Used: %llu, Bytes Total:%llu\n", myfs.usedSize(), myfs.totalSize());
    if ( resW < 0 ) {
      Serial.printf( "\nBig write ERR# %i 0x%X \n", resW, resW );
      //errsLFS++;
      myfs.remove(myFile);
    }
  }
}

bool bigVerify( char szPath[], char chNow ) {
  uint32_t timeMe = micros();
  file3 = myfs.open(szPath);
  if ( 0 == file3 ) {
    return false;
  }
  char mm;
  uint32_t ii = 0;
  uint32_t kk = file3.size() / 50;
  Serial.printf( "\tVerify %s bytes %llu : ", szPath, file3.size() );
  while ( file3.available() ) {
    file3.read( &mm , 1 );
    //rdCnt++;
    ii++;
    if ( !(ii % kk) ) Serial.print('.');
    if ( chNow != mm ) {
      Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow, mm, mm, ii );
      //parseCmd( '0' );
      //errsLFS++;
      //checkInput( 1 );
      break;
    }
  }
  if (ii != file3.size()) {
    Serial.printf( "\n\tRead Count fail! :: read %u != f.size %llu\n", ii, file3.size() );
    //parseCmd( '0' );
    //errsLFS++;
    //checkInput( 1 );  // PAUSE on CmdLine
  }
  else
    Serial.printf( "\tGOOD! >>  bytes %lu", ii );
  file3.close();
  timeMe = micros() - timeMe;
  Serial.printf( "\n\tBig read&compare KBytes per second %5.2f \n", ii / (timeMe / 1000.0) );
  if ( 0 == ii ) return false;
  return true;
}
