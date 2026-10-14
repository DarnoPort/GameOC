#ifndef IDE_H
#define IDE_H

#include "types.h"

#define IDE_SECTOR_SIZE 512u

/* Primary IDE master, LBA28, programmed-I/O. */
int ide_init(void);
int ide_is_available(void);
int ide_read_sector(uint32_t lba, uint8_t *buffer);
int ide_write_sector(uint32_t lba, const uint8_t *buffer);
int ide_flush(void);

#endif
