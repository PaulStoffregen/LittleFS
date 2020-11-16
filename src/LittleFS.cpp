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
	{{0xEF, 0x70, 0x17}, 24, 256, 4096, 8388608, 3000, 400000}, // Winbond W25Q64JV*IM (DTR)
	{{0xEF, 0x70, 0x18}, 24, 256, 4096, 16777216, 3000, 400000}, // Winbond W25Q128JV*IM (DTR)
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

	Serial.println("flash begin");

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

	Serial.printf("Flash ID: %02X %02X %02X\n", buf[1], buf[2], buf[3]);
	const struct chipinfo *info = chip_lookup(buf + 1);
	if (!info) return false;
	Serial.printf("Flash size is %.2f Mbyte\n", (float)info->chipsize / 1048576.0f);

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

	Serial.println("attempting to mount existing media");
	if (lfs_mount(&lfs, &config) < 0) {
		Serial.println("couldn't mount media, attemping to format");
		if (lfs_format(&lfs, &config) < 0) {
			Serial.println("format failed :(");
			port = nullptr;
			return false;
		}
		Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			Serial.println("mount after format failed :(");
			port = nullptr;
			return false;
		}
	}
	configured = true;
	Serial.println("success");
	return true;
}

FLASHMEM
bool LittleFS_SPIFlash::format()
{

	Serial.println("attempting to format existing media");
	if (lfs_format(&lfs, &config) < 0) {
		Serial.println("format failed :(");
		return false;
	}
		Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			Serial.println("mount after format failed :(");
			return false;
		}
	Serial.println("success");
	return true;
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

void printtbuf(const void *buf, unsigned int len)
{
	const uint8_t *p = (const uint8_t *)buf;
	Serial.print("    ");
	while (len--) Serial.printf("%02X ", *p++);
	Serial.println();
}

int LittleFS_SPIFlash::read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size)
{
	if (!port) return LFS_ERR_IO;
	const uint32_t addr = block * config.block_size + offset;
	uint8_t cmdaddr[5];
	//Serial.printf("  addrbits=%d\n", addrbits);
	make_command_and_address(cmdaddr, 0x03, addr, addrbits); // 0x03 = read
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
	uint8_t cmdaddr[5];
	make_command_and_address(cmdaddr, 0x02, addr, addrbits); // 0x02 = page program
	//printtbuf(cmdaddr, 1 + (addrbits >> 3));
	port->beginTransaction(SPICONFIG);
	digitalWrite(pin, LOW);
	port->transfer(0x06); // 0x06 = write enable
	digitalWrite(pin, HIGH);
#if defined(__IMXRT1062__)
	delayNanoseconds(250);
#else
	asm("nop\n nop\n nop\n nop\n");
#endif
	digitalWrite(pin, LOW);
	port->transfer(cmdaddr, 1 + (addrbits >> 3));
	port->transfer(buf, nullptr, size);
	digitalWrite(pin, HIGH);
	port->endTransaction();
	//printtbuf(buf, 20);
	return wait(progtime);
}

int LittleFS_SPIFlash::erase(lfs_block_t block)
{
	if (!port) return LFS_ERR_IO;
	const uint32_t addr = block * config.block_size;
	uint8_t cmdaddr[5];
	make_command_and_address(cmdaddr, 0x20, addr, addrbits); // 0x20 = erase sector
	//printtbuf(cmdaddr, 1 + (addrbits >> 3));
	port->beginTransaction(SPICONFIG);
	digitalWrite(pin, LOW);
	port->transfer(0x06); // 0x06 = write enable
	digitalWrite(pin, HIGH);
#if defined(__IMXRT1062__)
	delayNanoseconds(250);
#else
	asm("nop\n nop\n nop\n nop\n");
#endif
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
		Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\n", FLEXSPI2_IPRXFSTS);
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
		Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\r\n", FLEXSPI2_IPRXFSTS);
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
		Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\r\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}






FLASHMEM
bool LittleFS_QSPIFlash::begin()
{
	Serial.println("QSPI flash begin");

	configured = false;

	uint8_t buf[4] = {0, 0, 0, 0};

	FLEXSPI2_LUTKEY = FLEXSPI_LUTKEY_VALUE;
	FLEXSPI2_LUTCR = FLEXSPI_LUTCR_UNLOCK;
	// cmd index 8 = read ID bytes
	FLEXSPI2_LUT32 = LUT0(CMD_SDR, PINS1, 0x9F) | LUT1(READ_SDR, PINS1, 1);
	FLEXSPI2_LUT33 = 0;

	flexspi2_ip_read(8, 0x00800000, buf, 3);


	Serial.printf("Flash ID: %02X %02X %02X\n", buf[0], buf[1], buf[2]);
	const struct chipinfo *info = chip_lookup(buf);
	if (!info) return false;
	Serial.printf("Flash size is %.2f Mbyte\n", (float)info->chipsize / 1048576.0f);

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

	// configure FlexSPI2 for chip's size
	FLEXSPI2_FLSHA2CR0 = info->chipsize / 1024;

	FLEXSPI2_LUTKEY = FLEXSPI_LUTKEY_VALUE;
	FLEXSPI2_LUTCR = FLEXSPI_LUTCR_UNLOCK;
	// cmd index 9 = read QSPI
	FLEXSPI2_LUT36 = LUT0(CMD_SDR, PINS1, 0x6B) | LUT1(ADDR_SDR, PINS1, 24);
	FLEXSPI2_LUT37 = LUT0(DUMMY_SDR, PINS4, 8) |  LUT1(READ_SDR, PINS4, 1);
	FLEXSPI2_LUT38 = 0;
	// cmd index 10 = write enable
	FLEXSPI2_LUT40 = LUT0(CMD_SDR, PINS1, 0x06);
	// cmd index 11 = program QSPI
	FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0x32) | LUT1(ADDR_SDR, PINS1, 24);
	FLEXSPI2_LUT45 = LUT0(WRITE_SDR, PINS4, 1);
	// cmd index 12 = sector erase
	FLEXSPI2_LUT48 = LUT0(CMD_SDR, PINS1, 0x20) | LUT1(ADDR_SDR, PINS1, 24);
	FLEXSPI2_LUT49 = 0;
	// cmd index 13 = get status
	FLEXSPI2_LUT52 = LUT0(CMD_SDR, PINS1, 0x05) | LUT1(READ_SDR, PINS1, 1);
	FLEXSPI2_LUT53 = 0;


	Serial.println("attempting to mount existing media");
	if (lfs_mount(&lfs, &config) < 0) {
		Serial.println("couldn't mount media, attemping to format");
		if (lfs_format(&lfs, &config) < 0) {
			Serial.println("format failed :(");
			return false;
		}
		Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			Serial.println("mount after format failed :(");
			return false;
		}
	}
	configured = true;
	Serial.println("success");
	return true;
}

FLASHMEM
bool LittleFS_QSPIFlash::format()
{

	Serial.println("attempting to format existing media");
	if (lfs_format(&lfs, &config) < 0) {
		Serial.println("format failed :(");
		return false;
	}
		Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			Serial.println("mount after format failed :(");
			return false;
		}
	Serial.println("success");
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
	Serial.printf("  waited %u us\n", (unsigned int)usec);
	return 0; // success
}

#endif // __IMXRT1062__














