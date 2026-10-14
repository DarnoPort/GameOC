#!/usr/bin/env python3
import sys
import os
import struct

APPFS_MAGIC = b"AFS1"

def create_appfs(output_path, cbx_files):
    # Формат одной записи каталога:
    # filename[32], name[32], description[64], offset[4], size[4] (всего 136 байт)
    entry_fmt = "<32s32s64sII"
    entry_size = struct.calcsize(entry_fmt)
    
    file_count = len(cbx_files)
    table_offset = 8 + file_count * entry_size # 4 байта magic + 4 байта count

    entries = []
    file_blobs = []
    current_offset = table_offset

    for file_path in cbx_files:
        if not os.path.exists(file_path):
            raise FileNotFoundError(f"Файл {file_path} не найден!")

        with open(file_path, "rb") as f:
            data = f.read()

        if len(data) < 124 or data[:4] != b"CBX1":
            raise ValueError(f"Файл {file_path} не является валидным CBX бинарником!")

        # Извлекаем имя и описание из заголовка CBX
        _, _, _, cbx_name, cbx_desc = struct.unpack("<4sHH32s64s", data[:104])
        name_str = cbx_name.split(b'\x00')[0]
        desc_str = cbx_desc.split(b'\x00')[0]
        base_name = os.path.basename(file_path).encode('utf-8')[:31]

        entry_bytes = struct.pack(
            entry_fmt,
            base_name.ljust(32, b'\x00'),
            name_str.ljust(32, b'\x00'),
            desc_str.ljust(64, b'\x00'),
            current_offset,
            len(data)
        )
        entries.append(entry_bytes)
        file_blobs.append(data)
        current_offset += len(data)

    header = struct.pack("<4sI", APPFS_MAGIC, file_count)

    with open(output_path, "wb") as f:
        f.write(header)
        for e in entries:
            f.write(e)
        for b in file_blobs:
            f.write(b)

    print(f"[MKAPPFS] Создан образ {output_path}: упаковано приложений: {file_count}, общий размер: {current_offset} байт.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Использование: python3 tools/mkappfs.py <output_appfs.img> <app1.cbx> [app2.cbx ...]")
        sys.exit(1)

    out_img = sys.argv[1]
    input_cbxs = sys.argv[2:]
    create_appfs(out_img, input_cbxs)