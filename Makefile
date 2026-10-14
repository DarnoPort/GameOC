# ==============================================================================
# CustomOS - Automated Build System (Stage 7)
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

# Список приложений CustomOS
APPS_CBX      := $(BUILD_APP_DIR)/app_test.cbx \
                 $(BUILD_APP_DIR)/app_info.cbx \
                 $(BUILD_APP_DIR)/app_text.cbx \
                 $(BUILD_APP_DIR)/app_calc.cbx \
                 $(BUILD_APP_DIR)/app_game.cbx

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
        $(BUILD_DIR)/app_loader.o \
        $(BUILD_DIR)/app_registry.o \
        $(BUILD_DIR)/launcher.o

.PHONY: all clean run build_dirs

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

# --- Сборка виртуального диска AppFS ---
$(APPFS_IMG): $(APPS_CBX) tools/mkappfs.py | build_dirs
	@echo "[MKAPPFS] $(APPS_CBX) -> $@"
	@$(PYTHON) tools/mkappfs.py $@ $(APPS_CBX)

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
	@echo "=== CustomOS Kernel Stage 6 successfully built: $@ ==="

run: $(KERNEL_BIN)
	@echo "Запуск CustomOS в QEMU..."
	$(QEMU) -kernel $(KERNEL_BIN)

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Каталоги сборки очищены."