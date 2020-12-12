#include <LittleFS.h>

#define HALFCUT  // HALFCUT defined to fill half the disk
//#define ROOTONLY // NORMAL is NOT DEFINED!
#define NUMDIRS 28  // When not ROOTONLY must be 1 or more

//#define TEST_RAM
//#define TEST_SPI
#define TEST_QSPI
//#define TEST_PROG

// Set for SPI usage
const int FlashChipSelect = 6; // digital pin for flash chip CS pin

#ifdef TEST_RAM
LittleFS_RAM myfs;
// RUNTIME :: extern "C" uint8_t external_psram_size;
EXTMEM char buf[16 * 1024 * 1024];	// USE DMAMEM for more memory than ITCM allows - or remove
//DMAMEM char buf[490000];	// USE DMAMEM for more memory than ITCM allows - or remove
char szDiskMem[] = "RAM_DISK";
#elif defined(TEST_SPI)
//const int FlashChipSelect = 21; // Arduino 101 built-in SPI Flash
#define FORMATSPI
//#define FORMATSPI2
LittleFS_SPIFlash myfs;
char szDiskMem[] = "SPI_DISK";
#elif defined(TEST_PROG)
LittleFS_Program myfs;
char szDiskMem[] = "PRO_DISK";
#else // TEST_QSPI
LittleFS_QSPIFlash myfs;
char szDiskMem[] = "QSPI_DISK";
#endif

File file3;

uint32_t DELSTART = 3; // originally was 3 + higher bias more to writes and larger files
#define SUBADD 1024	// bytes added each pass (*times file number)
#define BIGADD 2048	// bytes added each pass - bigger will quickly consume more space
#define MAXNUM 26	// ALPHA A-Z is 26, less for fewer files
#define DELDELAY 0 	// delay before DEL files : delayMicroseconds
#define ADDDELAY 0 	// delay on ADD FILE : delayMicroseconds

const uint32_t lowOffset = 'a' - 'A';
const uint32_t lowShift = 13;
uint32_t errsLFS = 0;
uint32_t lCnt = 0;
uint32_t LoopCnt = 0;
uint32_t rdCnt = 0;
uint32_t wrCnt = 0;
int filecount = 0;

void setup() {
	while (!Serial) ; // wait
	Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
	Serial.println("LittleFS Test : File Integrity"); delay(5);

#ifdef TEST_RAM
	if (!myfs.begin(buf, sizeof(buf))) {
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
#elif defined(TEST_PROG)
	if (!myfs.begin(1024 * 1024 * 4)) {
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
bool bCheckFormat =  false;  // false CheckFormat
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
			while ( chStep != fileCycle(szDir) && ( loopLimit != 0 ) ) checkInput( 0 ); // user input can 0 loopLimit
		}
		checkInput( 0 );
		if ( loopLimit > 0 ) // -1 means continuous
			loopLimit--;
	}
	else
		checkInput( 1 );
}

char szInputs[] = "0123456789RdchkFqvplmuxBbZ+-?";
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
	switch (chIn ) {
	case '?':
		Serial.printf( "%s\n", " 0, 1-9 '#' passes continue loop before Pause\n\
 'R' Restart Teensy\n\
 'd' Directory of LittleFS\n\
 'b' big file delete\n\
 'B' BIG FILE RUN\n\
 'c' Continuous Loop\n\
 'h' Hundred loops\n\
 'k' Thousand loops\n\
 'F' LittleFS_ Low Level Format Disk \n\
 'q' LittleFS_ Quick Format Disk \n\
 'v' Verbose All Dir Prints - TOGGLE\n\
 'p' Pause after all Dir prints - TOGGLE\n\
 'l' Show count of loop()'s, Bytes Read,Written\n\
 'm' Make ROOT dirs (needed after format !ROOTONLY)\n\
 'u' Update Filecount\n\
 'x' Directory filecount verify - TOGGLE\n\
 'Z' ZIPPY write after 'F'ormat - TOGGLE\n\
 '+' more add to delete cycles\n\
 '-' fewer add to delete cycles\n\
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
		lastTime = micros();
		break;
	case 'b':
		bigFile( 0 ); // delete
		break;
	case 'B':
		lastTime = micros();
		bigFile( 1 ); // CREATE
		chIn = 0;
		break;
	case 'c':
		loopLimit = -1;
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

	case 'Z':
		bCheckFormat  = !bCheckFormat;
		timeMe = micros();
		bCheckFormat = myfs.checkFormat( bCheckFormat );
		timeMe = micros() - timeMe;
		Serial.printf( "\n Done myfs.checkFormat() in %lu us.\n", timeMe );
		bCheckFormat ? Serial.print("\n CheckFormat On: ") : Serial.print(" CheckFormat Off: ");
		lastTime = micros();
		chIn = 0;
		break;

	case 'l': // Show Loop Count
		Serial.printf("\n\t Loop Count: %u (#fileCycle=%u), Bytes read %u, written %u, #Files=%u\n", LoopCnt, lCnt, rdCnt, wrCnt, filecount );
		if ( 0 != errsLFS )
			Serial.printf("\t ERROR COUNT =%u\n", errsLFS );
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
			for ( ii = 0; ii < (nNum * SUBADD + BIGADD ) && resW > 0; ii++ ) {
				if ( 0 == ((ii + jj) / lowShift) % 2 )
					resW = file3.write( &mm , 1 );
				else
					resW = file3.write( &chNow , 1 );
				wrCnt++;
				// if ( lCnt%100 == 50 ) mm='x'; // GENERATE ERROR to detect on DELETE read verify
			}
			file3.close();
			Serial.printf(" %s +++ Add +++ [sz %u add %u]", szDiskMem, jj - 1, ii);
			if (resW < 0) {
				Serial.printf( "\n\twrite fail ERR# %i 0x%X \n", resW, resW );
				parseCmd( '0' );
				errsLFS++;
				checkInput( 1 );	// PAUSE on CmdLine
			}
			else if ( jj == 1 ) filecount++; // File Added
			Serial.printf(" ++ %c ", chNow);
			readVerify( szPath, chNow );
			if ( showDir ) {
				Serial.print("\n");
				printDirectory(myfs.open(dir), 1);
			}
		}
		if ( pauseDir ) checkInput( 1 );
		Serial.print("\n");
		delayMicroseconds(ADDDELAY);
	}
	checkInput( 0 ); // user stop request?
	if ( bDirVerify ) dirVerify();
	return cCnt;
}

void dirVerify() {
	if ( filecount != printDirectoryFilecount( myfs.open("/") ) ) {
		Serial.printf( "\tFilecount mismatch %u != %u\n", filecount, printDirectoryFilecount( myfs.open("/") ) );
		parseCmd( '0' );
		errsLFS++;
		checkInput( 1 );	// PAUSE on CmdLine
	}
}

void readVerify( char szPath[], char chNow ) {
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
	Serial.printf( "\tVerify %s bytes %u ", szPath, ii );
	if (ii != file3.size()) {
		Serial.printf( "\n\tRead Count fail! :: read %u != f.size %llu", ii, file3.size() );
		parseCmd( '0' );
		errsLFS++;
		checkInput( 1 );	// PAUSE on CmdLine
	}
	file3.close();
}


void bigFile( int doThis ) {
	char myFile[] = "/0_bigfile.txt";
	char fileID = '0' - 1;

	if ( 0 == doThis ) {	// delete File
		do {
			fileID++;
			myFile[1] = fileID;
			filecount--;
		} while ( myfs.remove(myFile) );
		filecount++;
	}
	else {	// FILL DISK
		lfs_ssize_t resW = 1;
		char someData[4000];
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
		memset( someData, fileID, 4000 );
		int hh = 0;
		while ( toWrite > 4000 && resW > 0 ) {
			resW = file3.write( someData , 4000 );
			hh++;
			if ( !(hh % 40) ) Serial.print('.');
			toWrite -= 4000;
		}
		xx -= toWrite;
		file3.close();
		timeMe = micros() - timeMe;
		if ( resW < 0 ) {
			Serial.printf( "\nBig write ERR# %i 0x%X \n", resW, resW );
		}
		file3 = myfs.open(myFile, FILE_WRITE);
		if ( file3.size() > 0 ) filecount++;
		Serial.printf( "\nBig write %s took %5.2f Sec for %lu Bytes : file3.size()=%llu", myFile , timeMe / 1000000.0, xx, file3.size() );
		file3.close();
		Serial.printf( "\n\tBig write KBytes per second %5.2f \n", xx / (timeMe / 1000.0) );
		Serial.printf("\nBytes Used: %llu, Bytes Total:%llu\n", myfs.usedSize(), myfs.totalSize());
	}
}
