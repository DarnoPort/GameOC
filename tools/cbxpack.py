#!/usr/bin/env python3
import sys
import struct
import argparse

def pack_elf_to_cbx(elf_path, out_path, name, desc, is_gui=True):
    with open(elf_path, 'rb') as f:
        elf = f.read()

    if elf[:4] != b'\x7fELF' or elf[4] != 1:
        raise ValueError("Файл не является 32-битным ELF: " + elf_path)

    # Заголовок ELF32
    e_entry, e_phoff, e_shoff, e_flags, e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx = struct.unpack(
        "<IIIIHHHHHH", elf[24:52]
    )

    sh_fmt = "<IIIIIIIIII"
    sh_size = struct.calcsize(sh_fmt)

    # Чтение таблицы имён секций
    str_off = e_shoff + e_shstrndx * sh_size
    str_sh = struct.unpack(sh_fmt, elf[str_off:str_off + sh_size])
    str_tab = elf[str_sh[4]:str_sh[4] + str_sh[5]]

    code_bytes = bytearray()
    data_bytes = bytearray()
    bss_size = 0
    base_addr = 0x00400000

    for i in range(e_shnum):
        off = e_shoff + i * sh_size
        sh = struct.unpack(sh_fmt, elf[off:off + sh_size])
        sh_name_idx, sh_type, sh_flags, sh_addr, sh_offset, sh_size_val = sh[0], sh[1], sh[2], sh[3], sh[4], sh[5]

        name_end = str_tab.find(b'\x00', sh_name_idx)
        sec_name = str_tab[sh_name_idx:name_end].decode('ascii', errors='ignore')

        if not (sh_flags & 2): # SHF_ALLOC
            continue

        if sh_type == 8: # SHT_NOBITS (.bss)
            bss_size += sh_size_val
        elif sh_flags & 4: # SHF_EXECINSTR (.text)
            code_bytes.extend(elf[sh_offset:sh_offset + sh_size_val])
        else: # Данные (.rodata, .data)
            data_bytes.extend(elf[sh_offset:sh_offset + sh_size_val])

    entry_offset = (e_entry - base_addr) if e_entry >= base_addr else 0

    header = struct.pack(
        "<4sHH32s64sIIIII",
        b"CBX1",
        1,                         # version
        1 if is_gui else 0,        # flags
        name.encode('utf-8')[:31].ljust(32, b'\x00'),
        desc.encode('utf-8')[:63].ljust(64, b'\x00'),
        entry_offset,
        len(code_bytes),
        len(data_bytes),
        bss_size,
        16384                      # stack_size (16 KB)
    )

    with open(out_path, 'wb') as f:
        f.write(header)
        f.write(code_bytes)
        f.write(data_bytes)

    print(f"[CBXPACK] Сформирован {out_path}: код={len(code_bytes)} б, данные={len(data_bytes)} б, bss={bss_size} б")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="CustomOS CBX Executable Packager")
    parser.add_argument("elf", help="Путь к скомпилированному .elf файлу")
    parser.add_argument("output", help="Путь к результирующему .cbx файлу")
    parser.add_argument("--name", default="App", help="Имя приложения (до 31 символа)")
    parser.add_argument("--desc", default="CustomOS Application", help="Описание программы")
    parser.add_argument("--gui", action="store_true", default=True, help="Графическое приложение")

    args = parser.parse_args()
    pack_elf_to_cbx(args.elf, args.output, args.name, args.desc, args.gui)