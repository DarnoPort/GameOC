#!/usr/bin/env python3
"""Boot a packaged standalone novel inside CustomOS and verify visible dialogue/menu/jump."""
from __future__ import annotations

import argparse
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
from mkappfs import create_appfs  # noqa: E402
from mkprojectfs import format_image  # noqa: E402
from test_projectfs_qemu import decode_status_line, monitor_command  # noqa: E402

def run_player(qemu: str, kernel: Path, appfs: Path, disk: Path,
               work: Path) -> dict[str, str]:
    monitor_path = work / "player-monitor.sock"
    process = subprocess.Popen([
        qemu, "-machine", "pc", "-m", "64M", "-display", "none", "-no-reboot",
        "-kernel", str(kernel), "-initrd", str(appfs),
        "-drive", f"file={disk},format=raw,if=ide,index=0",
        "-monitor", f"unix:{monitor_path},server,nowait",
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    channel = None
    screens: dict[str, str] = {}
    try:
        deadline = time.monotonic() + 15.0
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"QEMU exited before UI test, status {process.returncode}")
            if monitor_path.exists():
                candidate = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                candidate.settimeout(2.0)
                try:
                    candidate.connect(str(monitor_path))
                    channel = candidate
                    break
                except OSError:
                    candidate.close()
            time.sleep(0.1)
        if channel is None:
            raise RuntimeError("QEMU monitor socket did not appear")

        time.sleep(2.0)
        monitor_command(channel, "sendkey i", 0.4)
        monitor_command(channel, "sendkey ret", 0.8)
        time.sleep(0.5)
        first = work / "player-first.ppm"
        monitor_command(channel, f"screendump {first}", 0.25)
        screens["first_dialogue"] = decode_status_line(first, 42, 12, 36)

        monitor_command(channel, "sendkey space", 0.4)
        menu = work / "player-menu.ppm"
        monitor_command(channel, f"screendump {menu}", 0.25)
        # With choice 0 selected, the second option has a plain black background
        # and can be decoded directly with the stock 8x8 ASCII font.
        screens["menu_second_choice"] = decode_status_line(menu, 63, 16, 34)

        monitor_command(channel, "sendkey ret", 0.5)
        selected = work / "player-selected.ppm"
        monitor_command(channel, f"screendump {selected}", 0.25)
        screens["selected_branch"] = decode_status_line(selected, 42, 12, 36)

        monitor_command(channel, "sendkey space", 0.5)
        jumped = work / "player-jump.ppm"
        monitor_command(channel, f"screendump {jumped}", 0.25)
        screens["jump_target"] = decode_status_line(jumped, 42, 12, 36)

        monitor_command(channel, "sendkey space", 0.5)
        ending = work / "player-ending.ppm"
        monitor_command(channel, f"screendump {ending}", 0.25)
        screens["ending"] = decode_status_line(ending, 64, 8, 36)

        monitor_command(channel, "sendkey space", 0.2)
        monitor_command(channel, "quit", 0.2)
        try:
            process.wait(timeout=10.0)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=4.0)
            raise RuntimeError("QEMU did not stop after monitor quit")
        return screens
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
    parser.add_argument("--template", type=Path, default=ROOT / "build/apps/app_novel_player.cbx")
    parser.add_argument("--packer", type=Path, default=ROOT / "build/tools/pack_novel_player_host")
    parser.add_argument("--qemu", default="qemu-system-i386")
    args = parser.parse_args()

    qemu = shutil.which(args.qemu)
    kernel = args.kernel.resolve()
    template = args.template.resolve()
    packer = args.packer.resolve()
    fixture = ROOT / "tests/fixtures/novel_player_demo.scr"
    if qemu is None:
        print(f"[PLAYER-TEST] ERROR: {args.qemu} was not found", file=sys.stderr)
        return 2
    for label, path in (("kernel", kernel), ("player template", template),
                        ("host packer", packer), ("demo scenario", fixture)):
        if not path.is_file():
            print(f"[PLAYER-TEST] ERROR: {label} missing: {path}", file=sys.stderr)
            return 2

    with tempfile.TemporaryDirectory(prefix="customos-novel-player-") as temporary:
        work = Path(temporary)
        game = work / "StandaloneDemo.cbx"
        appfs = work / "player-appfs.img"
        disk = work / "player-data.img"
        packed = subprocess.run(
            [str(packer), str(template), str(fixture), str(game), "PlayerDemo"],
            capture_output=True, text=True, check=False)
        if packed.returncode != 0:
            print(packed.stdout, end="")
            print(packed.stderr, end="", file=sys.stderr)
            return 1
        print(packed.stdout, end="")
        try:
            create_appfs(str(appfs), [str(game)])
            format_image(str(disk))
            screens = run_player(qemu, kernel, appfs, disk, work)
        except Exception as error:
            print(f"[PLAYER-TEST] FAIL: {error}", file=sys.stderr)
            return 1

        for name, value in screens.items():
            print(f"[PLAYER-TEST] {name}: {value!r}")

        expectations = {
            "first_dialogue": "Welcome to standalone player test",
            "menu_second_choice": "Second route:",
            "selected_branch": "You selected route one",
            "jump_target": "Route one reached",
            "ending": "The story has ended.",
        }
        for name, expected in expectations.items():
            if expected not in screens.get(name, ""):
                print(f"[PLAYER-TEST] FAIL: {name} did not contain {expected!r}",
                      file=sys.stderr)
                return 1
        print("[PLAYER-TEST] PASS: standalone CBX shows dialogue, menu, jump target and ending.")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
