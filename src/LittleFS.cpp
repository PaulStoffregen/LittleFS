/* LittleFS for Teensy
 * Copyright (c) 2020, Paul Stoffregen, paul@pjrc.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Arduino.h>
#include <LittleFS.h>

#define SPICONFIG   SPISettings(30000000, MSBFIRST, SPI_MODE0)


PROGMEM static const struct chipinfo {
	uint8_t id[3];
	uint8_t addrbits;	// number of address bits, 24 or 32
	uint16_t progsize;	// page size for programming, in bytes
	uint32_t erasesize;	// sector size for erasing, in bytes
	uint32_t chipsize;	// total number of bytes in the chip
	uint32_t progtime;	// maximum microseconds to wait for page programming
	uint32_t erasetime;	// maximum microseconds to wait for sector erase
} known_chips[] = {
	{{0xEF, 0x40, 0x15}, 24, 256, 4096, 2097152, 3000, 400000}, // Winbond W25Q16JV*IQ/W25Q16FV
	{{0xEF, 0x40, 0x16}, 24, 256, 4096, 4194304, 3000, 400000}, // Winbond W25Q32JV*IQ/W25Q32FV
	{{0xEF, 0x40, 0x17}, 24, 256, 4096, 8388608, 3000, 400000}, // Winbond W25Q64JV*IQ/W25Q64FV
	{{0xEF, 0x40, 0x18}, 24, 256, 4096, 16777216, 3000, 400000}, // Winbond W25Q128JV*IQ/W25Q128FV
	{{0xEF, 0x40, 0x19}, 32, 256, 4096, 33554432, 3000, 400000}, // Winbond W25Q256JV*IQ
	{{0xEF, 0x40, 0x20}, 32, 256, 4096, 67108864, 3500, 400000}, // Winbond W25Q512JV*IQ
	{{0xEF, 0x70, 0x17}, 24, 256, 4096, 8388608, 3000, 400000}, // Winbond W25Q64JV*IM (DTR)
	{{0xEF, 0x70, 0x18}, 24, 256, 4096, 16777216, 3000, 400000}, // Winbond W25Q128JV*IM (DTR)
	{{0xEF, 0x70, 0x19}, 32, 256, 4096, 33554432, 3000, 400000}, // Winbond W25Q256JV*IM (DTR)
	{{0xEF, 0x70, 0x20}, 32, 256, 4096, 67108864, 3500, 400000}, // Winbond W25Q512JV*IM (DTR)
	{{0x1F, 0x84, 0x01}, 24, 256, 4096, 524288, 2500, 300000}, // Adesto/Atmel AT25SF041
	{{0x01, 0x40, 0x14}, 24, 256, 4096, 1048576, 5000, 300000}, // Spansion S25FL208K

};


static const struct chipinfo * chip_lookup(const uint8_t *id)
{
	const unsigned int numchips = sizeof(known_chips) / sizeof(struct chipinfo);
	for (unsigned int i=0; i < numchips; i++) {
		const uint8_t *chip = known_chips[i].id;
		if (id[0] == chip[0] && id[1] == chip[1] && id[2] == chip[2]) {
			return known_chips + i;
		}
	}
	return nullptr;
}

FLASHMEM
bool LittleFS_SPIFlash::begin(uint8_t cspin, SPIClass &spiport)
{
	pin = cspin;
	port = &spiport;

	//Serial.println("flash begin");
	configured = false;
	digitalWrite(pin, HIGH);
	pinMode(pin, OUTPUT);
	port->begin();

	uint8_t buf[4] = {0x9F, 0, 0, 0};
	port->beginTransaction(SPICONFIG);
	digitalWrite(pin, LOW);
	port->transfer(buf, 4);
	digitalWrite(pin, HIGH);
	port->endTransaction();

	//Serial.printf("Flash ID: %02X %02X %02X\n", buf[1], buf[2], buf[3]);
	const struct chipinfo *info = chip_lookup(buf + 1);
	if (!info) return false;
	//Serial.printf("Flash size is %.2f Mbyte\n", (float)info->chipsize / 1048576.0f);

	memset(&lfs, 0, sizeof(lfs));
	memset(&config, 0, sizeof(config));
	config.context = (void *)this;
	config.read = &static_read;
	config.prog = &static_prog;
	config.erase = &static_erase;
	config.sync = &static_sync;
	config.read_size = info->progsize;
	config.prog_size = info->progsize;
	config.block_size = info->erasesize;
	config.block_count = info->chipsize / info->erasesize;
	config.block_cycles = 400;
	config.cache_size = info->progsize;
	config.lookahead_size = info->progsize;
	config.name_max = LFS_NAME_MAX;
	addrbits = info->addrbits;
	progtime = info->progtime;
	erasetime = info->erasetime;
	checkformat = nullptr;
	checkused = nullptr;
	lfschange = true;
	configured = true;

	//Serial.println("attempting to mount existing media");
	if (lfs_mount(&lfs, &config) < 0) {
		//Serial.println("couldn't mount media, attemping to format");
		if (lfs_format(&lfs, &config) < 0) {
			//Serial.println("format failed :(");
			port = nullptr;
			return false;
		}
		//Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			//Serial.println("mount after format failed :(");
			port = nullptr;
			return false;
		}
	}
	mounted = true;
	//Serial.println("success");
	return true;
}

FLASHMEM
bool LittleFS::quickFormat()
{
	if (!configured) return false;
	if (mounted) {
		//Serial.println("unmounting filesystem");
		lfs_unmount(&lfs);
		mounted = false;
		// TODO: What happens if lingering LittleFSFile instances
		// still have lfs_file_t structs allocated which reference
		// this previously mounted filesystem?
	}
	//Serial.println("attempting to format existing media");
	if (lfs_format(&lfs, &config) < 0) {
		//Serial.println("format failed :(");
		return false;
	}
	//Serial.println("attempting to mount freshly formatted media");
	if (lfs_mount(&lfs, &config) < 0) {
		//Serial.println("mount after format failed :(");
		return false;
	}
	mounted = true;
	//Serial.println("success");
	return true;
}

static bool blockIsBlank(struct lfs_config *config, lfs_block_t block, void *readBuf)
{
	if (!readBuf) return false;
	for (lfs_off_t offset=0; offset < config->block_size; offset += config->read_size) {
		memset(readBuf, 0, config->read_size);
		config->read(config, block, offset, readBuf, config->read_size);
		const uint8_t *buf = (uint8_t *)readBuf;
		for (unsigned int i=0; i < config->read_size; i++) {
			if (buf[i] != 0xFF) return false;
		}
	}
	return true; // all bytes read as 0xFF
}

#define DEBUGCF
FLASHMEM
bool LittleFS::checkFormat(bool readCheck) {
	if ( !configured || !readCheck ) {
		lfschange = true;
		if ( checkformat != nullptr) free( checkformat );
		checkformat = nullptr;
	}
	else if ( readCheck ) { //  called by formatUnused()
		uint32_t iiblk = 1+(config.block_count /8);
		if ( checkformat == nullptr) checkformat = (uint8_t *)malloc( iiblk );
		if ( checkformat == nullptr) return (checkformat != nullptr);
		memset(checkformat, 0, iiblk);
		uint8_t jjbit;
		void *buffer = malloc(config.read_size);
#ifdef DEBUGCF
		Serial.printf( "\n\n checkFormat:: Pre-Formatted Blocks Map \n" );
		uint32_t *fake = (uint32_t *)checkformat;
		int ff=0;
#endif
		uint8_t *lfsUsed;
		checkUsed( true ); // Call this to exclude checking of reported lfs used blocks
		if ( nullptr != checkused ) lfsUsed = checkused; // 1 bits are lfs Used
		else lfsUsed = checkformat; // checkUsed() failed: pre-zeroed above will skip lfs exclusion


#ifdef DEBUGCF
		if ( nullptr != checkused ) Serial.printf( "\n checkFormat:: lfsUsed = checkused; \n" );
		else Serial.printf( "\n checkFormat:: lfsUsed = checkformat; \n" );
#endif


		for (unsigned int block=0; block < config.block_count; block++) {
			iiblk = block/8;
			jjbit = 1<<(block%8);
			if ( !(lfsUsed[iiblk] & jjbit ) ) {
				if (blockIsBlank(&config, block, buffer)) {
					checkformat[iiblk] = checkformat[iiblk] | jjbit;
				}
			}
#ifdef DEBUGCF
			if ( (block/8)>3 && (0==(block/8)%4) && 0==block%8 ) Serial.printf( "\t0x%lx", fake[ff++]);
			if ( !((block/8)%40) && 0==block%8 ) Serial.printf( "\n%u", block );
#endif
		}
		free(buffer);
	}
	return checkformat != nullptr;
}

static int cb_usedBlocks( void *inData, lfs_block_t block )
{
	static lfs_block_t maxBlock;
	static uint32_t totBlock;
	if ( nullptr == inData ) { // not null during traverse
		uint32_t totRet = totBlock;
		if ( 0 != block ) {
			maxBlock = block;
			totBlock = 0;
		}
		return totRet; // exit after init, end, or bad call
	}
	totBlock++;
	if ( block > maxBlock ) return block; // this is beyond media blocks
	uint32_t iiblk = block/8;
	uint8_t jjbit = 1<<(block%8);
	uint8_t *myData = (uint8_t *)inData;
	myData[iiblk] = myData[iiblk] | jjbit;
	return 0;
}

FLASHMEM
uint32_t LittleFS::formatUnused(uint32_t blockCnt) {
	// UNSAFE ???? if ( lfschange == true ) 
	checkFormat( true );
	if ( nullptr == checkformat || nullptr == checkused ) return 0;
	uint32_t ii=0, jj=0, tt;
	uint8_t bb;
	uint8_t *lfsUsed = checkused; // 1 bits are lfs Used
	uint8_t *preFmt = checkformat; // 1 bits are pre Formatted block
	while ( ii<config.block_count && jj<blockCnt ) {
		if ( 0==(ii%8) ) bb=1;
		tt = ii/8;
		if ( !(lfsUsed[tt] & bb ) && !(preFmt[tt] & bb) ) { // block not already formatted or used
			(*config.erase)(&config, ii);
#ifdef DEBUGCF
		Serial.printf( "\n UnUsed Block %lu formatted. # %lu of %lu", ii, jj+1, blockCnt );
		Serial.printf( "\t  lfsU=0x%x preF=0x%x\t(after0x%x)", lfsUsed[tt] , preFmt[tt], preFmt[tt]|bb );
#endif
			preFmt[tt] |= bb; // mark as pre-formatted
			jj++;
		}
		ii++;
		bb = bb<<1;
	}
#ifdef DEBUGCF
		Serial.printf( "\n formatUnused Block formatted :: # %lu of %lu\n", jj, config.block_count );
#endif
	return jj;
}

FLASHMEM
bool LittleFS::checkUsed(bool readCheck) {
	if ( !configured || !readCheck ) {
		if ( checkused != nullptr) {
			free( checkused );
		}
		checkused = nullptr;
		lfschange = true;
	}
	else if ( readCheck ) {

#ifdef DEBUGCF
	uint32_t tmpTotBlock = cb_usedBlocks( nullptr, 0 ); // get total blocks
#endif
		int err=0;
		cb_usedBlocks( nullptr, config.block_count ); // init and pass block_count
		uint32_t iiblk = 1+(config.block_count /8);
		if ( checkused == nullptr) checkused = (uint8_t *)malloc( iiblk );
		if ( checkused == nullptr) return (checkused != nullptr);
		memset(checkused, 0, iiblk);
		err = lfs_fs_traverse(&lfs, cb_usedBlocks, checkused);
		uint32_t totBlock = cb_usedBlocks( nullptr, 0 ); // get total blocks
#ifdef DEBUGCF
		if ( lfschange == false ) {
			if ( 0 != tmpTotBlock && totBlock != tmpTotBlock ) Serial.printf( "\n lfschange missed used %lu != %lu",tmpTotBlock, totBlock  );
			else Serial.printf( "\n No lfschange could have skipped ???" );
		}
		Serial.printf( "\n\n checkUsed:: lfs Used Blocks Map: #used is %lu : lfs traverse return %i (neg on err)\n", totBlock, err );
		uint32_t *fakeU = (uint32_t *)checkused;
		uint32_t ff=0;
		for (unsigned int block=0; block < config.block_count; block++) {
			if ( (block/8)>3 && (0==(block/8)%4) && 0==block%8 ) Serial.printf( "\t0x%lx", fakeU[ff++]);
			if ( !((block/8)%40) && 0==block%8 ) Serial.printf( "\n%u", block );
		}
		Serial.printf( "\n checkUsed:: lfs Used Blocks Map: #used is %lu\n", totBlock );
		if ( checkformat != nullptr && checkused != nullptr ) {
			uint32_t *fakeC = (uint32_t *)checkformat;
			Serial.printf( "\n lfs Used&Format overlap errs?: "  );
			uint32_t tt;
			for ( ff=0; ff < config.block_count/32; ff++) {
				tt = fakeU[ff]&fakeC[ff];
				//if ( 0!=(tt&fakeC[ff]) ) Serial.printf( "[%lu]:0x%lx!=0x%lx ^=%lX\t", ff*32, fakeU[ff], fakeC[ff], tt);
				if ( 0!=tt ) Serial.printf( "[%lu]:0x%lx&0x%lx !=0?\t", ff*32, fakeU[ff], fakeC[ff]);
			}
			Serial.printf( "{eol}\n"  );
		}
#endif
		if ( err < 0 )
		{
			if ( checkused != nullptr)
				free( checkused );
			checkused = nullptr;
		}
		else {
			lfschange = false; // TODO - this must be accurately set to allow avoiding refresh of bitmaps
		}
	}
	return checkused != nullptr;
}

FLASHMEM
bool LittleFS::lowLevelFormat(char progressChar)
{
	if (!configured) return false;
	checkFormat( false );
	if (mounted) {
		lfs_unmount(&lfs);
		mounted = false;
	}
	int ii=config.block_count/120;
	void *buffer = malloc(config.read_size);
	for (unsigned int block=0; block < config.block_count; block++) {
		if (progressChar && (0 == block%ii) ) Serial.write(progressChar);
		if (!blockIsBlank(&config, block, buffer)) {
			(*config.erase)(&config, block);
		}
	}
	free(buffer);
	if (progressChar) Serial.println();
	return quickFormat();
}

static void make_command_and_address(uint8_t *buf, uint8_t cmd, uint32_t addr, uint8_t addrbits)
{
	buf[0] = cmd;
	if (addrbits == 24) {
		buf[1] = addr >> 16;
		buf[2] = addr >> 8;
		buf[3] = addr;
	} else {
		buf[1] = addr >> 24;
		buf[2] = addr >> 16;
		buf[3] = addr >> 8;
		buf[4] = addr;
	}
}
static void printtbuf(const void *buf, unsigned int len) __attribute__((unused));
static void printtbuf(const void *buf, unsigned int len)
{
	//const uint8_t *p = (const uint8_t *)buf;
	//Serial.print("    ");
	//while (len--) Serial.printf("%02X ", *p++);
	//Serial.println();
}

int LittleFS_SPIFlash::read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size)
{
	if (!port) return LFS_ERR_IO;
	const uint32_t addr = block * config.block_size + offset;
	const uint8_t cmd = (addrbits == 24) ? 0x03 : 0x13; // standard read command
	uint8_t cmdaddr[5];
	//Serial.printf("  addrbits=%d\n", addrbits);
	make_command_and_address(cmdaddr, cmd, addr, addrbits);
	//printtbuf(cmdaddr, 1 + (addrbits >> 3));
	memset(buf, 0, size);
	port->beginTransaction(SPICONFIG);
	digitalWrite(pin, LOW);
	port->transfer(cmdaddr, 1 + (addrbits >> 3));
	port->transfer(buf, size);
	digitalWrite(pin, HIGH);
	port->endTransaction();
	//printtbuf(buf, 20);
	return 0;
}

int LittleFS_SPIFlash::prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size)
{
	if (!port) return LFS_ERR_IO;
	const uint32_t addr = block * config.block_size + offset;
	const uint8_t cmd = (addrbits == 24) ? 0x02 : 0x12; // page program
	uint8_t cmdaddr[5];
	make_command_and_address(cmdaddr, cmd, addr, addrbits);
	//printtbuf(cmdaddr, 1 + (addrbits >> 3));
	port->beginTransaction(SPICONFIG);
	digitalWrite(pin, LOW);
	port->transfer(0x06); // 0x06 = write enable
	digitalWrite(pin, HIGH);
	delayNanoseconds(250);
	digitalWrite(pin, LOW);
	port->transfer(cmdaddr, 1 + (addrbits >> 3));
	port->transfer(buf, nullptr, size);
	digitalWrite(pin, HIGH);
	port->endTransaction();
	//printtbuf(buf, 20);
	return wait(progtime);
}

bool knownFormatted( struct lfs_config *config, lfs_block_t block, uint8_t *checkformat, bool unset=true );
bool knownFormatted( struct lfs_config *config, lfs_block_t block, uint8_t *checkformat, bool unset ) {
	if ( checkformat != nullptr) {
		uint32_t iiblk = block/8;
		uint8_t jjbit = 1<<(block%8);
		if ( checkformat[iiblk] & jjbit ) { // was set as formatted, unset as erase precedes usage
			if ( unset )
				checkformat[iiblk] &= ~jjbit; // LFS call to format puts it in used state
			return true;
		}
		else if ( !unset ) { // TODO if called for PJRC format, not LFS, then it will be formatted after this
// TODO			checkformat[iiblk] &= jjbit; // other Format leaves it formatted
		}
	}

	void *readBuf = malloc(config->read_size);
//static bool blockIsBlank(struct lfs_config *config, lfs_block_t block, void *readBuf)
{
	if (!readBuf) return false;
	for (lfs_off_t offset=0; offset < config->block_size; offset += config->read_size) {
		memset(readBuf, 0, config->read_size);
		config->read(config, block, offset, readBuf, config->read_size);
		const uint8_t *buf = (uint8_t *)readBuf;
		for (unsigned int i=0; i < config->read_size; i++) {
			if (buf[i] != 0xFF) return false;
		}
	}
	free(readBuf);
	return true; // all bytes read as 0xFF
}
	free(readBuf);
	return false;
}


int LittleFS_SPIFlash::erase(lfs_block_t block)
{
	if (!port) return LFS_ERR_IO;
	if ( knownFormatted( &config, block, checkformat ) ) return 0;
	const uint32_t addr = block * config.block_size;
	const uint8_t cmd = (addrbits == 24) ? 0x20 : 0x21; // erase sector
	uint8_t cmdaddr[5];
	make_command_and_address(cmdaddr, cmd, addr, addrbits);
	//printtbuf(cmdaddr, 1 + (addrbits >> 3));
	port->beginTransaction(SPICONFIG);
	digitalWrite(pin, LOW);
	port->transfer(0x06); // 0x06 = write enable
	digitalWrite(pin, HIGH);
	delayNanoseconds(250);
	digitalWrite(pin, LOW);
	port->transfer(cmdaddr, 1 + (addrbits >> 3));
	digitalWrite(pin, HIGH);
	port->endTransaction();
	return wait(erasetime);
}

int LittleFS_SPIFlash::wait(uint32_t microseconds)
{
	elapsedMicros usec = 0;
	while (1) {
		port->beginTransaction(SPICONFIG);
		digitalWrite(pin, LOW);
		uint16_t status = port->transfer16(0x0500); // 0x05 = get status
		digitalWrite(pin, HIGH);
		port->endTransaction();
		if (!(status & 1)) break;
		if (usec > microseconds) return LFS_ERR_IO; // timeout
		yield();
	}
	//Serial.printf("  waited %u us\n", (unsigned int)usec);
	return 0; // success
}






#if defined(__IMXRT1062__)

#define LUT0(opcode, pads, operand) (FLEXSPI_LUT_INSTRUCTION((opcode), (pads), (operand)))
#define LUT1(opcode, pads, operand) (FLEXSPI_LUT_INSTRUCTION((opcode), (pads), (operand)) << 16)
#define CMD_SDR         FLEXSPI_LUT_OPCODE_CMD_SDR
#define ADDR_SDR        FLEXSPI_LUT_OPCODE_RADDR_SDR
#define READ_SDR        FLEXSPI_LUT_OPCODE_READ_SDR
#define WRITE_SDR       FLEXSPI_LUT_OPCODE_WRITE_SDR
#define DUMMY_SDR       FLEXSPI_LUT_OPCODE_DUMMY_SDR
#define PINS1           FLEXSPI_LUT_NUM_PADS_1
#define PINS4           FLEXSPI_LUT_NUM_PADS_4

static void flexspi2_ip_command(uint32_t index, uint32_t addr)
{
	uint32_t n;
	FLEXSPI2_IPCR0 = addr;
	FLEXSPI2_IPCR1 = FLEXSPI_IPCR1_ISEQID(index);
	FLEXSPI2_IPCMD = FLEXSPI_IPCMD_TRG;
	while (!((n = FLEXSPI2_INTR) & FLEXSPI_INTR_IPCMDDONE)); // wait
	if (n & FLEXSPI_INTR_IPCMDERR) {
		FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDERR;
		//Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}

static void flexspi2_ip_read(uint32_t index, uint32_t addr, void *data, uint32_t length)
{
	uint8_t *p = (uint8_t *)data;

	FLEXSPI2_INTR = FLEXSPI_INTR_IPRXWA;
	// Clear RX FIFO and set watermark to 16 bytes
	FLEXSPI2_IPRXFCR = FLEXSPI_IPRXFCR_CLRIPRXF | FLEXSPI_IPRXFCR_RXWMRK(1);
	FLEXSPI2_IPCR0 = addr;
	FLEXSPI2_IPCR1 = FLEXSPI_IPCR1_ISEQID(index) | FLEXSPI_IPCR1_IDATSZ(length);
	FLEXSPI2_IPCMD = FLEXSPI_IPCMD_TRG;
// page 1649 : Reading Data from IP RX FIFO
// page 1706 : Interrupt Register (INTR)
// page 1723 : IP RX FIFO Control Register (IPRXFCR)
// page 1732 : IP RX FIFO Status Register (IPRXFSTS)

	while (1) {
		if (length >= 16) {
			if (FLEXSPI2_INTR & FLEXSPI_INTR_IPRXWA) {
				volatile uint32_t *fifo = &FLEXSPI2_RFDR0;
				uint32_t a = *fifo++;
				uint32_t b = *fifo++;
				uint32_t c = *fifo++;
				uint32_t d = *fifo++;
				*(uint32_t *)(p+0) = a;
				*(uint32_t *)(p+4) = b;
				*(uint32_t *)(p+8) = c;
				*(uint32_t *)(p+12) = d;
				p += 16;
				length -= 16;
				FLEXSPI2_INTR = FLEXSPI_INTR_IPRXWA;
			}
		} else if (length > 0) {
			if ((FLEXSPI2_IPRXFSTS & 0xFF) >= ((length + 7) >> 3)) {
				volatile uint32_t *fifo = &FLEXSPI2_RFDR0;
				while (length >= 4) {
					*(uint32_t *)(p) = *fifo++;
					p += 4;
					length -= 4;
				}
				uint32_t a = *fifo;
				if (length >= 1) {
					*p++ = a & 0xFF;
					a = a >> 8;
				}
				if (length >= 2) {
					*p++ = a & 0xFF;
					a = a >> 8;
				}
				if (length >= 3) {
					*p++ = a & 0xFF;
					a = a >> 8;
				}
				length = 0;
			}
		} else {
			if (FLEXSPI2_INTR & FLEXSPI_INTR_IPCMDDONE) break;
		}
		// TODO: timeout...
	}
	if (FLEXSPI2_INTR & FLEXSPI_INTR_IPCMDERR) {
		FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDERR;
		//Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\r\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}

static void flexspi2_ip_write(uint32_t index, uint32_t addr, const void *data, uint32_t length)
{
	const uint8_t *src;
	uint32_t n, wrlen;

	FLEXSPI2_IPCR0 = addr;
	FLEXSPI2_IPCR1 = FLEXSPI_IPCR1_ISEQID(index) | FLEXSPI_IPCR1_IDATSZ(length);
	src = (const uint8_t *)data;
	FLEXSPI2_IPCMD = FLEXSPI_IPCMD_TRG;
	while (!((n = FLEXSPI2_INTR) & FLEXSPI_INTR_IPCMDDONE)) {
		if (n & FLEXSPI_INTR_IPTXWE) {
			wrlen = length;
			if (wrlen > 8) wrlen = 8;
			if (wrlen > 0) {
				//Serial.print("%");
				memcpy((void *)&FLEXSPI2_TFDR0, src, wrlen);
				src += wrlen;
				length -= wrlen;
				FLEXSPI2_INTR = FLEXSPI_INTR_IPTXWE;
			}
		}
	}
	if (n & FLEXSPI_INTR_IPCMDERR) {
		FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDERR;
		//Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\r\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}






FLASHMEM
bool LittleFS_QSPIFlash::begin()
{
	//Serial.println("QSPI flash begin");

	configured = false;

	uint8_t buf[4] = {0, 0, 0, 0};

	FLEXSPI2_LUTKEY = FLEXSPI_LUTKEY_VALUE;
	FLEXSPI2_LUTCR = FLEXSPI_LUTCR_UNLOCK;
	// cmd index 8 = read ID bytes
	FLEXSPI2_LUT32 = LUT0(CMD_SDR, PINS1, 0x9F) | LUT1(READ_SDR, PINS1, 1);
	FLEXSPI2_LUT33 = 0;

	flexspi2_ip_read(8, 0x00800000, buf, 3);


	//Serial.printf("Flash ID: %02X %02X %02X\n", buf[0], buf[1], buf[2]);
	const struct chipinfo *info = chip_lookup(buf);
	if (!info) return false;
	//Serial.printf("Flash size is %.2f Mbyte\n", (float)info->chipsize / 1048576.0f);

	memset(&lfs, 0, sizeof(lfs));
	memset(&config, 0, sizeof(config));
	config.context = (void *)this;
	config.read = &static_read;
	config.prog = &static_prog;
	config.erase = &static_erase;
	config.sync = &static_sync;
	config.read_size = info->progsize;
	config.prog_size = info->progsize;
	config.block_size = info->erasesize;
	config.block_count = info->chipsize / info->erasesize;
	config.block_cycles = 400;
	config.cache_size = info->progsize;
	config.lookahead_size = info->progsize;
	config.name_max = LFS_NAME_MAX;
	addrbits = info->addrbits;
	progtime = info->progtime;
	erasetime = info->erasetime;
	checkformat = nullptr;
	lfschange = true;
	configured = true;

	// configure FlexSPI2 for chip's size
	FLEXSPI2_FLSHA2CR0 = info->chipsize / 1024;

	FLEXSPI2_LUTKEY = FLEXSPI_LUTKEY_VALUE;
	FLEXSPI2_LUTCR = FLEXSPI_LUTCR_UNLOCK;

	// TODO: is this Winbond specific?  Diable for non-Winbond chips...
	FLEXSPI2_LUT40 = LUT0(CMD_SDR, PINS1, 0x50);
	flexspi2_ip_command(10, 0x00800000); // volatile write status enable
	FLEXSPI2_LUT40 = LUT0(CMD_SDR, PINS1, 0x31) | LUT1(CMD_SDR, PINS1, 0x02);
	FLEXSPI2_LUT41 = 0;
	flexspi2_ip_command(10, 0x00800000); // enable quad mode

	if (addrbits == 24) {
		// cmd index 9 = read QSPI (1-1-4)
		FLEXSPI2_LUT36 = LUT0(CMD_SDR, PINS1, 0x6B) | LUT1(ADDR_SDR, PINS1, 24);
		FLEXSPI2_LUT37 = LUT0(DUMMY_SDR, PINS4, 8) |  LUT1(READ_SDR, PINS4, 1);
		FLEXSPI2_LUT38 = 0;
		// cmd index 11 = program QSPI (1-1-4)
		FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0x32) | LUT1(ADDR_SDR, PINS1, 24);
		FLEXSPI2_LUT45 = LUT0(WRITE_SDR, PINS4, 1);
		// cmd index 12 = sector erase
		FLEXSPI2_LUT48 = LUT0(CMD_SDR, PINS1, 0x20) | LUT1(ADDR_SDR, PINS1, 24);
		FLEXSPI2_LUT49 = 0;
	} else {
		// cmd index 9 = read QSPI (1-1-4)
		FLEXSPI2_LUT36 = LUT0(CMD_SDR, PINS1, 0x6C) | LUT1(ADDR_SDR, PINS1, 32);
		FLEXSPI2_LUT37 = LUT0(DUMMY_SDR, PINS4, 8) |  LUT1(READ_SDR, PINS4, 1);
		FLEXSPI2_LUT38 = 0;
		// cmd index 11 = program QSPI (1-1-4)
		FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0x34) | LUT1(ADDR_SDR, PINS1, 32);
		FLEXSPI2_LUT45 = LUT0(WRITE_SDR, PINS4, 1);
		// cmd index 12 = sector erase
		FLEXSPI2_LUT48 = LUT0(CMD_SDR, PINS1, 0x21) | LUT1(ADDR_SDR, PINS1, 32);
		FLEXSPI2_LUT49 = 0;
		// cmd index 9 = read SPI (1-1-1)
		//FLEXSPI2_LUT36 = LUT0(CMD_SDR, PINS1, 0x13) | LUT1(ADDR_SDR, PINS1, 32);
		//FLEXSPI2_LUT37 = LUT0(READ_SDR, PINS1, 1);
		// cmd index 11 = program SPI (1-1-1)
		//FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0x12) | LUT1(ADDR_SDR, PINS1, 32);
		//FLEXSPI2_LUT45 = LUT0(WRITE_SDR, PINS1, 1);
	}
	// cmd index 10 = write enable
	FLEXSPI2_LUT40 = LUT0(CMD_SDR, PINS1, 0x06);
	// cmd index 13 = get status
	FLEXSPI2_LUT52 = LUT0(CMD_SDR, PINS1, 0x05) | LUT1(READ_SDR, PINS1, 1);
	FLEXSPI2_LUT53 = 0;


	//Serial.println("attempting to mount existing media");
	if (lfs_mount(&lfs, &config) < 0) {
		//Serial.println("couldn't mount media, attemping to format");
		if (lfs_format(&lfs, &config) < 0) {
			//Serial.println("format failed :(");
			return false;
		}
		//Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			//Serial.println("mount after format failed :(");
			return false;
		}
	}
	mounted = true;
	//Serial.println("success");
	return true;
}

int LittleFS_QSPIFlash::read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size)
{
	const uint32_t addr = block * config.block_size + offset;
	flexspi2_ip_read(9, 0x00800000 + addr, buf, size);
	// TODO: detect errors, return LFS_ERR_IO
	//printtbuf(buf, 20);
	return 0;
}

int LittleFS_QSPIFlash::prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size)
{
	flexspi2_ip_command(10, 0x00800000);
	const uint32_t addr = block * config.block_size + offset;
	//printtbuf(buf, 20);
	flexspi2_ip_write(11, 0x00800000 + addr, buf, size);
	// TODO: detect errors, return LFS_ERR_IO
	return wait(progtime);
}

int LittleFS_QSPIFlash::erase(lfs_block_t block)
{
	if ( knownFormatted( &config, block, checkformat ) ) return 0;
	flexspi2_ip_command(10, 0x00800000);
	const uint32_t addr = block * config.block_size;
	flexspi2_ip_command(12, 0x00800000 + addr);
	// TODO: detect errors, return LFS_ERR_IO
	return wait(erasetime);
}

int LittleFS_QSPIFlash::wait(uint32_t microseconds)
{
	elapsedMicros usec = 0;
	while (1) {
		uint8_t status;
		flexspi2_ip_read(13, 0x00800000, &status, 1);
		if (!(status & 1)) break;
		if (usec > microseconds) return LFS_ERR_IO; // timeout
		yield();
	}
	//Serial.printf("  waited %u us\n", (unsigned int)usec);
	return 0; // success
}

#endif // __IMXRT1062__







#if defined(__IMXRT1062__)

#if defined(ARDUINO_TEENSY40)
#define FLASH_SIZE  0x1F0000
#elif defined(ARDUINO_TEENSY41)
#define FLASH_SIZE  0x7C0000
#elif defined(ARDUINO_TEENSY_MICROMOD)
#define FLASH_SIZE  0xFC0000
#endif
extern unsigned long _flashimagelen;
uint32_t LittleFS_Program::baseaddr = 0;

FLASHMEM
bool LittleFS_Program::begin(uint32_t size)
{
	//Serial.println("Program flash begin");
	configured = false;
	baseaddr = 0;
	size = size & 0xFFFF0000;
	if (size == 0) return false;
	const uint32_t program_size = (uint32_t)&_flashimagelen + 4096; // extra 4K for CSF
	if (program_size >= FLASH_SIZE) return false;
	const uint32_t available_space = FLASH_SIZE - program_size;
	//Serial.printf("available_space = %u\n", available_space);
	if (size > available_space) return false;

	baseaddr = 0x60000000 + FLASH_SIZE - size;
	//Serial.printf("baseaddr = %x\n", baseaddr);

	memset(&lfs, 0, sizeof(lfs));
	memset(&config, 0, sizeof(config));
	config.context = (void *)baseaddr;
	config.read = &static_read;
	config.prog = &static_prog;
	config.erase = &static_erase;
	config.sync = &static_sync;
	config.read_size = 128;
	config.prog_size = 128;
	config.block_size = 4096;
	config.block_count = size >> 12;
	config.block_cycles = 800;
	config.cache_size = 128;
	config.lookahead_size = 128;
	config.name_max = LFS_NAME_MAX;
	checkformat = nullptr;
	configured = true;

	//Serial.println("attempting to mount existing media");
	if (lfs_mount(&lfs, &config) < 0) {
		//Serial.println("couldn't mount media, attemping to format");
		if (lfs_format(&lfs, &config) < 0) {
			//Serial.println("format failed :(");
			return false;
		}
		//Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			//Serial.println("mount after format failed :(");
			return false;
		}
	}
	mounted = true;
	return true;
}

int LittleFS_Program::static_read(const struct lfs_config *c, lfs_block_t block,
	lfs_off_t offset, void *buffer, lfs_size_t size)
{
	//Serial.printf("   prog rd: block=%d, offset=%d, size=%d\n", block, offset, size);
	const uint8_t *p = (uint8_t *)(baseaddr + block * 4096 + offset);
	memcpy(buffer, p, size);
	return 0;
}

// from eeprom.c
extern "C" void eepromemu_flash_write(void *addr, const void *data, uint32_t len);
extern "C" void eepromemu_flash_erase_sector(void *addr);

int LittleFS_Program::static_prog(const struct lfs_config *c, lfs_block_t block,
	lfs_off_t offset, const void *buffer, lfs_size_t size)
{
	//Serial.printf("   prog wr: block=%d, offset=%d, size=%d\n", block, offset, size);
	uint8_t *p = (uint8_t *)(baseaddr + block * 4096 + offset);
	eepromemu_flash_write(p, buffer, size);
	return 0;
}

int LittleFS_Program::static_erase(const struct lfs_config *c, lfs_block_t block)
{
	//Serial.printf("   prog er: block=%d\n", block);
	//if ( knownFormatted( block, checkformat ) ) return 0;
	uint8_t *p = (uint8_t *)(baseaddr + block * 4096);
	eepromemu_flash_erase_sector(p);
	return 0;
}


#endif // __IMXRT1062__





