#!/usr/bin/env python3
"""Gera sce_sys/param.sfo para o GTSTORE (PS4)."""
import argparse
import os
import re

DEFAULT_CONTENT_ID = "UP0000-GTST00001_00-GTSTOREAPP000000"
FMT_STR = 0x0204  # string UTF-8 terminada em nulo
FMT_INT = 0x0404  # inteiro de 32 bits


def to_u32(val):
    return val.to_bytes(4, "little")


def to_u16(val):
    return val.to_bytes(2, "little")


def text(key, value, max_len):
    return (key, FMT_STR, max_len, value.encode("utf-8") + b"\x00")


def integer(key, value):
    return (key, FMT_INT, 4, to_u32(value))


def build_entries(content_id):
    if not re.fullmatch(r"[A-Z]{2}\d{4}-[A-Z]{4}\d{5}_\d{2}-[A-Z0-9]{16}", content_id):
        raise SystemExit(f"CONTENT_ID invalido: {content_id!r}")

    # CONTENT_ID = XXNNNN-TITLEID99_00-LABEL..., o TITLE_ID sao os 9 caracteres do meio.
    title_id = content_id[7:16]

    return [
        integer("APP_TYPE", 1),
        text("APP_VER", "01.00", 8),
        integer("ATTRIBUTE", 0),
        text("CATEGORY", "gd", 4),
        text("CONTENT_ID", content_id, 48),
        integer("DOWNLOAD_DATA_SIZE", 0),
        text("FORMAT", "obs", 4),
        text("NP_TITLE_ID", title_id, 12),
        integer("PARENTAL_LEVEL", 1),
        integer("SYSTEM_VER", 0),
        text("TITLE", "GTSTORE", 128),
        text("TITLE_ID", title_id, 12),
        text("VERSION", "01.00", 8),
    ]


def write_sfo(content_id, output):
    entries = sorted(build_entries(content_id), key=lambda e: e[0])

    count = len(entries)
    key_table_offset = 20 + count * 16  # header (20) + indice (16 por entrada)

    keys_data = bytearray()
    vals_data = bytearray()
    index_entries = []

    for key, fmt, max_len, val_bytes in entries:
        if len(val_bytes) > max_len:
            raise SystemExit(
                f"{key}: valor com {len(val_bytes)} bytes excede max_len ({max_len})"
            )

        key_off = len(keys_data)
        keys_data.extend(key.encode("ascii") + b"\x00")

        val_off = len(vals_data)
        vals_data.extend(val_bytes + b"\x00" * (max_len - len(val_bytes)))

        index_entries.append((key_off, fmt, len(val_bytes), max_len, val_off))

    while len(keys_data) % 4 != 0:
        keys_data.append(0)

    data_table_offset = key_table_offset + len(keys_data)

    out_dir = os.path.dirname(output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(output, "wb") as f:
        f.write(b"\x00PSF")
        f.write(b"\x01\x01\x00\x00")
        f.write(to_u32(key_table_offset))
        f.write(to_u32(data_table_offset))
        f.write(to_u32(count))

        for key_off, fmt, data_len, max_len, val_off in index_entries:
            f.write(to_u16(key_off))
            f.write(to_u16(fmt))
            f.write(to_u32(data_len))
            f.write(to_u32(max_len))
            f.write(to_u32(val_off))

        f.write(keys_data)
        f.write(vals_data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Gera o param.sfo do GTSTORE.")
    parser.add_argument("--content-id", default=DEFAULT_CONTENT_ID)
    parser.add_argument("--output", default="sce_sys/param.sfo")
    args = parser.parse_args()
    write_sfo(args.content_id, args.output)
