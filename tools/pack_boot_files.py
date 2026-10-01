"""Pack user-owned boot files for resident read-only access; no EFI execution."""
import argparse
from pathlib import Path
import struct


def pack(root, output):
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise ValueError('input must be a directory')
    if output.resolve().is_relative_to(root):
        raise ValueError('output must be outside the input directory')
    paths = [root] + sorted(root.rglob('*'), key=lambda p: str(p.relative_to(root)).casefold())
    if len(paths) > 256:
        raise ValueError('archive supports at most 256 files and directories')
    records = []
    contents = []
    seen = set()
    for path in paths:
        if path.is_symlink() or not (path.is_file() or path.is_dir()):
            raise ValueError(f'unsupported entry: {path}')
        name = '\\' if path == root else '\\' + '\\'.join(path.relative_to(root).parts)
        if any(c in name for c in '/:') or any(ord(c) < 32 for c in name):
            raise ValueError(f'unsupported path: {name}')
        # Match the resident comparison: ASCII case folding, literal UTF-16 otherwise.
        identity = ''.join(c.upper() if 'a' <= c <= 'z' else c for c in name)
        if identity in seen:
            raise ValueError(f'duplicate EFI path: {name}')
        seen.add(identity)
        encoded = name.encode('utf-16-le') + b'\0\0'
        if len(encoded) > 512:
            raise ValueError(f'path exceeds 255 UTF-16 code units: {name}')
        records.append((encoded.ljust(512, b'\0'), int(path.is_dir())))
        contents.append(b'' if path.is_dir() else path.read_bytes())
    offset = 24 + len(records)*536
    table = bytearray()
    payload = bytearray()
    for (name, directory), data in zip(records, contents):
        table += name + struct.pack('<QQII', offset, len(data), directory, 0)
        payload += data
        offset += len(data)
    total = (offset + 511) & ~511
    result = b'PWLFILES' + struct.pack('<IIQ', 1, len(records), total) + table + payload
    output.write_bytes(result.ljust(total, b'\0'))
    return len(records), total


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input_directory', type=Path)
    parser.add_argument('output_archive', type=Path)
    args = parser.parse_args()
    count, size = pack(args.input_directory, args.output_archive)
    print(f'Packed {count} files/directories, {size} bytes; read-only resident archive')
