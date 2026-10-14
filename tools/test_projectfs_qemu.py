#!/usr/bin/env python3
"""End-to-end persistence check for the CustomOS ProjectFS using QEMU's IDE disk."""

from __future__ import annotations

import argparse
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
SECTOR = 512
SUPERBLOCK = struct.Struct("<4sIIIIIIII")
ENTRY = struct.Struct("<20sIIBBH")
DIR_LBA = 1
DATA_LBA = 16
SLOT_SECTORS = 128
SLOT_COUNT = 128
FS_RESULT = "FS_RESULT.DAT"
FS_MARKER = "FS_TEST.DAT"


def fail(message: str) -> None:
    raise RuntimeError(message)


def inspect_file(image_path: Path, name: str) -> bytes | None:
    with image_path.open("rb") as stream:
        sector0 = stream.read(SECTOR)
        if len(sector0) < SUPERBLOCK.size:
            fail("disk image is shorter than one sector")
        values = SUPERBLOCK.unpack(sector0[: SUPERBLOCK.size])
        magic, version, total, dir_lba, count, entry_size, data_lba, slot_sectors, slots = values
        if (
            magic != b"CFS1"
            or version != 1
            or total != image_path.stat().st_size // SECTOR
            or dir_lba != DIR_LBA
            or count != 128
            or entry_size != ENTRY.size
            or data_lba != DATA_LBA
            or slot_sectors != SLOT_SECTORS
            or slots != SLOT_COUNT
        ):
            fail(f"invalid ProjectFS superblock: {values!r}")

        stream.seek(DIR_LBA * SECTOR)
        directory = stream.read(8 * SECTOR)
        for index in range(SLOT_COUNT):
            offset = index * ENTRY.size
            raw_name, size, crc, slot, flags, _reserved = ENTRY.unpack(
                directory[offset : offset + ENTRY.size]
            )
            entry_name = raw_name.split(b"\x00", 1)[0].decode("ascii", errors="strict")
            if flags != 1 or entry_name != name:
                continue
            if slot >= SLOT_COUNT or size > SLOT_SECTORS * SECTOR:
                fail(f"invalid {name} directory entry")
            stream.seek((DATA_LBA + slot * SLOT_SECTORS) * SECTOR)
            data = stream.read(size)
            if len(data) != size:
                fail(f"truncated data for {name}")
            if (zlib.crc32(data) & 0xFFFFFFFF) != crc:
                fail(f"CRC mismatch for {name}")
            return data
    return None


def read_monitor_responses(channel: socket.socket) -> None:
    channel.setblocking(False)
    try:
        while True:
            try:
                block = channel.recv(65536)
                if not block:
                    break
            except (BlockingIOError, OSError):
                break
    finally:
        channel.setblocking(True)
        channel.settimeout(2.0)


def monitor_command(channel: socket.socket, command: str, delay: float = 0.15) -> None:
    channel.sendall(command.encode("ascii") + b"\n")
    time.sleep(delay)
    read_monitor_responses(channel)


def run_one_boot(qemu: str, kernel: Path, appfs: Path, disk: Path, monitor_path: Path) -> None:
    command = [
        qemu,
        "-machine", "pc",
        "-m", "64M",
        "-display", "none",
        "-no-reboot",
        "-kernel", str(kernel),
        "-initrd", str(appfs),
        "-drive", f"file={disk},format=raw,if=ide,index=0",
        "-monitor", f"unix:{monitor_path},server,nowait",
    ]
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    channel = None
    try:
        deadline = time.monotonic() + 15.0
        while time.monotonic() < deadline:
            if process.poll() is not None:
                fail(f"QEMU exited early with status {process.returncode}")
            if monitor_path.exists():
                try:
                    channel = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    channel.settimeout(2.0)
                    channel.connect(str(monitor_path))
                    break
                except OSError:
                    if channel:
                        channel.close()
                    channel = None
            time.sleep(0.1)
        if channel is None:
            fail("QEMU monitor socket did not become available")

        # Wait for the kernel and app launcher to settle before sending input.
        time.sleep(2.0)
        read_monitor_responses(channel)
        monitor_command(channel, "sendkey i", 0.4)
        # Built-in launcher currently lists five applications; the external
        # persistence test is imported as item 6.
        for _ in range(5):
            monitor_command(channel, "sendkey down", 0.18)
        monitor_command(channel, "sendkey enter", 0.7)
        # ProjectFS operations run synchronously inside the diagnostic app.
        time.sleep(1.0)
        monitor_command(channel, "sendkey esc", 0.25)
        monitor_command(channel, "quit", 0.2)
        try:
            process.wait(timeout=10.0)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=5.0)
            fail("QEMU did not shut down after monitor quit")
    finally:
        if channel is not None:
            channel.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        try:
            monitor_path.unlink()
        except FileNotFoundError:
            pass


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", type=Path, default=ROOT / "bin/customos.elf")
    parser.add_argument("--app", type=Path, default=ROOT / "build/apps/app_fs_test.cbx")
    parser.add_argument("--qemu", default="qemu-system-i386")
    args = parser.parse_args()

    qemu = shutil.which(args.qemu)
    if qemu is None:
        print(f"[QEMU-TEST] ERROR: {args.qemu} was not found", file=sys.stderr)
        return 2
    kernel = args.kernel.resolve()
    app = args.app.resolve()
    if not kernel.is_file() or not app.is_file():
        print("[QEMU-TEST] ERROR: kernel or diagnostic CBX is missing", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="customos-projectfs-") as temp:
        work = Path(temp)
        disk = work / "test-disk.img"
        appfs = work / "external-apps.img"
        try:
            sys.path.insert(0, str(TOOLS))
            from mkappfs import create_appfs
            from mkprojectfs import format_image

            format_image(str(disk))
            create_appfs(str(appfs), [str(app)])
            run_one_boot(qemu, kernel, appfs, disk, work / "monitor-1.sock")
            first_result = inspect_file(disk, FS_RESULT)
            marker = inspect_file(disk, FS_MARKER)
            if first_result != b"MARKER_CREATED":
                fail(f"first boot result was {first_result!r}, expected MARKER_CREATED")
            if marker is None or not marker.startswith(b"CustomOS ProjectFS persistence marker"):
                fail("first boot did not persist the expected marker file")
            print("[QEMU-TEST] First boot wrote a marker to the IDE disk.")

            run_one_boot(qemu, kernel, appfs, disk, work / "monitor-2.sock")
            second_result = inspect_file(disk, FS_RESULT)
            marker_after_reboot = inspect_file(disk, FS_MARKER)
            if second_result != b"PERSISTENCE_PASS":
                fail(f"second boot result was {second_result!r}, expected PERSISTENCE_PASS")
            if marker_after_reboot != marker:
                fail("persistent marker changed between boots")
            print("[QEMU-TEST] Second boot read the prior marker successfully.")
            print("[QEMU-TEST] PASS: ProjectFS survives a complete QEMU reboot.")
            return 0
        except Exception as exc:  # Render the exact failing condition in CI.
            print(f"[QEMU-TEST] FAIL: {exc}", file=sys.stderr)
            return 1


if __name__ == "__main__":
    raise SystemExit(main())
