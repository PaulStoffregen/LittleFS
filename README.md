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

