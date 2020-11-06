#include <Arduino.h>
#include <LittleFS.h>

#define SPICONFIG   SPISettings(30000000, MSBFIRST, SPI_MODE0)


PROGMEM static const struct chipinfo {
	uint8_t id[3];
	uint8_t addrbits;	// number of address bits, 24 or 32
	uint16_t progsize;	// page size for programming, in bytes
	uint32_t erasesize;	// sector size for erasing, in bytes
	uint32_t chipsize;	// total number of bytes in the chip
} known_chips[] = {
	{{0xEF, 0x40, 0x17}, 24, 256, 4096, 8388608}, // Winbond W25Q64JVSSIQ
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

	Serial.println("attemping to mount existing media");
	if (lfs_mount(&lfs, &config) < 0) {
		Serial.println("couldn't mount media, attemping to format");
		if (lfs_format(&lfs, &config) < 0) {
			Serial.println("format failed :(");
			port = nullptr;
			return false;
		}
		Serial.println("attemping to mount freshly formatted media");
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
	delayNanoseconds(250);
	digitalWrite(pin, LOW);
	port->transfer(cmdaddr, 1 + (addrbits >> 3));
	port->transfer(buf, nullptr, size);
	digitalWrite(pin, HIGH);
	port->endTransaction();
	//printtbuf(buf, 20);
	return wait(3000);
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
	delayNanoseconds(250);
	digitalWrite(pin, LOW);
	port->transfer(cmdaddr, 1 + (addrbits >> 3));
	digitalWrite(pin, HIGH);
	port->endTransaction();
	return wait(400000);
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
	Serial.printf("  waited %u us\n", (unsigned int)usec);
	return 0; // success
}



