import os

def to_u32(val):
    return val.to_bytes(4, 'little')

def to_u16(val):
    return val.to_bytes(2, 'little')

def write_sfo():
    magic = b'\x00PSF'
    version = b'\x01\x01\x00\x00'

    # A propriedade FORMAT foi removida. Apenas as chaves obrigatórias para executáveis estão aqui.
    entries = [
        (b"APP_TYPE\x00", 0x0404, 4, to_u32(1)),
        (b"APP_VER\x00",  0x0204, 8, b'01.00\x00'),
        (b"CATEGORY\x00", 0x0204, 4, b'gd\x00'),
        (b"CONTENT_ID\x00", 0x0204, 48, b'UP0000-CUSA99999_00-GTSTOREAPP000000\x00'),
        (b"DOWNLOAD_DATA_SIZE\x00", 0x0404, 4, to_u32(0)),
        (b"NP_TITLE_ID\x00", 0x0204, 16, b'CUSA99999\x00'),
        (b"SYSTEM_VER\x00", 0x0404, 4, to_u32(0)),
        (b"TITLE\x00",    0x0204, 64, b'GTSTORE\x00'),
        (b"TITLE_ID\x00", 0x0204, 16, b'CUSA99999\x00'),
        (b"VERSION\x00",  0x0204, 8, b'01.00\x00')
    ]

    entries.sort(key=lambda x: x[0])

    count = len(entries)
    header_size = 20
    index_table_size = count * 16
    key_table_offset = header_size + index_table_size

    keys_data = bytearray()
    vals_data = bytearray()
    index_entries = []

    current_key_off = 0
    current_val_off = 0

    for key, fmt, max_len, val_bytes in entries:
        key_off = current_key_off
        current_key_off += len(key)
        keys_data.extend(key)

        padded_val = val_bytes + b'\x00' * (max_len - len(val_bytes))
        val_off = current_val_off
        current_val_off += len(padded_val)
        vals_data.extend(padded_val)

        data_len = len(val_bytes)
        index_entries.append((key_off, fmt, data_len, max_len, val_off))

    while len(keys_data) % 4 != 0:
        keys_data.append(0)

    data_table_offset = key_table_offset + len(keys_data)

    os.makedirs('sce_sys', exist_ok=True)
    
    with open('sce_sys/param.sfo', 'wb') as f:
        f.write(magic)
        f.write(version)
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

if __name__ == '__main__':
    write_sfo()
