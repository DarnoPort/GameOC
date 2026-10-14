#!/usr/bin/env python3
"""Pack a linked 32-bit CustomOS ELF into the CBX v1 container.

The application linker emits absolute addresses based at 0x00400000.  Therefore
we must preserve the virtual-address layout (including alignment gaps) when
building the CBX payload.  Simply concatenating .text, .rodata and .data shifts
absolute string/data references and can make applications read the wrong memory.
"""
import argparse
import struct

BASE_ADDR = 0x00400000
MAX_APP_MEMORY = 900 * 1024
SHT_NOBITS = 8
SHF_ALLOC = 0x2
SHF_EXECINSTR = 0x4

ELF_SECTION_HEADER = struct.Struct("<IIIIIIIIII")
ELF_HEADER_FIELDS = struct.Struct("<IIIIHHHHHH")
CBX_HEADER = struct.Struct("<4sHH32s64sIIIII")


def _checked_slice(blob, offset, size, label):
    if offset < 0 or size < 0 or offset > len(blob) or size > len(blob) - offset:
        raise ValueError(f"Некорректные границы {label} в ELF")
    return blob[offset:offset + size]


def pack_elf_to_cbx(elf_path, out_path, name, desc, is_gui=True):
    with open(elf_path, "rb") as f:
        elf = f.read()

    if len(elf) < 52 or elf[:4] != b"\x7fELF":
        raise ValueError("Файл не является ELF: " + elf_path)
    if elf[4] != 1 or elf[5] != 1:
        raise ValueError("Ожидался 32-битный ELF в little-endian: " + elf_path)

    # ELF32 header fields starting at e_entry (file offset 24).
    (e_entry, _e_phoff, e_shoff, _e_flags, _e_ehsize, _e_phentsize,
     _e_phnum, e_shentsize, e_shnum, e_shstrndx) = ELF_HEADER_FIELDS.unpack(elf[24:52])

    if e_shentsize != ELF_SECTION_HEADER.size:
        raise ValueError("Неожиданный размер заголовка ELF-секции")
    if e_shnum == 0 or e_shstrndx >= e_shnum:
        raise ValueError("В ELF отсутствует корректная таблица секций")
    _checked_slice(elf, e_shoff, e_shnum * ELF_SECTION_HEADER.size, "таблицы секций")

    # Resolve names for useful validation errors.
    str_sh_offset = e_shoff + e_shstrndx * ELF_SECTION_HEADER.size
    str_sh = ELF_SECTION_HEADER.unpack(
        _checked_slice(elf, str_sh_offset, ELF_SECTION_HEADER.size, "таблицы имён")
    )
    str_tab = _checked_slice(elf, str_sh[4], str_sh[5], "строковой таблицы")

    file_sections = []
    bss_ranges = []
    executable_ranges = []
    image_end = 0
    memory_end = 0
    code_end = 0

    for i in range(e_shnum):
        sh_offset_in_table = e_shoff + i * ELF_SECTION_HEADER.size
        sh = ELF_SECTION_HEADER.unpack(
            _checked_slice(elf, sh_offset_in_table, ELF_SECTION_HEADER.size, "заголовка секции")
        )
        (sh_name_idx, sh_type, sh_flags, sh_addr, sh_offset, sh_size,
         _sh_link, _sh_info, _sh_addralign, _sh_entsize) = sh

        if sh_name_idx >= len(str_tab):
            raise ValueError(f"Некорректное имя секции ELF #{i}")
        name_end = str_tab.find(b"\x00", sh_name_idx)
        if name_end < 0:
            raise ValueError(f"Незавершённое имя секции ELF #{i}")
        section_name = str_tab[sh_name_idx:name_end].decode("ascii", errors="replace")

        if not (sh_flags & SHF_ALLOC) or sh_size == 0:
            continue
        if sh_addr < BASE_ADDR:
            raise ValueError(f"Секция {section_name} расположена ниже базового адреса приложения")

        start = sh_addr - BASE_ADDR
        end = start + sh_size
        if end > MAX_APP_MEMORY:
            raise ValueError(f"Секция {section_name} выходит за лимит памяти приложения")
        memory_end = max(memory_end, end)

        if sh_type == SHT_NOBITS:
            bss_ranges.append((start, end, section_name))
            continue

        section_bytes = _checked_slice(elf, sh_offset, sh_size, f"секции {section_name}")
        file_sections.append((start, end, section_bytes, section_name))
        image_end = max(image_end, end)

        if sh_flags & SHF_EXECINSTR:
            executable_ranges.append((start, end, section_name))
            code_end = max(code_end, end)

    if not executable_ranges:
        raise ValueError("В ELF не найдена исполняемая секция")
    entry_offset = e_entry - BASE_ADDR
    if entry_offset < 0 or not any(start <= entry_offset < end for start, end, _ in executable_ranges):
        raise ValueError("Точка входа ELF находится вне исполняемых секций")

    # The loader zeroes BSS immediately after the file-backed payload.  Preserve
    # the original address layout by including alignment gaps before BSS.
    if bss_ranges:
        payload_end = min(start for start, _end, _name in bss_ranges)
        if image_end > payload_end:
            raise ValueError("Секции с данными пересекаются с .bss; такой ELF не поддерживается")
    else:
        payload_end = image_end

    if code_end > payload_end:
        raise ValueError("Исполняемый сегмент выходит за границу CBX payload")
    if payload_end > MAX_APP_MEMORY or memory_end > MAX_APP_MEMORY:
        raise ValueError("Приложение превышает лимит памяти 900 КБ")

    payload = bytearray(payload_end)
    for start, end, section_bytes, section_name in file_sections:
        if end > payload_end:
            raise ValueError(f"Секция {section_name} выходит за границу файлового образа")
        payload[start:end] = section_bytes

    code_size = code_end
    data_size = payload_end - code_size
    bss_size = memory_end - payload_end if bss_ranges else 0

    if code_size + data_size + bss_size > MAX_APP_MEMORY:
        raise ValueError("Приложение превышает лимит памяти 900 КБ")

    header = CBX_HEADER.pack(
        b"CBX1",
        1,                         # version
        1 if is_gui else 0,        # flags
        name.encode("utf-8")[:31].ljust(32, b"\x00"),
        desc.encode("utf-8")[:63].ljust(64, b"\x00"),
        entry_offset,
        code_size,
        data_size,
        bss_size,
        16384                      # recommended stack size (16 KB)
    )

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(payload)

    print(
        f"[CBXPACK] Сформирован {out_path}: код={code_size} б, "
        f"данные={data_size} б (с сохранением адресов), bss={bss_size} б"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CustomOS CBX Executable Packager")
    parser.add_argument("elf", help="Путь к результирующему 32-битному ELF")
    parser.add_argument("output", help="Путь к результирующему CBX")
    parser.add_argument("--name", default="App", help="Имя приложения (до 31 символа)")
    parser.add_argument("--desc", default="CustomOS Application", help="Описание приложения")
    parser.add_argument("--gui", action="store_true", default=True, help="Графическое приложение")

    args = parser.parse_args()
    pack_elf_to_cbx(args.elf, args.output, args.name, args.desc, args.gui)
