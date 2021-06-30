# LittleFS

This is a wrapper for the LittleFS File System for the Teensy family of microprocessors and provides support for RAM Disks, NOR and NAND Flash chips, and FRAM chips.  For the NOR, NAND Flash support is provided for SPI and QSPI in the case of the Teensy 4.1.  For FRAM only SPI is supported.

In addition, LittleFS is linked with the SD library so the user can create, delete, read/write files to any of the supported memory type.  See usuage section. 

## Supported Chips

### NOR Flash

####
MFG | PART # | Size
------------ | ------------- |------------ 
Winbond | W25Q16JV*IQ/W25Q16FV | 16Mb
... | W25Q32JV*IQ/W25Q32FV | 32Mb
... | W25Q64JV*IQ/W25Q64FV | 64Mb
... | W25Q128JV*IQ/W25Q128FV | 128Mb
... | W25Q256JV*IQ | 256Mb
... | Winbond W25Q512JV*IQ | 512Mb
... | W25Q64JV*IM (DTR) | 64Mb
... | W25Q128JV*IM (DTR) | 128Mb
... | W25Q256JV*IM (DTR) | 256Mb
... | W25Q512JV*IM (DTR) | 512Mb
Adesto/Atmel | AT25SF041 | 4Mb
Spansion | S25FL208K | 8Mb

### NAND Flash

MFG | PART # | Size
------------ | ------------- |------------ 
Winbond  | W25N01G | 1Gb
... | W25N02G | 2Gb
... | W25M02 | 2Gb


### FRAM

MFG | PART # | Size
------------ | ------------- |------------ 
Cypress | CY15B108QN-40SXI | 8Mb
... | FM25V10-G | 1Mb
... | FM25V10-G rev1 | 1Mb
... | CY15B104Q-SXI | 4Mb
ROHM | MR45V100A | 1Mb


## USUAGE

### RAM Disk

A RAM disk can be setup to use space on the PSRAM chip in the case of the Teensy 4.1 or to use DMAMEM or RAM space on the Teensy itself.  If using DMAMEM or RAM the amount of available space to create these disks to going to be dependent on the Teensy model you are using so be careful.  Not to worry though it will let you know if you are out of space on compile.

To create a RAM disk  you need to first allocate the space based on the type of memory area, for the Teensy 4.1 for example you can use these settings:
1. ```EXTMEM char buf[8 * 1024 * 1024]; ```  This allocates all 8Mb of the PSRAM for the RAM disk.
2. ```DMAMEM char buf[490000]; ``` This allocates 490K of the DMAMEM
3. ```char buf[390000]; ``` This allocates 390K of RAM

Afer allocating space you specify that you want to create a RAM Disk:

```LittleFS_RAM myfs;```

In setup you have to specify to begin using the memory area:
```
 if (!myfs.begin(buf, sizeof(buf))) {
    Serial.printf("Error starting %s\n", "RAM DISK);
  } 
```
  
At this point you can access or create files in the same manner as you would with an SD Card using the SD Library bundled with Teensyduio.

### QSPI

QSPI is only supported on the Teensy 4.1.  These are the Flash chips that you would solder onto the bottom side of the Teesny 4.1.  To access Flash NOR or NAND chips QSPI is similar to the RAM disk:
1. For a NAND Flash chip use the following construtor: ```LittleFS_QPINAND myfs;```
2. For a NOR flash ```LittleFS_QSPIFlash myfs; ```

And then in setup all you need for the NAND or NOR QSPI is:
```
 if (!myfs.begin() {
    Serial.printf("Error starting %s\n", "QSPI");
  } 
```

### SPI


