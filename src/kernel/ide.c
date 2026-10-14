#include "ide.h"
#include "io.h"

#define ATA_DATA       0x1F0
#define ATA_ERROR      0x1F1
#define ATA_SECCOUNT   0x1F2
#define ATA_LBA_LOW    0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HIGH   0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_STATUS     0x1F7
#define ATA_COMMAND    0x1F7
#define ATA_CONTROL    0x3F6

#define ATA_SR_ERR     0x01
#define ATA_SR_DRQ     0x08
#define ATA_SR_DF      0x20
#define ATA_SR_BSY     0x80

#define ATA_CMD_IDENTIFY     0xEC
#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_FLUSH_CACHE  0xE7

#define ATA_POLL_LIMIT 1000000u
#define ATA_LBA28_LIMIT 0x10000000u

static int ata_initialized = 0;
static int ata_present = 0;

static uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static void ata_delay_400ns(void) {
    (void)inb(ATA_CONTROL);
    (void)inb(ATA_CONTROL);
    (void)inb(ATA_CONTROL);
    (void)inb(ATA_CONTROL);
}

static int ata_wait_not_busy(void) {
    for (uint32_t i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status == 0x00 || status == 0xFF) return -1;
        if (status & ATA_SR_BSY) continue;
        if (status & (ATA_SR_ERR | ATA_SR_DF)) return -2;
        return 0;
    }
    return -3;
}

static int ata_wait_drq(void) {
    for (uint32_t i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status == 0x00 || status == 0xFF) return -1;
        if (status & ATA_SR_BSY) continue;
        if (status & (ATA_SR_ERR | ATA_SR_DF)) return -2;
        if (status & ATA_SR_DRQ) return 0;
    }
    return -3;
}

static int ata_select_lba(uint32_t lba) {
    if (lba >= ATA_LBA28_LIMIT) return -1;
    outb(ATA_DRIVE, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    ata_delay_400ns();
    if (ata_wait_not_busy() != 0) return -2;

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFFu));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFFu));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFFu));
    return 0;
}

int ide_init(void) {
    uint8_t status;

    ata_initialized = 1;
    ata_present = 0;

    /* IDENTIFY is bounded: a missing disk must not hang the kernel. */
    outb(ATA_DRIVE, 0xA0);
    ata_delay_400ns();
    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    status = inb(ATA_STATUS);
    if (status == 0x00 || status == 0xFF) return -1;

    for (uint32_t i = 0; i < ATA_POLL_LIMIT; i++) {
        status = inb(ATA_STATUS);
        if (status == 0x00 || status == 0xFF) return -1;
        if (status & ATA_SR_BSY) continue;
        if (status & ATA_SR_ERR) return -2;
        if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HIGH) != 0) return -3;
        if (status & ATA_SR_DRQ) {
            for (uint32_t word = 0; word < 256; word++) (void)inw(ATA_DATA);
            ata_present = 1;
            return 0;
        }
    }

    return -4;
}

int ide_is_available(void) {
    return ata_initialized && ata_present;
}

int ide_read_sector(uint32_t lba, uint8_t *buffer) {
    if (!ide_is_available() || buffer == NULL) return -1;
    if (ata_select_lba(lba) != 0) return -2;

    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);
    ata_delay_400ns();
    if (ata_wait_drq() != 0) return -3;

    for (uint32_t i = 0; i < 256; i++) {
        uint16_t value = inw(ATA_DATA);
        buffer[i * 2] = (uint8_t)(value & 0xFFu);
        buffer[i * 2 + 1] = (uint8_t)(value >> 8);
    }
    ata_delay_400ns();
    return 0;
}

int ide_write_sector(uint32_t lba, const uint8_t *buffer) {
    if (!ide_is_available() || buffer == NULL) return -1;
    if (ata_select_lba(lba) != 0) return -2;

    outb(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);
    ata_delay_400ns();
    if (ata_wait_drq() != 0) return -3;

    for (uint32_t i = 0; i < 256; i++) {
        uint16_t value = (uint16_t)buffer[i * 2] |
                         ((uint16_t)buffer[i * 2 + 1] << 8);
        outw(ATA_DATA, value);
    }
    ata_delay_400ns();
    return ata_wait_not_busy();
}

int ide_flush(void) {
    if (!ide_is_available()) return -1;
    if (ata_wait_not_busy() != 0) return -2;
    outb(ATA_COMMAND, ATA_CMD_FLUSH_CACHE);
    ata_delay_400ns();
    return ata_wait_not_busy();
}
