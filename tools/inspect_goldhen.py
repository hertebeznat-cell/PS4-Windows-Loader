#!/usr/bin/env python3
"""Offline extraction for one reviewed GoldHEN input; never execute payload code."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

OUTER_SHA = 'df3f27c1b35bc7c40e3a08caab948930914dc7d0301a73b68945cf6ffe40ea12'
INNER_SHA = '4e2864fb568707470bbc0dcf28ade7ed2096c3ea42ea87dd5ff13613db91abee'

def inspect(data):
    if hashlib.sha256(data).hexdigest() != OUTER_SHA:
        raise ValueError('Unreviewed binary: fixed offsets are not applicable')
    size = struct.unpack_from('<I', data, 0x47294)[0]
    compressed = data[0x6900:0x6900 + size]
    if len(compressed) != size:
        raise ValueError('Truncated compressed region')
    decoder = zlib.decompressobj()
    inner = decoder.decompress(compressed, 1048577)
    if len(inner) > 1048576 or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError('Invalid or oversized zlib stream')
    digest = hashlib.sha256(inner).hexdigest()
    if digest != INNER_SHA or len(inner) != 636760:
        raise ValueError('Unexpected internal image')
    return inner, {'outer_sha256': OUTER_SHA, 'inner_sha256': digest,
                   'compressed_file_offset': '0x6900', 'compressed_size': size,
                   'inner_size': len(inner), 'binding_verified': False}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--extract-to', type=Path)
    args = parser.parse_args()
    try:
        if args.binary.stat().st_size != 293120:
            raise ValueError('Unexpected binary size')
        inner, report = inspect(args.binary.read_bytes())
        if args.extract_to:
            with args.extract_to.open('xb') as output:
                output.write(inner)
        print(json.dumps(report, indent=2))
    except (OSError, ValueError, zlib.error) as error:
        parser.exit(1, str(error) + '\n')

if __name__ == '__main__':
    main()
