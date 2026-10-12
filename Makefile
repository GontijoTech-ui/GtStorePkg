import sys
import struct
import urllib.request
import argparse

def patch_sfo(content_id, title, output_file):
    url = "https://raw.githubusercontent.com/PacBrew/ps4-openorbis-sample/master/sce_sys/param.sfo"
    print(f"Baixando SFO base oficial de {url}...")
    
    # Baixa o SFO original do template
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req) as response:
        data = bytearray(response.read())

    magic = data[0:4]
    if magic != b"\x00PSF":
        print("Erro: Arquivo SFO base invalido.")
        sys.exit(1)

    key_ofs, data_ofs, count = struct.unpack("<III", data[8:20])
    new_title_id = content_id[7:16].encode('utf-8')
    new_content_id = content_id.encode('utf-8')
    new_title = title.encode('utf-8')

    # Faz a cirurgia byte a byte para injetar os seus dados no arquivo oficial
    for i in range(count):
        entry_ofs = 20 + (i * 16)
        k_ofs, fmt, length, max_len, d_ofs = struct.unpack("<HHIII", data[entry_ofs:entry_ofs+16])
        
        k_start = key_ofs + k_ofs
        k_end = data.find(b"\x00", k_start)
        key_name = data[k_start:k_end]
        d_start = data_ofs + d_ofs

        if key_name == b"CONTENT_ID":
            data[d_start:d_start+36] = new_content_id
        elif key_name in (b"TITLE_ID", b"NP_COMMUNICATION_ID"):
            data[d_start:d_start+9] = new_title_id
        elif key_name == b"TITLE":
            if len(new_title) >= max_len:
                new_title = new_title[:max_len-1]
            padded_title = new_title.ljust(max_len, b"\x00")
            data[d_start:d_start+max_len] = padded_title
            # Atualiza o tamanho real da string na tabela de índices
            struct.pack_into("<I", data, entry_ofs + 4, len(new_title) + 1)
            
    with open(output_file, "wb") as f:
        f.write(data)
    print("param.sfo injetado e gerado com sucesso!")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--content-id", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    patch_sfo(args.content_id, "GTSTORE", args.output)
