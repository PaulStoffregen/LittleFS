#include <Arduino.h>
#include <FS.h>
#include "littlefs/lfs.h"


class LittleFSFile : public File
{
public:
	LittleFSFile(lfs_t *lfsin, lfs_file_t *filein) {
		lfs = lfsin;
		file = filein;
		dir = nullptr;
		//Serial.printf("  LittleFSFile ctor (file), this=%x\n", (int)this);
	}
	LittleFSFile(lfs_t *lfsin, lfs_dir_t *dirin) {
		lfs = lfsin;
		dir = dirin;
		file = nullptr;
		//Serial.printf("  LittleFSFile ctor (dir), this=%x\n", (int)this);
	}
	virtual ~LittleFSFile() {
		//Serial.printf("  LittleFSFile dtor, this=%x\n", (int)this);
		close();
	}
#ifdef FILE_WHOAMI
	virtual void whoami() {
		Serial.printf("  LittleFSFile this=%x, refcount=%u\n",
			(int)this, getRefcount());
	}
#endif
	virtual size_t write(const void *buf, size_t size) {
		//Serial.println("write");
		if (!file) return 0;
		//Serial.println(" is regular file");
		return lfs_file_write(lfs, file, buf, size);
	}
	virtual int peek() {
		return -1; // TODO...
	}
	virtual int available() {
		if (!file) return 0;
		lfs_soff_t pos = lfs_file_tell(lfs, file);
		if (pos < 0) return 0;
		lfs_soff_t size = lfs_file_size(lfs, file);
		if (size < 0) return 0;
		return size - pos;
	}
	virtual void flush() {
		if (file) lfs_file_sync(lfs, file);
	}
	virtual size_t read(void *buf, size_t nbyte) {
		if (file) {
			lfs_ssize_t r = lfs_file_read(lfs, file, buf, nbyte);
			if (r < 0) r = 0;
			return r;
		}
		return 0;
	}
	virtual bool seek(uint32_t pos, int mode = SeekSet) {
		if (!file) return false;
		int whence;
		if (mode == SeekSet) whence = LFS_SEEK_SET;
		else if (mode == SeekCur) whence = LFS_SEEK_CUR;
		else if (mode == SeekEnd) whence = LFS_SEEK_END;
		else return false;
		if (lfs_file_seek(lfs, file, pos, whence) >= 0) return true;
		return false;
	}
	virtual uint32_t position() {
		if (!file) return 0;
		lfs_soff_t pos = lfs_file_tell(lfs, file);
		if (pos < 0) pos = 0;
		return pos;
	}
	virtual uint32_t size() {
		if (!file) return 0;
		lfs_soff_t size = lfs_file_size(lfs, file);
		if (size < 0) size = 0;
		return size;
	}
	virtual void close() {
		if (file) {
			//Serial.printf("  close file, this=%x, lfs=%x", (int)this, (int)lfs);
			lfs_file_close(lfs, file); // we get stuck here, but why?
			free(file);
			file = nullptr;
		}
		if (dir) {
			//Serial.printf("  close dir, this=%x, lfs=%x", (int)this, (int)lfs);
			lfs_dir_close(lfs, dir);
			free(dir);
			dir = nullptr;
		}
		//Serial.println("  end of close");
	}
	virtual operator bool() {
		return file || dir;
	}
	virtual const char * name() {
		// TODO: much work needed for this...
		static char zeroterm = 0;
		filename = &zeroterm;
		return filename;
	}
	virtual boolean isDirectory(void) {
		return dir != nullptr;
	}
	virtual File openNextFile(uint8_t mode=0) {
		if (!dir) return File();
		struct lfs_info info;
		do {
			memset(&info, 0, sizeof(info)); // is this necessary?
			if (lfs_dir_read(lfs, dir, &info) <= 0) return File();
		} while (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0);
		//Serial.printf("  next name = %s\n", info.name);
		if (info.type == LFS_TYPE_REG) {
			lfs_file_t *f = (lfs_file_t *)malloc(sizeof(lfs_file_t));
			if (!f) return File();
			if (lfs_file_open(lfs, f, info.name, LFS_O_RDONLY) >= 0) {
				return File(new LittleFSFile(lfs, f));
			}
			free(f);
		} else { // LFS_TYPE_DIR
			lfs_dir_t *d = (lfs_dir_t *)malloc(sizeof(lfs_dir_t));
			if (!d) return File();
			if (lfs_dir_open(lfs, d, info.name) >= 0) {
				return File(new LittleFSFile(lfs, d));
			}
			free(d);
		}
		return File();
	}
	virtual void rewindDirectory(void) {
		if (dir) lfs_dir_rewind(lfs, dir);
	}

	using Print::write;
private:
	lfs_t *lfs;
	lfs_file_t *file;
	lfs_dir_t *dir;
	char *filename;
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
		if (mode == FILE_READ) {
			struct lfs_info info;
			if (lfs_stat(&lfs, filepath, &info) < 0) return File();
			//Serial.printf("LittleFS open got info, name=%s\n", info.name);
			if (info.type == LFS_TYPE_REG) {
				//Serial.println("  regular file");
				lfs_file_t *file = (lfs_file_t *)malloc(sizeof(lfs_file_t));
				if (!file) return File();
				if (lfs_file_open(&lfs, file, filepath, LFS_O_RDONLY) >= 0) {
					return File(new LittleFSFile(&lfs, file));
				}
				free(file);
			} else { // LFS_TYPE_DIR
				//Serial.println("  directory");
				lfs_dir_t *dir = (lfs_dir_t *)malloc(sizeof(lfs_dir_t));
				if (!dir) return File();
				if (lfs_dir_open(&lfs, dir, filepath) >= 0) {
					return File(new LittleFSFile(&lfs, dir));
				}
				free(dir);
			}
		} else {
			lfs_file_t *file = (lfs_file_t *)malloc(sizeof(lfs_file_t));
			if (!file) return File();
			if (lfs_file_open(&lfs, file, filepath, LFS_O_RDWR | LFS_O_CREAT) >= 0) {
				lfs_file_seek(&lfs, file, 0, LFS_SEEK_END);
				return File(new LittleFSFile(&lfs, file));
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


