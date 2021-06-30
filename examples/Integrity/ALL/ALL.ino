#include "LittleFS.h"

/* NOTES on LittleFS and Integrity testa
 - Sketches are a Media specific subset of tests done during development
 - When the Media is properly connected, it allows repeated FS file access to fill and verify function
 - The goal then was to repeatedly Create files and then Add to the file, or Delete the file
 - This allowed verifying File and Directory data integrtty as the Media was filled and reused as the data moved
 - Other than for Media and device testing - the code provides examples of support for the commands shown.
 - It will also provide a rough estimate of data I/O speed for the Media at hand
 - There is a Case Sensitive command set active when run displayed on start or when '?' is entered
 - For the primary looping enter number 1-9, h, or k and 'c' for continuous. A running count will stop with '0'
 - 'd' for directory will walk the directory and verify against the expected number of files added/deleted
 - Many commands were added as LittleFS developed, early work has lots of verbose settings now TOGGLED off for speed
 -
 - Each named sketch works on a specific Media type as presented
 - there are various commands provided to demonstrate usage
 - All .size() functions return a 64 bit uint64_t - take care when printing
*/


//#define TEST_RAM
//#define TEST_SPI
//#define TEST_QSPI
//#define TEST_SPI_NAND
#define TEST_QSPI_NAND
//#define TEST_PROG
//#define TEST_MRAM

// Set for SPI usage
//const int FlashChipSelect = 10; // AUDIO BOARD
const int FlashChipSelect = 4; // PJRC Mem board 64MB on #5, #6 : NAND 1Gb on #3, 2GB on #4
//const int FlashChipSelect = 5; // PJRC Mem board 64MB on #5, #6 : NAND 1Gb on #3, 2GB on #4
//const int FlashChipSelect = 6; // digital pin for flash chip CS pin


#ifdef TEST_RAM
LittleFS_RAM myfs;
// RUNTIME :: extern "C" uint8_t external_psram_size;
EXTMEM char buf[MYPSRAM * 1024 * 1024];	// USE DMAMEM for more memory than ITCM allows - or remove
//DMAMEM char buf[490000];	// USE DMAMEM for more memory than ITCM allows - or remove
char szDiskMem[] = "RAM_DISK";
#elif defined(TEST_SPI)
//const int FlashChipSelect = 10; // Arduino 101 built-in SPI Flash
#define FORMATSPI
//#define FORMATSPI2
LittleFS_SPIFlash myfs;
char szDiskMem[] = "SPI_DISK";
#elif defined(TEST_MRAM)
//const int FlashChipSelect = 10; // Arduino 101 built-in SPI Flash
LittleFS_SPIFram myfs;
char szDiskMem[] = "FRAM_DISK";
#elif defined(TEST_PROG)
LittleFS_Program myfs;
char szDiskMem[] = "PRO_DISK";
#elif defined(TEST_QSPI_NAND)
char szDiskMem[] = "QSPI_NAND";
LittleFS_QPINAND myfs;
#elif defined(TEST_SPI_NAND)
char szDiskMem[] = "SPI_NAND";
LittleFS_SPINAND myfs;
#else // TEST_QSPI
LittleFS_QSPIFlash myfs;
char szDiskMem[] = "QSPI_DISK";

#endif


// Adjust these for amount of disk space used in iterations
#define MAXNUM 26	// Number of files : ALPHA A-Z is MAX of 26, less for fewer files
#define NUMDIRS 12  // Number of Directories to use 0 is Rootonly
#define BIGADD 2024	// bytes added each pass - bigger will quickly consume more space
#define SUBADD 512	// bytes added each pass (*times file number)
#define MAXFILL 2048 // 66000	// ZERO to disable :: Prevent iterations from over filling - require this much free

// These can likely be left unchanged
#define MYBLKSIZE 2048 // 2048
#define SLACK_SPACE	40960 // allow for overhead slack space :: WORKS on FLASH {some need more with small alloc units}
uint32_t DELSTART = 3; // originally was 3 + higher bias more to writes and larger files - lower odd
#define DELDELAY 0 	// delay before DEL files : delayMicroseconds
#define ADDDELAY 0 	// delay on ADD FILE : delayMicroseconds

// Various Globals
const uint32_t lowOffset = 'a' - 'A';
const uint32_t lowShift = 13;
uint32_t errsLFS = 0, warnLFS = 0; // Track warnings or Errors
uint32_t lCnt = 0, LoopCnt = 0; // loop counters
uint64_t rdCnt = 0, wrCnt = 0; // Track Bytes Read and Written
int filecount = 0;
int loopLimit = 0; // -1 continuous, otherwise # to count down to 0
bool pauseDir = false;  // Start Pause on each off
bool showDir =  false;  // false Start Dir on each off
bool bDirVerify =  false;  // false Start Dir on each off
bool bWriteVerify = true;  // Verify on Write Toggle
bool bAutoFormat =  false;  // false Auto formatUnused() off
bool bCheckFormat =  false;  // false CheckFormat
bool bCheckUsed =  false;  // false CheckUsed
uint32_t res = 0; // for formatUnused
File file3; // Single FILE used for all functions

void setup() {
	while (!Serial) ; // wait
	Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
	Serial.println("LittleFS Test : File Integrity"); delay(5);

#ifdef TEST_RAM
	if (!myfs.begin(buf, sizeof(buf))) {
#elif defined(TEST_RAM2)
	if (!myfs.begin(buf, sizeof(buf), MYBLKSIZE )) {
#elif defined(TEST_SPI)
#ifdef FORMATSPI
	if (!myfs.begin( FlashChipSelect )) {
#elif defined(FORMATSPI2)
	pinMode(FlashChipSelect, OUTPUT);
	digitalWriteFast(FlashChipSelect, LOW);
	SPI2.setMOSI(50);
	SPI2.setMISO(54);
	SPI2.setSCK(49);
	SPI2.begin();
	if (!myfs.begin(51, SPI2)) {
#endif
#elif defined(TEST_MRAM)
	if (!myfs.begin( FlashChipSelect )) {
#elif defined(TEST_SPI_NAND)
	if (!myfs.begin( FlashChipSelect )) {
#elif defined(TEST_PROG)
	if (!myfs.begin(1024 * 1024 * 6)) {
#else
	if (!myfs.begin()) {
#endif
		Serial.printf("Error starting %s\n", szDiskMem);
		checkInput( 1 );
	}
	filecount = printDirectoryFilecount( myfs.open("/") );  // Set base value of filecount for disk
	printDirectory();
	parseCmd( '?' );
	makeRootDirs();
	checkInput( 1 );
	filecount = printDirectoryFilecount( myfs.open("/") );  // Set base value of filecount for disk
	printDirectory();
}


void loop() {
	char szDir[16];
	LoopCnt++;
	uint32_t chStep;
	if ( loopLimit != 0 ) {
		for ( uint32_t ii = 0; ii <= NUMDIRS && ( loopLimit != 0 ); ii++ )
		{
			if ( ii == 0 )
				sprintf( szDir, "/" );
			else
				sprintf( szDir, "/%lu_dir", ii );
			chStep = fileCycle(szDir);
			if ( bAutoFormat && !(lCnt % 5) ) res = loopAutoFormat( 20, res );
			while ( chStep != fileCycle(szDir) && ( loopLimit != 0 ) ) {
				if ( bAutoFormat && !(lCnt % 5) ) res = loopAutoFormat( 12, res ); // how often and how many depends on media and sizes
				checkInput( 0 ); // user input can 0 loopLimit
			}
		}
		checkInput( 0 );
		if ( loopLimit > 0 ) // -1 means continuous
			loopLimit--;
	}
	else
		checkInput( 1 );
}
