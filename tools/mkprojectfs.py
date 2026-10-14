#!/usr/bin/env python3
"""Create or verify the small fixed-slot CustomOS project disk image."""
import argparse
import os
import struct
import sys

SECTOR_SIZE = 512
IMAGE_SIZE = 16 * 1024 * 1024
MAGIC = b"CFS1"
VERSION = 1
DIR_LBA = 1
DIR_ENTRIES = 128
ENTRY_SIZE = 32
DIR_SECTORS = DIR_ENTRIES * ENTRY_SIZE // SECTOR_SIZE
DATA_LBA = 16
SLOT_SECTORS = 128
SLOT_COUNT = 128
REQUIRED_SECTORS = DATA_LBA + SLOT_SECTORS * SLOT_COUNT
SUPERBLOCK = struct.Struct("<4sIIIIIIII")


def superblock_bytes(total_sectors: int) -> bytes:
    values = (
        MAGIC, VERSION, total_sectors, DIR_LBA, DIR_ENTRIES, ENTRY_SIZE,
        DATA_LBA, SLOT_SECTORS, SLOT_COUNT,
    )
    return SUPERBLOCK.pack(*values).ljust(SECTOR_SIZE, b"\x00")


def verify_image(path: str) -> bool:
    with open(path, "rb") as stream:
        header = stream.read(SECTOR_SIZE)
        size = os.path.getsize(path)
    if len(header) != SECTOR_SIZE or size % SECTOR_SIZE:
        return False
    try:
        fields = SUPERBLOCK.unpack(header[:SUPERBLOCK.size])
    except struct.error:
        return False
    magic, version, total_sectors, dir_lba, dir_entries, entry_size, data_lba, slot_sectors, slot_count = fields
    return (
        magic == MAGIC
        and version == VERSION
        and total_sectors == size // SECTOR_SIZE
        and total_sectors >= REQUIRED_SECTORS
        and dir_lba == DIR_LBA
        and dir_entries == DIR_ENTRIES
        and entry_size == ENTRY_SIZE
        and data_lba == DATA_LBA
        and slot_sectors == SLOT_SECTORS
        and slot_count == SLOT_COUNT
    )


def format_image(path: str) -> None:
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    total_sectors = IMAGE_SIZE // SECTOR_SIZE
    with open(path, "wb") as stream:
        stream.truncate(IMAGE_SIZE)
        stream.seek(0)
        stream.write(superblock_bytes(total_sectors))
    print(f"[PROJECTFS] Created {path} ({IMAGE_SIZE} bytes)")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", help="raw disk image used by QEMU")
    parser.add_argument(
        "--format", action="store_true",
        help="destructively reformat an existing image (all projects are erased)",
    )
    args = parser.parse_args(argv)

    if os.path.exists(args.image) and not args.format:
        if verify_image(args.image):
            print(f"[PROJECTFS] Existing image is valid; preserving data: {args.image}")
            return 0
        print(
            f"[PROJECTFS] ERROR: {args.image} exists but is not a valid ProjectFS image. "
            "It was not changed. Use --format only if you intend to erase it.",
            file=sys.stderr,
        )
        return 2

    format_image(args.image)
    if not verify_image(args.image):
        print("[PROJECTFS] ERROR: verification failed after formatting", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
