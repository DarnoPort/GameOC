# ==============================================================================
# CustomOS - Automated Build System (Stage 8)
# ==============================================================================

CC      := gcc
AS      := nasm
LD      := ld
PYTHON  := python3
QEMU    := qemu-system-i386

CFLAGS  := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
           -nostdlib -Wall -Wextra -O2 -Isrc/kernel
ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T src/kernel/linker.ld -nostdlib

BUILD_DIR     := build
BUILD_APP_DIR := build/apps
BIN_DIR       := bin
SRC_DIR       := src

KERNEL_BIN    := $(BIN_DIR)/customos.elf
APPFS_IMG     := $(BUILD_DIR)/appfs.img
EXTERNAL_APPFS_IMG := $(BUILD_DIR)/external_apps.img
EXTERNAL_CBX ?=
DATA_DISK       := customos_data.img

# Список приложений CustomOS
APPS_CBX      := $(BUILD_APP_DIR)/app_test.cbx \
                 $(BUILD_APP_DIR)/app_info.cbx \
                 $(BUILD_APP_DIR)/app_text.cbx \
                 $(BUILD_APP_DIR)/app_calc.cbx \
                 $(BUILD_APP_DIR)/app_game.cbx \
                 $(BUILD_APP_DIR)/app_novel_editor.cbx

OBJS := $(BUILD_DIR)/boot.o \
        $(BUILD_DIR)/isr_asm.o \
        $(BUILD_DIR)/app_invoke.o \
        $(BUILD_DIR)/embedded_appfs.o \
        $(BUILD_DIR)/kernel.o \
        $(BUILD_DIR)/vga.o \
        $(BUILD_DIR)/idt.o \
        $(BUILD_DIR)/isr.o \
        $(BUILD_DIR)/pic.o \
        $(BUILD_DIR)/timer.o \
        $(BUILD_DIR)/keyboard.o \
        $(BUILD_DIR)/gfx.o \
        $(BUILD_DIR)/ide.o \
        $(BUILD_DIR)/projectfs.o \
        $(BUILD_DIR)/app_loader.o \
        $(BUILD_DIR)/app_registry.o \
        $(BUILD_DIR)/launcher.o

.PHONY: all clean run build_dirs ensure-data-disk external-apps run-external external-demo run-external-demo run-fs-test test test-novel-player

all: $(KERNEL_BIN)

build_dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_APP_DIR)
	@mkdir -p $(BIN_DIR)

# --- Правила сборки приложений в формат CBX ---

# 1. Base AppTest
$(BUILD_APP_DIR)/app_test.o: apps/app_test/app_test.c | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -O2 -I sdk/ -c $< -o $@

$(BUILD_APP_DIR)/app_test.elf: $(BUILD_APP_DIR)/app_test.o | build_dirs
	@echo "[APP-LD] $< -> $@"
	@$(LD) -m elf_i386 -T sdk/app.ld $< -o $@

$(BUILD_APP_DIR)/app_test.cbx: $(BUILD_APP_DIR)/app_test.elf tools/cbxpack.py | build_dirs
	@echo "[CBXPACK] $< -> $@"
	@$(PYTHON) tools/cbxpack.py $< $@ --name "AppTest" --desc "Base Application Test"

# 2. SysInfo
$(BUILD_APP_DIR)/app_info.o: apps/app_info/app_info.c | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -O2 -I sdk/ -c $< -o $@

$(BUILD_APP_DIR)/app_info.elf: $(BUILD_APP_DIR)/app_info.o | build_dirs
	@echo "[APP-LD] $< -> $@"
	@$(LD) -m elf_i386 -T sdk/app.ld $< -o $@

$(BUILD_APP_DIR)/app_info.cbx: $(BUILD_APP_DIR)/app_info.elf tools/cbxpack.py | build_dirs
	@echo "[CBXPACK] $< -> $@"
	@$(PYTHON) tools/cbxpack.py $< $@ --name "SysInfo" --desc "System Diagnostics & Specs"

# 3. App 1: Text Subsystem Demo
$(BUILD_APP_DIR)/app_text.o: apps/app_text/app_text.c | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -O2 -I sdk/ -c $< -o $@

$(BUILD_APP_DIR)/app_text.elf: $(BUILD_APP_DIR)/app_text.o | build_dirs
	@echo "[APP-LD] $< -> $@"
	@$(LD) -m elf_i386 -T sdk/app.ld $< -o $@

$(BUILD_APP_DIR)/app_text.cbx: $(BUILD_APP_DIR)/app_text.elf tools/cbxpack.py | build_dirs
	@echo "[CBXPACK] $< -> $@"
	@$(PYTHON) tools/cbxpack.py $< $@ --name "TextDemo" --desc "App 1: Text Layout & Contrast"

# 4. App 2: Logic & Arithmetic Demo
$(BUILD_APP_DIR)/app_calc.o: apps/app_calc/app_calc.c | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -O2 -I sdk/ -c $< -o $@

$(BUILD_APP_DIR)/app_calc.elf: $(BUILD_APP_DIR)/app_calc.o | build_dirs
	@echo "[APP-LD] $< -> $@"
	@$(LD) -m elf_i386 -T sdk/app.ld $< -o $@

$(BUILD_APP_DIR)/app_calc.cbx: $(BUILD_APP_DIR)/app_calc.elf tools/cbxpack.py | build_dirs
	@echo "[CBXPACK] $< -> $@"
	@$(PYTHON) tools/cbxpack.py $< $@ --name "LogicCalc" --desc "App 2: Logic, Math & Loops"

# 5. Stage 7: Reusable mini game engine + interactive Dodge game
$(BUILD_APP_DIR)/app_game.o: apps/app_game/app_game.c apps/app_game/game_engine.h sdk/customos.h | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -O2 -I sdk/ -I apps/app_game/ -c $< -o $@

$(BUILD_APP_DIR)/game_engine.o: apps/app_game/game_engine.c apps/app_game/game_engine.h sdk/customos.h | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -O2 -I sdk/ -I apps/app_game/ -c $< -o $@

$(BUILD_APP_DIR)/app_game.elf: $(BUILD_APP_DIR)/app_game.o $(BUILD_APP_DIR)/game_engine.o | build_dirs
	@echo "[APP-LD] $^ -> $@"
	@$(LD) -m elf_i386 -T sdk/app.ld $^ -o $@

$(BUILD_APP_DIR)/app_game.cbx: $(BUILD_APP_DIR)/app_game.elf tools/cbxpack.py | build_dirs
	@echo "[CBXPACK] $< -> $@"
	@$(PYTHON) tools/cbxpack.py $< $@ --name "Dodge" --desc "Stage 7: Game Engine & Dodge"


# Shared bounded visual-novel engine and CBX resource packer.
$(BUILD_APP_DIR)/vn_engine.o: apps/app_novel_shared/vn_engine.c apps/app_novel_shared/vn_engine.h | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -nostdlib -Wall -Wextra -O2 -I sdk/ -I apps/app_novel_shared/ -c $< -o $@

$(BUILD_APP_DIR)/vn_package.o: apps/app_novel_shared/vn_package.c apps/app_novel_shared/vn_package.h | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -nostdlib -Wall -Wextra -O2 -I sdk/ -I apps/app_novel_shared/ -c $< -o $@

$(BUILD_APP_DIR)/app_novel_player.o: apps/app_novel_player/app_novel_player.c apps/app_novel_shared/vn_engine.h apps/app_novel_shared/vn_package.h sdk/customos.h | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -nostdlib -Wall -Wextra -O2 -I sdk/ -I apps/app_novel_shared/ -c $< -o $@

$(BUILD_APP_DIR)/app_novel_player.elf: $(BUILD_APP_DIR)/app_novel_player.o $(BUILD_APP_DIR)/vn_engine.o $(BUILD_APP_DIR)/vn_package.o | build_dirs
	@echo "[APP-LD] $^ -> $@"
	@$(LD) -m elf_i386 -T sdk/app.ld $^ -o $@

$(BUILD_APP_DIR)/app_novel_player.cbx: $(BUILD_APP_DIR)/app_novel_player.elf tools/cbxpack.py | build_dirs
	@echo "[CBXPACK] $< -> $@"
	@$(PYTHON) tools/cbxpack.py $< $@ --name "NovelPlayer" --desc "Standalone Visual Novel Player"

$(BUILD_APP_DIR)/novel_player_template.h: $(BUILD_APP_DIR)/app_novel_player.cbx tools/bin2c.py | build_dirs
	@$(PYTHON) tools/bin2c.py $< $@ --symbol vn_player_template


$(BUILD_APP_DIR)/app_novel_editor.o: apps/app_novel_editor/app_novel_editor.c apps/app_novel_shared/vn_engine.h apps/app_novel_shared/vn_package.h sdk/customos.h $(BUILD_APP_DIR)/novel_player_template.h | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -nostdlib -Wall -Wextra -O2 -I sdk/ -I apps/app_novel_shared/ -I $(BUILD_APP_DIR)/ -c $< -o $@

$(BUILD_APP_DIR)/app_novel_editor.elf: $(BUILD_APP_DIR)/app_novel_editor.o $(BUILD_APP_DIR)/vn_engine.o $(BUILD_APP_DIR)/vn_package.o | build_dirs
	@echo "[APP-LD] $^ -> $@"
	@$(LD) -m elf_i386 -T sdk/app.ld $^ -o $@

$(BUILD_APP_DIR)/app_novel_editor.cbx: $(BUILD_APP_DIR)/app_novel_editor.elf tools/cbxpack.py | build_dirs
	@echo "[CBXPACK] $< -> $@"
	@$(PYTHON) tools/cbxpack.py $< $@ --name "NovelStudio" --desc "Visual Novel Studio for CustomOS"

# Standalone demo used to test the external-app import path (not built into AppFS).
$(BUILD_APP_DIR)/external_demo.o: apps/external_demo/external_demo.c sdk/customos.h | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -O2 -I sdk/ -c $< -o $@

$(BUILD_APP_DIR)/external_demo.elf: $(BUILD_APP_DIR)/external_demo.o | build_dirs
	@echo "[APP-LD] $< -> $@"
	@$(LD) -m elf_i386 -T sdk/app.ld $< -o $@

$(BUILD_APP_DIR)/external_demo.cbx: $(BUILD_APP_DIR)/external_demo.elf tools/cbxpack.py | build_dirs
	@echo "[CBXPACK] $< -> $@"
	@$(PYTHON) tools/cbxpack.py $< $@ --name "ExternalDemo" --desc "External App Transfer Test"

external-demo: $(BUILD_APP_DIR)/external_demo.cbx

run-external-demo: $(KERNEL_BIN) $(BUILD_APP_DIR)/external_demo.cbx ensure-data-disk | build_dirs
	@$(PYTHON) tools/mkappfs.py $(EXTERNAL_APPFS_IMG) $(BUILD_APP_DIR)/external_demo.cbx
	@echo "Запуск CustomOS с тестовым внешним приложением..."
	$(QEMU) -kernel $(KERNEL_BIN) -initrd $(EXTERNAL_APPFS_IMG) -drive file=$(DATA_DISK),format=raw,if=ide,index=0



# Host-side unit tests exercise the same freestanding engine compiled into CustomOS apps.
HOSTCC ?= gcc

$(BUILD_DIR)/tests/vn_engine_test: apps/app_novel_shared/vn_engine.c apps/app_novel_shared/vn_engine.h tests/vn_engine_test.c | build_dirs
	@mkdir -p $(BUILD_DIR)/tests
	@echo "[HOST-TEST-CC] scenario parser and runtime"
	@$(HOSTCC) -std=c11 -Wall -Wextra -Werror -O2 -I apps/app_novel_shared apps/app_novel_shared/vn_engine.c tests/vn_engine_test.c -o $@


$(BUILD_DIR)/tests/vn_package_test: apps/app_novel_shared/vn_package.c apps/app_novel_shared/vn_package.h tests/vn_package_test.c | build_dirs
	@mkdir -p $(BUILD_DIR)/tests
	@echo "[HOST-TEST-CC] novel CBX packer"
	@$(HOSTCC) -std=c11 -Wall -Wextra -Werror -O2 -I apps/app_novel_shared apps/app_novel_shared/vn_package.c tests/vn_package_test.c -o $@


$(BUILD_DIR)/tools/pack_novel_player_host: tools/pack_novel_player_host.c apps/app_novel_shared/vn_package.c apps/app_novel_shared/vn_package.h apps/app_novel_shared/vn_engine.h | build_dirs
	@mkdir -p $(BUILD_DIR)/tools
	@echo "[HOST-CC] standalone player test packer"
	@$(HOSTCC) -std=c11 -Wall -Wextra -Werror -O2 -I apps/app_novel_shared tools/pack_novel_player_host.c apps/app_novel_shared/vn_package.c -o $@

test-novel-player: $(KERNEL_BIN) $(BUILD_APP_DIR)/app_novel_player.cbx $(BUILD_DIR)/tools/pack_novel_player_host
	@python3 tools/test_novel_player_qemu.py --kernel $(KERNEL_BIN) --template $(BUILD_APP_DIR)/app_novel_player.cbx --packer $(BUILD_DIR)/tools/pack_novel_player_host

# ProjectFS persistence diagnostic (kept external; not included in the regular launcher).
$(BUILD_APP_DIR)/app_fs_test.o: apps/app_fs_test/app_fs_test.c sdk/customos.h | build_dirs
	@echo "[APP-CC] $< -> $@"
	@$(CC) -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -O2 -I sdk/ -c $< -o $@

$(BUILD_APP_DIR)/app_fs_test.elf: $(BUILD_APP_DIR)/app_fs_test.o | build_dirs
	@echo "[APP-LD] $< -> $@"
	@$(LD) -m elf_i386 -T sdk/app.ld $< -o $@

$(BUILD_APP_DIR)/app_fs_test.cbx: $(BUILD_APP_DIR)/app_fs_test.elf tools/cbxpack.py | build_dirs
	@echo "[CBXPACK] $< -> $@"
	@$(PYTHON) tools/cbxpack.py $< $@ --name "FsPersistTest" --desc "ProjectFS persistence test"

run-fs-test: $(KERNEL_BIN) $(BUILD_APP_DIR)/app_fs_test.cbx ensure-data-disk | build_dirs
	@$(PYTHON) tools/mkappfs.py $(EXTERNAL_APPFS_IMG) $(BUILD_APP_DIR)/app_fs_test.cbx
	@echo "Запуск CustomOS с диагностическим приложением ProjectFS..."
	$(QEMU) -kernel $(KERNEL_BIN) -initrd $(EXTERNAL_APPFS_IMG) -drive file=$(DATA_DISK),format=raw,if=ide,index=0

# Persistent ProjectFS data disk. The image is outside build/ so make clean keeps user data.
ensure-data-disk:
	@$(PYTHON) tools/mkprojectfs.py $(DATA_DISK)

# --- Сборка виртуального диска AppFS ---
$(APPFS_IMG): $(APPS_CBX) tools/mkappfs.py | build_dirs
	@echo "[MKAPPFS] $(APPS_CBX) -> $@"
	@$(PYTHON) tools/mkappfs.py $@ $(APPS_CBX)

# --- External package: make run-external EXTERNAL_CBX="path/app1.cbx path/app2.cbx" ---
# CBX files are packed into a separate AppFS module passed to the Multiboot loader.
external-apps: build_dirs
	@if [ -z "$(strip $(EXTERNAL_CBX))" ]; then \
		echo "Usage: make external-apps EXTERNAL_CBX='path/to/app.cbx [path/to/another.cbx]'"; \
		exit 2; \
	fi
	@$(PYTHON) tools/mkappfs.py $(EXTERNAL_APPFS_IMG) $(EXTERNAL_CBX)

run-external: $(KERNEL_BIN) external-apps ensure-data-disk
	@echo "Запуск CustomOS с внешним пакетом $(EXTERNAL_APPFS_IMG)..."
	$(QEMU) -kernel $(KERNEL_BIN) -initrd $(EXTERNAL_APPFS_IMG) -drive file=$(DATA_DISK),format=raw,if=ide,index=0

# --- Сборка компонентов ядра ---
$(BUILD_DIR)/boot.o: $(SRC_DIR)/boot/boot.asm | build_dirs
	@echo "[AS]  $< -> $@"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/isr_asm.o: $(SRC_DIR)/kernel/isr.asm | build_dirs
	@echo "[AS]  $< -> $@"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/app_invoke.o: $(SRC_DIR)/kernel/app_invoke.asm | build_dirs
	@echo "[AS]  $< -> $@"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/embedded_appfs.o: $(SRC_DIR)/kernel/embedded_appfs.asm $(APPFS_IMG) | build_dirs
	@echo "[AS]  $< -> $@"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.c | build_dirs
	@echo "[CC]  $< -> $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJS) | build_dirs
	@echo "[LD]  Component objects -> $@"
	@$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "=== CustomOS Kernel Stage 8 successfully built: $@ ==="

run: $(KERNEL_BIN) ensure-data-disk
	@echo "Запуск CustomOS в QEMU..."
	$(QEMU) -kernel $(KERNEL_BIN) -drive file=$(DATA_DISK),format=raw,if=ide,index=0

test: $(BUILD_APP_DIR)/app_fs_test.cbx $(BUILD_DIR)/tests/vn_engine_test $(BUILD_DIR)/tests/vn_package_test $(BUILD_APP_DIR)/app_novel_player.cbx $(BUILD_APP_DIR)/novel_player_template.h
	@$(BUILD_DIR)/tests/vn_engine_test
	@$(BUILD_DIR)/tests/vn_package_test
	@PYTHONPATH=tools $(PYTHON) -m unittest discover -s tests -p 'test_*.py'

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Каталоги сборки очищены. Образ customos_data.img и пользовательские проекты сохранены."