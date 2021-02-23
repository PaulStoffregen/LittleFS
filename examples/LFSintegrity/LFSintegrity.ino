//#include <LittleFS.h>
#include <LittleFS_NAND.h>

#define HALFCUT  // HALFCUT defined to fill half the disk
//#define ROOTONLY // NORMAL is NOT DEFINED!
#define NUMDIRS 32  // When not ROOTONLY must be 1 or more
#define MYPSRAM 8	// compile time PSRAM size
#define MYBLKSIZE 2048 // 2048
//#define SHOW_YIELD_CNT  1 // uncommented shows calls to Yield per second

//#define TEST_RAM
//#define TEST_SPI
#define TEST_QSPI
//#define TEST_SPI_NAND
//#define TEST_QSPI_NAND
//#define TEST_PROG
//#define TEST_MRAM

IntervalTimer clocked10ms;
uint32_t yCalls = 0;
uint32_t yCallsMax = 0;
uint32_t yCallsLast = 0;
uint32_t yCallsIdx = 0;
uint32_t yCallsSum = 0;
#ifdef SHOW_YIELD_CNT
void yield() {
	yCalls++;
}
#endif
void clock_isr() {
	yCallsIdx++;
	if ( yCallsIdx >= 100 ) {
		if (yCallsSum > 0 )
			Serial.printf( "\n yps=%lu [mx=%lu]\t", yCallsSum, yCallsMax * 100);
		yCallsIdx = 0;
		yCallsSum = 0;
		yCallsMax = 0;
	}
	yCallsSum += yCalls - yCallsLast;
	if ( yCalls - yCallsLast > yCallsMax ) yCallsMax = yCalls - yCallsLast;
	yCallsLast = yCalls;
}

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

File file3;

uint32_t DELSTART = 3; // originally was 3 + higher bias more to writes and larger files - lower odd
#define SUBADD 512	// bytes added each pass (*times file number)
#define BIGADD 1024	// bytes added each pass - bigger will quickly consume more space
#define MAXNUM 26	// ALPHA A-Z is 26, less for fewer files
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
	// parseCmd( 'F' ); // ENABLE this if disk won't allow startup
	filecount = printDirectoryFilecount( myfs.open("/") );  // Set base value of filecount for disk
	printDirectory();
	parseCmd( '?' );
#ifndef ROOTONLY // make subdirs if !ROOTONLY
	makeRootDirs();
#endif
	checkInput( 1 );
	filecount = printDirectoryFilecount( myfs.open("/") );  // Set base value of filecount for disk
	printDirectory();
#ifdef SHOW_YIELD_CNT
	clocked10ms.begin(clock_isr, 10000);
#endif
	while ( 1 ) {
		loopX(); // Avoid end of loop() yield so the clock_isr() isn't noisy when used.
#ifndef SHOW_YIELD_CNT
		yield(); // simulate normal exec
#endif
	}
}

void makeRootDirs() {
	char szDir[16];
	for ( uint32_t ii = 1; ii <= NUMDIRS; ii++ ) {
		sprintf( szDir, "/%lu_dir", ii );
		myfs.mkdir( szDir );
	}
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
void loop() { }
void loopX() {
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
//			if ( bAutoFormat && !(lCnt % 20) ) res = loopAutoFormat( 6, res );
			while ( chStep != fileCycle(szDir) && ( loopLimit != 0 ) ) {
				if ( bAutoFormat && !(lCnt % 5) ) res = loopAutoFormat( 12, res ); // how often and how many depends on media and sizes
				//if ( bAutoFormat && !(lCnt % 20) ) res = loopAutoFormat( 6, res );
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
uint32_t loopAutoFormat( uint32_t cnt, uint32_t myres ) {
	uint32_t retres;
	retres = myfs.formatUnused( cnt, myres );
	Serial.printf("\t fmtU @ %lu - %lu \n", myres, retres );
	return retres;

}

char szInputs[] = "0123456789RdwcghkFqvplmusSBbyYxfan+-?";
uint32_t lastTime;
void checkInput( int step ) { // prompt for input without user input with step != 0
	uint32_t nowTime = micros();

	char retVal = 0, temp;
	char *pTemp;
	if ( step != 0 ) {
		nowTime -= lastTime;
		Serial.printf( "[%6.2f M](%6.5f M elap) Awaiting input %s loops left %d >", millis() / 60000.0, nowTime / 60000000.0, szInputs, loopLimit );
	}
	else {
		if ( !Serial.available() ) return;
		nowTime -= lastTime;
		Serial.printf( "[%6.2f M](%6.2f elap) Awaiting input %s loops left %d >", millis() / 60000.0, nowTime / 60000000.0, szInputs, loopLimit );
		//Serial.printf( "[%6.2f] Awaiting input %s loops left %d >", millis() / 60000.0, szInputs, loopLimit );
		while ( Serial.available() ) {
			temp = Serial.read( );
			if ( (pTemp = strchr(szInputs, temp)) ) {
				retVal = pTemp[0];
				parseCmd( retVal );
			}
		}
	}
	while ( !Serial.available() );
	while ( Serial.available() ) {
		temp = Serial.read();
		if ( (pTemp = strchr(szInputs, temp)) ) {
			retVal = pTemp[0];
			parseCmd( retVal );
		}
	}
	Serial.print( '\n' );
	if ( '?' == retVal ) checkInput( 1 ); // recurse on '?' to allow command show and response
	return;
}
void parseCmd( char chIn ) { // pass chIn == '?' for help
	uint32_t timeMe;
		char szNone[]="";
	switch (chIn ) {
	case '?':
		Serial.printf( "%s\n", " 0, 1-9 '#' passes continue loop before Pause\n\
 'a' Auto formatUnused() during iterations - TOGGLE\n\
 'R' Restart Teensy\n\
 'd' Directory of LittleFS\n\
 'w' WIPE Directory of LittleFS\n\
 'b' big file delete\n\
 'B' BIG FILE MAKE\n\
 'S' FILE 2MB MAKE\n\
 's' FILE 2MB delete\n\
 'c' Continuous Loop\n\
 'g' run speedBench()\n\
 'h' Hundred loops\n\
 'k' Thousand loops\n\
 'F' LittleFS_ Low Level Format Disk \n\
 'f' LittleFS::formatUnused( ALL ) : DATA PRESERVED \n\
 'q' LittleFS_ Quick Format Disk \n\
 'v' Verbose All Dir Prints - TOGGLE\n\
 'p' Pause after all Dir prints - TOGGLE\n\
 'l' Show count of loop()'s, Bytes Read,Written\n\
 'm' Make ROOT dirs (needed after q/F format !ROOTONLY)\n\
 'u' Update Filecount\n\
 'x' Directory filecount verify - TOGGLE\n\
 'n' No verify on Write- TOGGLE\n\
 '+' more add to delete cycles\n\
 '-' fewer add to delete cycles\n\
 'y' reclaim 1 block :: myfs.formatUnused( 1 )\n\
 'Y' reclaim 15 blocks :: myfs.formatUnused( 15 )\n\
 '?' Help list" );
		break;
	case 'R':
		Serial.print(" RESTART Teensy ...");
		delay(100);
		SCB_AIRCR = 0x05FA0004;
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		loopLimit = chIn - '0';
		if ( chIn != '0' )	// Preserve elapsed time on Error or STOP command
			lastTime = micros();
		break;
	case 'b':
		bigFile( 0 ); // delete
		chIn = 0;
		break;
	case 'B':
		lastTime = micros();
		bigFile( 1 ); // CREATE
		chIn = 0;
		break;
	case 's':
		bigFile2MB( 0 ); // CREATE
		chIn = 0;
		break;
	case 'S':
		lastTime = micros();
		bigFile2MB( 1 ); // CREATE
		chIn = 0;
		break;
	case 'c':
		loopLimit = -1;
		break;
	case 'g':
		lastTime = micros();
		speedBench();
		chIn = 0;
		break;
	case 'd':
		Serial.print( " d\n" );
		lastTime = micros();
		printDirectory();
		Serial.print( '\n' );
		parseCmd( 'l' );
		checkInput( 1 );
		chIn = 0;
		break;
	case 'w':
		Serial.println("\nWipe All Files and DIRS:");
		deleteAllDirectory(myfs.open("/"), szNone );
		errsLFS = 0; // No Errors on new Format
		warnLFS = 0; // No warning on new Format
		chIn = 0;
		parseCmd( 'u' );
		break;
	case 'h':
		loopLimit = 100;
		lastTime = micros();
		break;
	case 'k':
		loopLimit = 1000;
		lastTime = micros();
		break;
	case 'F': // Low Level format
		Serial.print( "\nFormatting Low Level:\n\t" );
		lastTime = micros();
		timeMe = micros();
		myfs.lowLevelFormat('.');
		timeMe = micros() - timeMe;
		Serial.printf( "\n Done Formatting Low Level in %lu us.\n", timeMe );
		errsLFS = 0; // No Errors on new Format
		warnLFS = 0; // No warning on new Format
		bCheckFormat  = false;
		parseCmd( 'u' );
		break;
	case 'q': // quick format
		lastTime = micros();
		myfs.quickFormat();
		errsLFS = 0; // No Errors on new Format
		parseCmd( 'u' );
		break;
	case 'v': // verbose dir
		showDir = !showDir;
		showDir ? Serial.print(" Verbose on: ") : Serial.print(" Verbose off: ");
		chIn = 0;
		break;
	case 'p': // pause on dirs
		pauseDir = !pauseDir;
		pauseDir ? Serial.print(" Pause on: ") : Serial.print(" Pause off: ");
		chIn = 0;
		break;
	case 'x': // dir filecount Verify
		bDirVerify = !bDirVerify;
		bDirVerify ? Serial.print(" FileCnt on: ") : Serial.print(" FileCnt off: ");
		lastTime = micros();
		dirVerify();
		chIn = 0;
		break;
	case 'n': // No Verify on write
		bWriteVerify = !bWriteVerify;
		bWriteVerify ? Serial.print(" Write Verify on: ") : Serial.print(" Write Verify off: ");
		chIn = 0;
		break;
	case 'a': // Auto myfs.formatUnused() during iterations
		bAutoFormat = !bAutoFormat;
		bAutoFormat ? Serial.print(" \nAuto formatUnused() On: ") : Serial.print(" \nAuto formatUnused() Off: ");
		chIn = 0;
		break;
	case 'y': // Reclaim 1 unused format
		lastTime = micros();
		Serial.printf( "\n myfs.formatUnused( 1 ) ...\n" );
		timeMe = micros();
		res = myfs.formatUnused( 1, res );
		timeMe = micros() - timeMe;
		Serial.printf( "\n\t formatUnused :: Done Formatting Low Level in %lu us (last %lu).\n", timeMe, res );
		chIn = 0;
		break;
	case 'Y': // Reclaim 15 unused format
		lastTime = micros();
		Serial.printf( "\n myfs.formatUnused( 15 ) ...\n" );
		timeMe = micros();
		res = myfs.formatUnused( 15, res );
		timeMe = micros() - timeMe;
		Serial.printf( "\n\t formatUnused :: Done Formatting Low Level in %lu us (last %lu).\n", timeMe, res );
		chIn = 0;
		break;
	case 'f': // Reclaim all unused format
		lastTime = micros();
		Serial.printf( "\n myfs.formatUnused( 0 ) ...\n" );
		timeMe = micros();
		myfs.formatUnused( 0, 0 );
		timeMe = micros() - timeMe;
		Serial.printf( "\n\t formatUnused :: Done Formatting Low Level in %lu us.\n", timeMe );
		chIn = 0;
		break;
	case 'l': // Show Loop Count
		Serial.printf("\n\t Loop Count: %u (#fileCycle=%u), Bytes read %llu, written %llu, #Files=%u\n", LoopCnt, lCnt, rdCnt, wrCnt, filecount );
		if ( 0 != errsLFS )
			Serial.printf("\t ERROR COUNT =%u\n", errsLFS );
		if ( 0 != warnLFS )
			Serial.printf("\t Free Space Warn COUNT =%u\n", warnLFS );
		dirVerify();
		chIn = 0;
		break;
	case 'm':
		Serial.printf("m \n\t Making Root Dirs\n" );
		makeRootDirs();
		parseCmd( 'd' );
		chIn = 0;
		break;
	case 'u': // Show Loop Count
		filecount = printDirectoryFilecount( myfs.open("/") );
		Serial.printf("u \n\t Updated filecount %u\n", filecount );
		chIn = 0;
		break;
	case '+': // increase add cycles
		DELSTART++;
		Serial.printf("+\n Deletes start after %u cycles ", DELSTART);
		chIn = 0;
		break;
	case '-': // decrease add cycles
		DELSTART--;
		if ( DELSTART < 1 ) DELSTART = 1;
		Serial.printf("-\n Deletes start after %u cycles ", DELSTART);
		chIn = 0;
		break;
	default:
		Serial.println( chIn ); // never see without unhandled char in szInputs[]
		break;
	}
	if ( 0 != chIn ) Serial.print( chIn );
}

uint32_t fTot, totSize;
void printDirectory() {
	fTot = 0, totSize = 0;
	Serial.printf("printDirectory %s\n--------------\n", szDiskMem);
	printDirectory(myfs.open("/"), 0);
	Serial.printf(" %Total %u files of Size %u Bytes\n", fTot, totSize);
	Serial.printf("Bytes Used: %llu, Bytes Total:%llu\n", myfs.usedSize(), myfs.totalSize());
}

void deleteAllDirectory(File dir, char *fullPath ) {
	char myPath[ 256 ] = "";
	while (true) {
		File entry =  dir.openNextFile();
		if (! entry) {
			// no more files
			break;
		}
		if (entry.isDirectory()) {
			strcpy( myPath, fullPath);
			strcat( myPath, entry.name());
			strcat( myPath, "/");
			deleteAllDirectory(entry, myPath);
			entry.close();
			if ( !myfs.remove( myPath ) )
				Serial.print( "  Fail remove DIR>\t");
			else
				Serial.print( "  Removed DIR>\t");
			Serial.println( myPath );

		} else {
			strcpy( myPath, fullPath);
			strcat( myPath, entry.name());
			entry.close();
			if ( !myfs.remove( myPath ) )
				Serial.print( "\tFail remove>\t");
			else
				Serial.print( "\tRemoved>\t");
			Serial.println( myPath );
		}
	}
}

int printDirectoryFilecount(File dir) {
	unsigned int filecnt = 0;
	while (true) {
		File entry =  dir.openNextFile();
		if (! entry) {
			// no more files
			break;
		}
		if (entry.isDirectory()) {
			filecnt += printDirectoryFilecount(entry);
		} else {
			filecnt++;
		}
		entry.close();
	}
	return filecnt;
}

void printDirectory(File dir, int numTabs) {
	//dir.whoami();
	uint32_t fSize = 0, dCnt = 0, fCnt = 0;
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
			totSize += fSize;
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

uint32_t cCnt = 0;
uint32_t fileCycle(const char *dir) {
	static char szFile[] = "_file.txt";
	char szPath[150];
	int ii;
	lCnt++;
	byte nNum = lCnt % MAXNUM;
	char chNow = 'A' + lCnt % MAXNUM;
	lfs_ssize_t resW = 1;
	uint32_t timeMeAll = micros();

	if ( dir[1] == 0 )	// catch root
		sprintf( szPath, "/%c%s", chNow, szFile );
	else
		sprintf( szPath, "%s/%c%s", dir, chNow, szFile );
	if ( cCnt >= DELSTART && myfs.exists(szPath) ) { // DELETE ALL KNOWN FILES
		if ( nNum == 1 ) {
			Serial.print( "\n == == ==   DELETE PASS START  == == == = \n");
			if ( showDir ) {
				printDirectory();
				Serial.print( " == == ==   DELETE PASS START  == == == = \n");
			}
			delayMicroseconds(DELDELAY);
		}
	}
	Serial.printf( ":: %s ", szPath );
	if ( cCnt >= DELSTART && myfs.exists(szPath) ) { // DELETE ALL KNOWN FILES
		readVerify( szPath, chNow );
		myfs.remove(szPath);
		filecount--;
		Serial.printf(" %s ----DEL----", szDiskMem);
		Serial.printf(" -- %c", chNow);
		if ( showDir ) {
			Serial.print("\n");
			printDirectory(myfs.open(dir), 1);
		}
		if ( pauseDir ) checkInput( 1 );
		Serial.println();
	}
	else {
		if ( bWriteVerify && myfs.totalSize() - myfs.usedSize() < MAXFILL ) {
			warnLFS++;
			Serial.printf( "\tXXX\tXXX\tXXX\tXXX\tSIZE WARNING { MAXFILL } \n" );
			cCnt = DELSTART;
			return cCnt;	// EARLY EXIT
		}
		if ( nNum == 0 ) {
			nNum = 10;
			cCnt++;
			if ( cCnt >= DELSTART + 2 ) cCnt = 0;
		}
		file3 = myfs.open(szPath, FILE_WRITE);
		if ( 0 == file3 ) {
			Serial.printf( "\tXXX\tXXX\tXXX\tXXX\tFail File open {mkdir?}\n" );
			delayMicroseconds(300000);
			checkInput( 1 );	// PAUSE on CmdLine
		}
		else {
			delayMicroseconds(ADDDELAY);
			char mm = chNow + lowOffset;
			uint32_t jj = file3.size() + 1;
			uint32_t timeMe = micros();
			for ( ii = 0; ii < (nNum * SUBADD + BIGADD ) && resW > 0; ii++ ) {
				if ( 0 == ((ii + jj) / lowShift) % 2 )
					resW = file3.write( &mm , 1 );
				else
					resW = file3.write( &chNow , 1 );
				wrCnt++;
				// if ( lCnt%100 == 50 ) mm='x'; // GENERATE ERROR to detect on DELETE read verify
			}
			file3.close();
			timeMe = micros() - timeMe;
			timeMeAll = micros() - timeMeAll;
			Serial.printf(" %s +++ Add [sz %u add %u] @KB/sec %5.2f {%5.2f} ", szDiskMem, jj - 1, ii, ii / (timeMe / 1000.0), ii / (timeMeAll / 1000.0) );
			if (resW < 0) {
				Serial.printf( "\n\twrite fail ERR# %i 0x%X \n", resW, resW );
				parseCmd( '0' );
				errsLFS++;
				checkInput( 1 );	// PAUSE on CmdLine
			}
			else if ( jj == 1 ) filecount++; // File Added
			Serial.printf(" ++ %c ", chNow);
			if ( bWriteVerify )
				readVerify( szPath, chNow );
			else
				Serial.print('\n');
			if ( showDir ) {
				Serial.print('\n');
				printDirectory(myfs.open(dir), 1);
			}
		}
		if ( pauseDir ) checkInput( 1 );
		//Serial.print("\n");
		delayMicroseconds(ADDDELAY);
	}
	checkInput( 0 ); // user stop request?
	if ( bDirVerify ) dirVerify();
	return cCnt;
}

void dirVerify() {
	uint32_t timeMe = micros();
	Serial.printf("\tdirV...");
	if ( filecount != printDirectoryFilecount( myfs.open("/") ) ) {
		Serial.printf( "\tFilecount mismatch %u != %u\n", filecount, printDirectoryFilecount( myfs.open("/") ) );
		parseCmd( '0' );
		errsLFS++;
		checkInput( 1 );	// PAUSE on CmdLine
	}
	timeMe = micros() - timeMe;
	Serial.printf("%lu_us\t", timeMe);
}

void readVerify( char szPath[], char chNow ) {
	uint32_t timeMe = micros();
	file3 = myfs.open(szPath);
	if ( 0 == file3 ) {
		Serial.printf( "\tV\t Fail File open %s\n", szPath );
		parseCmd( '0' );
		errsLFS++;
		checkInput( 1 );
	}
	char mm;
	char chNow2 = chNow + lowOffset;
	uint32_t ii = 0;
	while ( file3.available() ) {
		file3.read( &mm , 1 );
		rdCnt++;
		//Serial.print( mm ); // show chars as read
		ii++;
		if ( 0 == (ii / lowShift) % 2 ) {
			if ( chNow2 != mm ) {
				Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow2, mm, mm, ii );
				parseCmd( '0' );
				errsLFS++;
				checkInput( 1 );
				break;
			}
		}
		else {
			if ( chNow != mm ) {
				Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow, mm, mm, ii );
				parseCmd( '0' );
				errsLFS++;
				checkInput( 1 );
				break;
			}
		}
	}
	Serial.printf( "  Verify %s %uB ", szPath, ii );
	if (ii != file3.size()) {
		Serial.printf( "\n\tRead Count fail! :: read %u != f.size %llu", ii, file3.size() );
		parseCmd( '0' );
		errsLFS++;
		checkInput( 1 );	// PAUSE on CmdLine
	}
	file3.close();
	timeMe = micros() - timeMe;
	Serial.printf( " @KB/sec %5.2f \n", ii / (timeMe / 1000.0) );
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
		rdCnt++;
		ii++;
		if ( !(ii % kk) ) Serial.print('.');
		if ( chNow != mm ) {
			Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow, mm, mm, ii );
			parseCmd( '0' );
			errsLFS++;
			checkInput( 1 );
			break;
		}
	}
	if (ii != file3.size()) {
		Serial.printf( "\n\tRead Count fail! :: read %u != f.size %llu\n", ii, file3.size() );
		parseCmd( '0' );
		errsLFS++;
		checkInput( 1 );	// PAUSE on CmdLine
	}
	else
		Serial.printf( "\tGOOD! >>  bytes %lu", ii );
	file3.close();
	timeMe = micros() - timeMe;
	Serial.printf( "\n\tBig read&compare KBytes per second %5.2f \n", ii / (timeMe / 1000.0) );
	if ( 0 == ii ) return false;
	return true;
}


void bigFile( int doThis ) {
	char myFile[] = "/0_bigfile.txt";
	char fileID = '0' - 1;

	if ( 0 == doThis ) {	// delete File
		Serial.printf( "\nDelete with read verify all #bigfile's\n");
		do {
			fileID++;
			myFile[1] = fileID;
			if ( myfs.exists(myFile) && bigVerify( myFile, fileID) ) {
				filecount--;
				myfs.remove(myFile);
			}
			else break; // no more of these
		} while ( 1 );
	}
	else {	// FILL DISK
		lfs_ssize_t resW = 1;
		char someData[2048];
		uint32_t xx, toWrite;
		toWrite = (myfs.totalSize()) - myfs.usedSize();
		if ( toWrite < 65535 ) {
			Serial.print( "Disk too full! DO :: b or q or F");
			return;
		}
		toWrite -= 40960; // allow for slack space :: WORKS on FLASH?
#ifdef HALFCUT
		toWrite /= 2; // cutting to this works on LittleFS_RAM myfs - except reported file3.size()=2054847098 OR now 0
#endif
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
			Serial.print( "Disk has 9 halves 0-8! DO :: b or q or F");
			return;
		}
		memset( someData, fileID, 2048 );
		int hh = 0;
		while ( toWrite > 2048 && resW > 0 ) {
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
			filecount++;
			Serial.printf( "\nBig write %s took %5.2f Sec for %lu Bytes : file3.size()=%llu", myFile , timeMe / 1000000.0, xx, file3.size() );
		}
		if ( file3 != 0 ) file3.close();
		Serial.printf( "\n\tBig write KBytes per second %5.2f \n", xx / (timeMe / 1000.0) );
		Serial.printf("\nBytes Used: %llu, Bytes Total:%llu\n", myfs.usedSize(), myfs.totalSize());
		if ( resW < 0 ) {
			Serial.printf( "\nBig write ERR# %i 0x%X \n", resW, resW );
			errsLFS++;
			myfs.remove(myFile);
		}
	}
}

void bigFile2MB( int doThis ) {
	char myFile[] = "/0_2MBfile.txt";
	char fileID = '0' - 1;

	if ( 0 == doThis ) {	// delete File
		Serial.printf( "\nDelete with read verify all #bigfile's\n");
		do {
			fileID++;
			myFile[1] = fileID;
			if ( myfs.exists(myFile) && bigVerify( myFile, fileID) ) {
				filecount--;
				myfs.remove(myFile);
			}
			else break; // no more of these
		} while ( 1 );
	}
	else {	// FILL DISK
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
			Serial.print( "Disk has 9 files 0-8! DO :: b or q or F");
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
			filecount++;
			Serial.printf( "\nBig write %s took %5.2f Sec for %lu Bytes : file3.size()=%llu", myFile , timeMe / 1000000.0, xx, file3.size() );
		}
		if ( file3 != 0 ) file3.close();
		Serial.printf( "\n\tBig write KBytes per second %5.2f \n", xx / (timeMe / 1000.0) );
		Serial.printf("\nBytes Used: %llu, Bytes Total:%llu\n", myfs.usedSize(), myfs.totalSize());
		if ( resW < 0 ) {
			Serial.printf( "\nBig write ERR# %i 0x%X \n", resW, resW );
			errsLFS++;
			myfs.remove(myFile);
		}
	}
}

#include <sdios.h>
ArduinoOutStream cout(Serial);
File file;

// File size in bytes.
const uint32_t FILE_SIZE = 16 * 1024;

// Set SKIP_FIRST_LATENCY true if the first read/write to the SD can
// be avoid by writing a file header or reading the first record.
const bool SKIP_FIRST_LATENCY = true;

// Size of read/write.
const size_t BUF_SIZE = 2048;

// Write pass count.
const uint8_t WRITE_COUNT = 5;

// Read pass count.
const uint8_t READ_COUNT = 5;

//Block size for qspi
#define MYBLKSIZE 2048 // 2048

// Insure 4-byte alignment.
uint32_t buf32[(BUF_SIZE + 3) / 4];
uint8_t* bufA32 = (uint8_t*)buf32;

//Number of random reads
#define randomReads 1

void speedBench() {
	File file;
	float s;
	uint32_t t;
	uint32_t maxLatency;
	uint32_t minLatency;
	uint32_t totalLatency;
	bool skipLatency;

	myfs.remove("bench.dat");
	//for(uint8_t cnt=0; cnt < 10; cnt++) {

	// fill buf with known data
	if (BUF_SIZE > 1) {
		for (size_t i = 0; i < (BUF_SIZE - 2); i++) {
			bufA32[i] = 'A' + (i % 26);
		}
		bufA32[BUF_SIZE - 2] = '\r';
	}
	bufA32[BUF_SIZE - 1] = '\n';

	Serial.printf("%s Disk Stats:", szDiskMem );
	Serial.printf("Bytes Used: %llu, Bytes Total:%llu\n", myfs.usedSize(), myfs.totalSize());
	Serial.printf("%s Benchmark:\n", szDiskMem );
	cout << F("FILE_SIZE = ") << FILE_SIZE << endl;
	cout << F("BUF_SIZE = ") << BUF_SIZE << F(" bytes\n");
	cout << F("Starting write test, please wait.") << endl << endl;

	// do write test
	uint32_t n = FILE_SIZE / BUF_SIZE;
	cout << F("write speed and latency") << endl;
	cout << F("speed,max,min,avg") << endl;
	cout << F("KB/Sec,usec,usec,usec") << endl;

	// open or create file - truncate existing file.
	file = myfs.open("bench.dat", FILE_WRITE);

	for (uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++) {
		file.seek(0);

		maxLatency = 0;
		minLatency = 9999999;
		totalLatency = 0;
		skipLatency = SKIP_FIRST_LATENCY;
		t = millis();

		for (uint32_t i = 0; i < n; i++) {
			uint32_t m = micros();
			if (file.write(bufA32, BUF_SIZE) != BUF_SIZE) {
				Serial.println("write failed");
			}
			m = micros() - m;
			totalLatency += m;
			if (skipLatency) {
				// Wait until first write to SD, not just a copy to the cache.
				skipLatency = file.position() < 512;
			} else {
				if (maxLatency < m) {
					maxLatency = m;
				}
				if (minLatency > m) {
					minLatency = m;
				}
			}
		}

		t = millis() - t;
		s = file.size();
		cout << s / t << ',' << maxLatency << ',' << minLatency;
		cout << ',' << totalLatency / n << endl;
	}
	cout << endl << F("Starting sequential read test, please wait.") << endl;
	cout << endl << F("read speed and latency") << endl;
	cout << F("speed,max,min,avg") << endl;
	cout << F("KB/Sec,usec,usec,usec") << endl;

	// do read test
	for (uint8_t nTest = 0; nTest < READ_COUNT; nTest++) {
		file.seek(0);
		maxLatency = 0;
		minLatency = 9999999;
		totalLatency = 0;
		skipLatency = SKIP_FIRST_LATENCY;
		t = micros();
		for (uint32_t i = 0; i < n; i++) {
			bufA32[BUF_SIZE - 1] = 0;
			uint32_t m = micros();
			int32_t nr = file.read(bufA32, BUF_SIZE);
			if (nr != BUF_SIZE) {
				Serial.println("read failed");
			}
			m = micros() - m;
			totalLatency += m;
			if (bufA32[BUF_SIZE - 1] != '\n') {
				Serial.println("data check error");
			}
			if (skipLatency) {
				skipLatency = false;
			} else {
				if (maxLatency < m) {
					maxLatency = m;
				}
				if (minLatency > m) {
					minLatency = m;
				}
			}
		}

		s = file.size();

		t = micros() - t;
		cout << s * 1000 / t << ',' << maxLatency << ',' << minLatency;
		cout << ',' << totalLatency / n << endl;
	}

	cout << endl << F("Done") << endl;

	cout << endl << F("Starting random read test, please wait.") << endl;

	Serial.printf("Number of random reads: %d\n", randomReads);
	Serial.printf("Number of blocks: %d\n", n);

	cout << endl << F("read speed and latency") << endl;
	cout << F("speed,max,min,avg") << endl;

	// do read test
	for (uint8_t nTest = 0; nTest < READ_COUNT; nTest++) {
		file.seek(0);
		maxLatency = 0;
		minLatency = 0;
		totalLatency = 0;
		skipLatency = SKIP_FIRST_LATENCY;
		t = micros();
		for (uint32_t i = 0; i < randomReads; i++) {
			bufA32[BUF_SIZE - 1] = 0;
			uint32_t m = micros();
			uint32_t block_pos = random(0, (n - 1));
			uint32_t random_pos = block_pos * MYBLKSIZE;
			cout << "Position (bytes), Block: " << random_pos << ", ";
			cout << block_pos << endl;
			uint32_t startCNT = ARM_DWT_CYCCNT;
			file.seek(random_pos);
			int32_t nr = file.read(bufA32, BUF_SIZE);
			uint32_t endCNT = ARM_DWT_CYCCNT;
			cout << F("Read Time (ARM Cycle Delta): ") << endCNT - startCNT << endl;
			if (nr != BUF_SIZE) {
				Serial.println("read failed");
			}
			m = micros() - m;
			totalLatency += m;
			if (bufA32[BUF_SIZE - 1] != '\n') {
				Serial.println("data check error");
			}
			if (skipLatency) {
				skipLatency = false;
			} else {
				if (maxLatency < m) {
					maxLatency = m;
				}
				if (minLatency > m) {
					minLatency = m;
				}
			}
		}

		s = file.size();


		t = micros() - t;
		cout << F("KB/Sec,usec,usec,usec") << endl;
		cout << s * 1000 / t << ',' << maxLatency << ',' << minLatency;
		cout << ',' << totalLatency / n << endl;
	}
	cout << endl << F("Done") << endl;


	file.close();
	myfs.remove("bench.dat");

}
