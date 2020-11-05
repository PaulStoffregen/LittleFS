#include <Arduino.h>
#include <FS.h>
#include "littlefs/lfs.h"


class LittleFSFile : public File
{
public:
	LittleFSFile(lfs_t *lfsin, const lfs_file_t *filein) {
		lfs = lfsin;
		memcpy(&file, filein, sizeof(lfs_file_t));
		is_file = true;
		is_dir = false;
		//Serial.printf("  LittleFSFile ctor, this=%x\n", (int)this);
	}
	LittleFSFile(lfs_t *lfsin, const lfs_dir_t *dirin) {
		lfs = lfsin;
		memcpy(&dir, dirin, sizeof(lfs_dir_t));
		is_dir = true;
		is_file = false;
	}
#ifdef FILE_WHOAMI
	virtual void whoami() {
		Serial.printf("  LittleFSFile this=%x, refcount=%u\n",
			(int)this, getRefcount());
	}
#endif
	virtual size_t write(const void *buf, size_t size) {
		//Serial.println("write");
		if (!is_file) return 0;
		//Serial.println(" is regular file");
		return lfs_file_write(lfs, &file, buf, size);
	}
	virtual int peek() {
		return -1; // TODO...
	}
	virtual int available() {
		if (!is_file) return 0;
		lfs_soff_t pos = lfs_file_tell(lfs, &file);
		if (pos < 0) return 0;
		lfs_soff_t size = lfs_file_size(lfs, &file);
		if (size < 0) return 0;
		return size - pos;
	}
	virtual void flush() {
		if (is_file) lfs_file_sync(lfs, &file);
	}
	virtual size_t read(void *buf, size_t nbyte) {
		if (is_file) {
			lfs_ssize_t r = lfs_file_read(lfs, &file, buf, nbyte);
			if (r < 0) r = 0;
			return r;
		}
		return 0;
	}
	virtual bool seek(uint32_t pos, int mode = SeekSet) {
		if (is_file) return false;
		int whence;
		if (mode == SeekSet) whence = LFS_SEEK_SET;
		else if (mode == SeekCur) whence = LFS_SEEK_CUR;
		else if (mode == SeekEnd) whence = LFS_SEEK_END;
		else return false;
		if (lfs_file_seek(lfs, &file, pos, whence) >= 0) return true;
		return false;
	}
	virtual uint32_t position() {
		if (!is_file) return 0;
		lfs_soff_t pos = lfs_file_tell(lfs, &file);
		if (pos < 0) pos = 0;
		return pos;
	}
	virtual uint32_t size() {
		if (!is_file) return 0;
		lfs_soff_t size = lfs_file_size(lfs, &file);
		if (size < 0) size = 0;
		return size;
	}
	virtual void close() {
		if (is_file) {
			Serial.println("  close regular file");
			lfs_file_close(lfs, &file); // we get stuck here, but why?
			is_file = false;
		}
		if (is_dir) {
			Serial.println("  close directory");
			lfs_dir_close(lfs, &dir);
			is_dir = false;
		}
		Serial.println("  end of close");
	}
	virtual operator bool() {
		return is_file || is_dir;
	}

	using Print::write;
private:
	lfs_t *lfs;
	bool is_file;
	bool is_dir;
	lfs_file_t file;
	lfs_dir_t dir;
	//struct lfs_info info;
	//char path[256];
};




class LittleFS : public FS
{
public:
	LittleFS() {
		configured = false;
		config.context = nullptr;
	}
	File open(const char *filepath, uint8_t mode = FILE_READ) {
		//Serial.println("LittleFS open");
		if (!configured) return File();
		lfs_file_t file;
		if (mode == FILE_READ) {
			struct lfs_info info;
			if (lfs_stat(&lfs, filepath, &info) < 0) return File();
			//Serial.println("LittleFS open got info");
			if (info.type == LFS_TYPE_REG) {
				//Serial.println("  regular file");
				if (lfs_file_open(&lfs, &file, filepath, LFS_O_RDONLY) >= 0) {
					return File(new LittleFSFile(&lfs, &file));
				}
			} else {
				//Serial.println("  directory");
				lfs_dir_t dir;
				if (lfs_dir_open(&lfs, &dir, filepath) >= 0) {
					return File(new LittleFSFile(&lfs, &dir));
				}
			}
		} else {
			if (lfs_file_open(&lfs, &file, filepath, LFS_O_RDWR | LFS_O_CREAT) >= 0) {
				lfs_file_seek(&lfs, &file, 0, LFS_SEEK_END);	
				return File(new LittleFSFile(&lfs, &file));
			}
		}
		return File();
	}
	bool exists(const char *filepath) {
		if (!configured) return false;
		return false;
	}
	bool mkdir(const char *filepath) {
		if (!configured) return false;
		return false;
	}
	bool remove(const char *filepath) {
		if (!configured) return false;
		return false;
	}
	bool rmdir(const char *filepath) {
		if (!configured) return false;
		return false;
	}
protected:
	bool configured;
	lfs_t lfs;
	lfs_config config;
};





class LittleFS_RAM : public LittleFS
{
public:
	LittleFS_RAM() { }
	bool begin(uint32_t size) {
		return begin(extmem_malloc(size), size);
	}
	bool begin(void *ptr, uint32_t size) {
		//Serial.println("configure "); delay(5);
		configured = false;
		if (!ptr) return false;
		memset(ptr, 0, size); // always start with blank slate
		size = size & 0xFFFFFF00;
		memset(&lfs, 0, sizeof(lfs));
		memset(&config, 0, sizeof(config));
		config.context = ptr;
		config.read = &static_read;
		config.prog = &static_prog;
		config.erase = &static_erase;
		config.sync = &static_sync;
		config.read_size = 64;
		config.prog_size = 64;
		config.block_size = 256;
		config.block_count = size / 256;
		config.block_cycles = 50;
		config.cache_size = 64;
		config.lookahead_size = 64;
		config.name_max = 64;
		config.file_max = 0;
		config.attr_max = 0;
		if (lfs_format(&lfs, &config) < 0) return false;
		//Serial.println("formatted");
		if (lfs_mount(&lfs, &config) < 0) return false;
		//Serial.println("mounted atfer format");
		configured = true;
		return true;
	}
private:
	static int static_read(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, void *buffer, lfs_size_t size) {
		//Serial.printf("    ram rd: block=%d, offset=%d, size=%d\n", block, offset, size);
		uint32_t index = block * 256 + offset;
		memcpy(buffer, (uint8_t *)(c->context) + index, size);
		return 0;
	}
	static int static_prog(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, const void *buffer, lfs_size_t size) {
		//Serial.printf("    ram wr: block=%d, offset=%d, size=%d\n", block, offset, size);
		uint32_t index = block * 256 + offset;
		memcpy((uint8_t *)(c->context) + index, buffer, size);
		return 0;
	}
	static int static_erase(const struct lfs_config *c, lfs_block_t block) {
		uint32_t index = block * 256;
		memset((uint8_t *)(c->context) + index, 0xFF, 256);
		return 0;
	}
	static int static_sync(const struct lfs_config *c) {
		return 0;
	}
};


