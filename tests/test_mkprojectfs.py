import os
import struct
import tempfile
import unittest

from mkprojectfs import (
    DATA_LBA,
    DIR_ENTRIES,
    ENTRY_SIZE,
    MAGIC,
    REQUIRED_SECTORS,
    SECTOR_SIZE,
    SLOT_COUNT,
    SLOT_SECTORS,
    SUPERBLOCK,
    VERSION,
    format_image,
    main,
    verify_image,
)


class ProjectFsImageTests(unittest.TestCase):
    def test_format_and_verify_image(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "customos_data.img")
            format_image(path)
            self.assertTrue(verify_image(path))
            self.assertEqual(os.path.getsize(path) % SECTOR_SIZE, 0)
            with open(path, "rb") as stream:
                fields = SUPERBLOCK.unpack(stream.read(SUPERBLOCK.size))
            self.assertEqual(fields[0], MAGIC)
            self.assertEqual(fields[1], VERSION)
            self.assertGreaterEqual(fields[2], REQUIRED_SECTORS)
            self.assertEqual(fields[4], DIR_ENTRIES)
            self.assertEqual(fields[5], ENTRY_SIZE)
            self.assertEqual(fields[6], DATA_LBA)
            self.assertEqual(fields[7], SLOT_SECTORS)
            self.assertEqual(fields[8], SLOT_COUNT)

    def test_existing_valid_image_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "customos_data.img")
            format_image(path)
            with open(path, "r+b") as stream:
                stream.seek(DATA_LBA * SECTOR_SIZE)
                stream.write(b"existing project data")
            with open(path, "rb") as stream:
                before = stream.read()
            self.assertEqual(main([path]), 0)
            with open(path, "rb") as stream:
                after = stream.read()
            self.assertEqual(after, before)

    def test_invalid_image_is_not_overwritten_implicitly(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "customos_data.img")
            original = b"do not erase this"
            with open(path, "wb") as stream:
                stream.write(original)
            self.assertEqual(main([path]), 2)
            with open(path, "rb") as stream:
                self.assertEqual(stream.read(), original)

    def test_format_flag_is_explicitly_destructive(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "customos_data.img")
            with open(path, "wb") as stream:
                stream.write(b"old content")
            self.assertEqual(main([path, "--format"]), 0)
            self.assertTrue(verify_image(path))
            with open(path, "rb") as stream:
                stream.seek(DATA_LBA * SECTOR_SIZE)
                self.assertEqual(stream.read(11), b"\x00" * 11)


if __name__ == "__main__":
    unittest.main()
