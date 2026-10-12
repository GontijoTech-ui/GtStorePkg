import sys
import struct
import argparse

def create_sfo(content_id, output_path):
    # Extrai o TITLE_ID a partir do CONTENT_ID (ex: UP0000-GTST00001_00... -> GTST00001)
    title_id = content_id[7:16]
    
    # Lista de chaves obrigatórias para a PS4 aceitar a app
    keys = [
        b"APP_TYPE", b"APP_VER", b"ATTRIBUTE", b"CATEGORY", 
        b"CONTENT_ID", b"DOWNLOAD_DATA_SIZE", b"FORMAT", 
        b"NP_COMMUNICATION_ID", b"PUBTOOLINFO", b"PUBTOOLVER", 
        b"SYSTEM_VER", b"TITLE", b"TITLE_ID", b"VERSION"
    ]
    
    # Dicionário com os valores (formato, tamanho_maximo, valor)
    # 0x0404 = Número Inteiro | 0x0204 = Texto (String)
    vals = {
        b"APP_TYPE": (0x0404, 4, 1),
        b"APP_VER": (0x0204, 8, b"01.00"),
        b"ATTRIBUTE": (0x0404, 4, 0),
        b"CATEGORY": (0x0204, 4, b"gd"),
        b"CONTENT_ID": (0x0204, 48, content_id.encode('utf-8')),
        b"DOWNLOAD_DATA_SIZE": (0x0404, 4, 0),
        b"FORMAT": (0x0204, 4, b"obs"),
        b"NP_COMMUNICATION_ID": (0x0204, 16, title_id.encode('utf-8')),
        b"PUBTOOLINFO": (0x0204, 256, b"c_date=20261012,c_time=000000,st_type=app"),
        b"PUBTOOLVER": (0x0404, 4, 0x00010000),
        b"SYSTEM_VER": (0x0404, 4, 0x05050000),
        b"TITLE": (0x0204, 128, b"GTSTORE"),
        b"TITLE_ID": (0x0204, 16, title_id.encode('utf-8')),
        b"VERSION": (0x0204, 8, b"01.00")
    }

    keys.sort()
    
    key_table = b""
    key_offsets = []
    for k in keys:
        key_offsets.append(len(key_table))
        key_table += k + b"\x00"
        
    while len(key_table) % 4 != 0:
        key_table += b"\x00"
        
    data_table = b""
    data_offsets = []
    for k in keys:
        fmt, max_len, v = vals[k]
        pad_len = 0
        if fmt == 0x0404:
            v_bytes = struct.pack("<I", v)
            max_len = 4
            real_len = 4
        else:
            v_bytes = v + b"\x00"
            real_len = len(v_bytes)
            pad_len = max_len - real_len
            
        data_offsets.append(len(data_table))
        data_table += v_bytes + (b"\x00" * pad_len)
        
    # Cabeçalho do SFO
    magic = b"\x00PSF"
    version = b"\x01\x01\x00\x00"
    
    key_ofs = 20 + len(keys) * 16
    data_ofs = key_ofs + len(key_table)
    
    header = magic + version + struct.pack("<III", key_ofs, data_ofs, len(keys))
    
    # Tabela de Índices
    idx_table = b""
    for i, k in enumerate(keys):
        fmt, max_len, v = vals[k]
        if fmt == 0x0404:
            real_len = 4
        else:
            real_len = len(v) + 1
        idx_table += struct.pack("<HHIII", key_offsets[i], fmt, real_len, max_len, data_offsets[i])
        
    # Salva o arquivo final
    with open(output_path, "wb") as f:
        f.write(header)
        f.write(idx_table)
        f.write(key_table)
        f.write(data_table)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--content-id", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    create_sfo(args.content_id, args.output)
