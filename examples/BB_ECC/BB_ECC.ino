#include <LittleFS.h>

//#define TEST_SPI_NAND
#define TEST_QSPI_NAND

#define USE_W25N01 1
//#define USE_W25N02 1
//#define UES_W25M02 1

// Set for SPI usage
//const int FlashChipSelect = 10; // AUDIO BOARD
#if defined(USE_W25N01)
  const int FlashChipSelect = 3; // PJRC Mem board 64MB on #5, #6 : NAND 1Gb on #3, 2GB on #4
  #define flashBufferSize 2112   //2,112 bytes, 64 ecc + 2048 data buffer
  #define dies = 1;
  uint16_t block_cnt =  1024;
#elif defined(USE_W25N02)
  const int FlashChipSelect = 4; // PJRC Mem board 64MB on #5, #6 : NAND 1Gb on #3, 2GB on #4
  #define flashBufferSize 2176  //for the n02 nand flash
  #define dies = 2;
  uint16_t block_cnt =  1024 * 2;
#else //W25M02
  const int FlashChipSelect = 6; // digital pin for flash chip CS pin
  #define flashBufferSize 2112   //2,112 bytes, 64 ecc + 2048 data buffer
  #define dies = 2;
  uint16_t block_cnt = 1024 * 2;
#endif
//const int FlashChipSelect = 5; // PJRC Mem board 64MB on #5, #6 : NAND 1Gb on #3, 2GB on #4
//const int FlashChipSelect = 6; // digital pin for flash chip CS pin

//All three NANDs have the same block size
#define blockSize 131072

#if defined(TEST_QSPI_NAND)
char szDiskMem[] = "QPI_NAND";
LittleFS_QPINAND myNAND;
#elif defined(TEST_SPI_NAND)
char szDiskMem[] = "SPI_NAND";
LittleFS_SPINAND myNAND;
#endif


uint8_t buffer[flashBufferSize];

uint16_t LBA[20], PBA[20], LUT_STATUS[20], openEntries;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Begin Init");

  #if defined(TEST_SPI_NAND)
    if (!myNAND.begin( FlashChipSelect )) {
  #else
    if (!myNAND.begin()) {
  #endif
  
    Serial.printf("Error starting %s\n", szDiskMem);
    while( 1 );
  }

  // A “Bad Block Marker” is a non-FFh data byte stored at Byte 0 of
  // Page 0 for each bad block. An additional marker is also stored in the first byte 
  // of the 64-Byte spare area.
  Serial.println("Reading Data in byte0 of the ECC Spare area:");
  memset(buffer, 0, flashBufferSize);
  for(uint32_t j = 0; j < block_cnt; j++){
    //uint8_t eccCode2;
    myNAND.readECC(j*blockSize, buffer, flashBufferSize); 
      if(j % 50 == 0) {
        Serial.print(".");
      }
      if(255 != buffer[2048]){
        Serial.printf("\nBad Block Marker Found in ECC Block in Page %d, 0x%2x\n", j, buffer[2048]);
      }
  }
  Serial.println("\nScan Finished!");
  
  //READ BB_LUT
  uint16_t LBA[20], PBA[20], openEntries = 0;
  uint8_t  LUT_STATUS[20];
  myNAND.readBBLUT(LBA, PBA, LUT_STATUS);
  Serial.println("Status of the links");
  for(uint16_t i = 0; i < 20; i++){
    if(LUT_STATUS[i] > 0) {
        Serial.printf("\tEntry: %d: Logical BA - %d, Physical BA - %d\n", i, LBA[i], PBA[i]);
        LUT_STATUS[i] = (uint8_t) (LBA[i] >> 14);
        if(LUT_STATUS[i] == 3) Serial.println("\t    This link is enabled and its a Valid Link!");
        if(LUT_STATUS[i] == 4) Serial.println("\t    This link was enabled but its not valid any more!");
        if(LUT_STATUS[i] == 1) Serial.println("\t    Not Applicable!");
    } else {
        openEntries++;
    }
  } Serial.printf("OpenEntries: %d\n", openEntries);
  

  //Test addBBLUT FUNCTION!
  myNAND.addBBLUT(2);
 
 
}

void loop() {}