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


#define SLACK_SPACE	40960 // allow for overhead slack space :: WORKS on FLASH {some need more with small alloc units}
#define ROOTONLY // NORMAL is NOT DEFINED!
#define NUMDIRS 2  // When not ROOTONLY must be 1 or more
#define MYBLKSIZE 2048 // 2048

#define TEST_MRAM

// Set for SPI usage
const int FlashChipSelect = 10; // AUDIO BOARD is 10, other set as connected.
LittleFS_SPIFram myfs;
char szDiskMem[] = "FRAM_DISK";

uint32_t DELSTART = 3; // originally was 3 + higher bias more to writes and larger files - lower odd
#define SUBADD 512	// bytes added each pass (*times file number)
#define BIGADD 1024	// bytes added each pass - bigger will quickly consume more space
#define MAXNUM 12	// ALPHA A-Z is 26, less for fewer files
#define MAXFILL 2048 // 66000	// ZERO to disable :: Prevent iterations from over filling - require this much free
#define DELDELAY 0 	// delay before DEL files : delayMicroseconds
#define ADDDELAY 0 	// delay on ADD FILE : delayMicroseconds

const uint32_t lowOffset = 'a' - 'A';
const uint32_t lowShift = 13;
uint32_t errsLFS = 0;
uint32_t warnLFS = 0;
uint32_t lCnt = 0;
uint32_t LoopCnt = 0;
uint64_t rdCnt = 0;
uint64_t wrCnt = 0;
int filecount = 0;

File file3;
void setup() {
	while (!Serial) ; // wait
	Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
	Serial.println("LittleFS Test : File Integrity"); delay(5);

	if (!myfs.begin( FlashChipSelect )) {
		Serial.printf("Error starting %s\n", szDiskMem);
		checkInput( 1 );
	}
	filecount = printDirectoryFilecount( myfs.open("/") );  // Set base value of filecount for disk
	printDirectory();
	parseCmd( '?' );
#ifndef ROOTONLY // make subdirs if !ROOTONLY
	makeRootDirs();
#endif
	checkInput( 1 );
	filecount = printDirectoryFilecount( myfs.open("/") );  // Set base value of filecount for disk
	printDirectory();
}

int loopLimit = 0; // -1 continuous, otherwise # to count down to 0
bool pauseDir = false;  // Start Pause on each off
bool showDir =  false;  // false Start Dir on each off
bool bDirVerify =  false;  // false Start Dir on each off
bool bWriteVerify = true;  // Verify on Write Toggle
bool bAutoFormat =  false;  // false Auto formatUnused() off
bool bCheckFormat =  false;  // false CheckFormat
bool bCheckUsed =  false;  // false CheckUsed
uint32_t res = 0; // for formatUnused
void loop() {
	char szDir[16];
	LoopCnt++;
	uint32_t chStep;
	if ( loopLimit != 0 ) {
#ifdef ROOTONLY // ii=1-NUMDIRS are subdirs. #0 is Root
		for ( uint32_t ii = 0; ii < 1 && ( loopLimit != 0 ); ii++ )
#else
		for ( uint32_t ii = 0; ii < NUMDIRS && ( loopLimit != 0 ); ii++ )
#endif
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
