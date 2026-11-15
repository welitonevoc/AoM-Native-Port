#!/usr/bin/env python3
"""
Extrator de arquivos .bar do Age of Mythology
Extrai todos os arquivos .prt dos arquivos .bar originais.

Uso:
    python3 extract_aom.py <caminho-do-aom> -o <pasta-de-saida>

Exemplo:
    python3 extract_aom.py "/sdcard/Documents/aom" -o ./aom_prt_extracted
"""

import os
import sys
import re
import argparse


def extract_prt_from_bar(bar_path, output_dir):
    """Extrai todos os arquivos .prt de um arquivo .bar"""
    if not os.path.exists(bar_path):
        print(f"[ERRO] Arquivo não encontrado: {bar_path}")
        return 0

    os.makedirs(output_dir, exist_ok=True)

    with open(bar_path, 'rb') as f:
        data = f.read()

    # Encontrar todas as strings que terminam com .prt
    prt_pattern = rb'([a-zA-Z0-9_ \.\/\\-]+\.prt)'
    matches = list(re.finditer(prt_pattern, data))

    extracted = 0
    seen_names = set()

    for i, match in enumerate(matches):
        prt_name = match.group(1).decode('ascii', errors='replace')

        # Pular duplicatas
        if prt_name in seen_names:
            continue
        seen_names.add(prt_name)

        # O offset do nome no arquivo
        name_offset = match.start(1)

        # Tentar determinar o tamanho do arquivo lendo à frente
        data_start = match.end()

        # Procurar o próximo nome de arquivo .prt para estimar o fim deste
        next_match = matches[i + 1] if i + 1 < len(matches) else None
        if next_match:
            data_end = next_match.start() - 1
        else:
            data_end = len(data)

        # O tamanho real dos dados é entre o fim do nome e o próximo nome
        prt_data = data[data_start:data_end]

        # Limpar: remover bytes nulos do início e fim
        prt_data = prt_data.strip(b'\x00')

        if len(prt_data) < 10:
            continue

        # Caminho de saída
        safe_name = prt_name.replace('/', '_').replace('\\', '_').replace(':', '_')
        filepath = os.path.join(output_dir, safe_name)

        try:
            with open(filepath, 'wb') as out:
                out.write(prt_data)
            extracted += 1
            print(f"  [OK] {prt_name} ({len(prt_data)} bytes)")
        except Exception as e:
            print(f"  [ERRO] {prt_name}: {e}")

    print(f"[RESUMO] {extracted} arquivos .prt extraídos de {bar_path}")
    return extracted


def extract_all_prt(aom_dir, output_dir):
    """Extrai todos os .prt de todos os .bar na pasta do AoM"""
    bar_files = []

    for root, dirs, files in os.walk(aom_dir):
        for f in files:
            if f.lower().endswith('.bar'):
                bar_files.append(os.path.join(root, f))

    print(f"[INFO] Encontrados {len(bar_files)} arquivos .bar")
    print()

    total = 0
    for bar_path in sorted(bar_files):
        print(f"Processando: {os.path.basename(bar_path)}")
        count = extract_prt_from_bar(bar_path, output_dir)
        total += count
        print()

    print(f"[TOTAL] {total} arquivos .prt extraídos para: {output_dir}")
    return total


def main():
    parser = argparse.ArgumentParser(description='Extrator de .bar do Age of Mythology')
    parser.add_argument('aom_dir', help='Pasta raiz do Age of Mythology')
    parser.add_argument('-o', '--output', default='./aom_prt_extracted', help='Pasta de saída')
    parser.add_argument('--single-bar', help='Extrair de um único arquivo .bar')

    args = parser.parse_args()

    if args.single_bar:
        if not os.path.exists(args.single_bar):
            print(f"[ERRO] Arquivo não encontrado: {args.single_bar}")
            sys.exit(1)
        extract_prt_from_bar(args.single_bar, args.output)
    else:
        if not os.path.isdir(args.aom_dir):
            print(f"[ERRO] Diretório não encontrado: {args.aom_dir}")
            sys.exit(1)
        extract_all_prt(args.aom_dir, args.output)


if __name__ == '__main__':
    main()