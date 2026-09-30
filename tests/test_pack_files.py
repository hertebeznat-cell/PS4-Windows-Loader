import importlib.util
from pathlib import Path
import struct
import tempfile

spec = importlib.util.spec_from_file_location('pack_boot_files', Path(__file__).resolve().parents[1]/'tools/pack_boot_files.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as directory:
    base = Path(directory)
    source = base/'source'
    boot = source/'EFI'/'Microsoft'/'Boot'
    boot.mkdir(parents=True)
    (boot/'bootmgfw.efi').write_bytes(b'EFI fixture')
    (boot/'BCD').write_bytes(b'BCD fixture')
    count, size = module.pack(source, base/'files.bin')
    data = (base/'files.bin').read_bytes()
    assert count == 6 and size == len(data) and size % 512 == 0
    assert data[:8] == b'PWLFILES' and struct.unpack_from('<IIQ', data, 8) == (1, count, size)
    records = []
    for i in range(count):
        record = 24 + 536*i
        name = data[record:record+512].decode('utf-16-le').split('\0')[0]
        offset, length, flags, reserved = struct.unpack_from('<QQII', data, record+512)
        assert offset >= 24+count*536 and offset+length <= size and reserved == 0
        records.append((name, data[offset:offset+length], flags))
    assert ('\\EFI\\Microsoft\\Boot\\bootmgfw.efi', b'EFI fixture', 0) in records
    module.pack(source, base/'files2.bin')
    assert data == (base/'files2.bin').read_bytes()
    (boot/'bcd').write_bytes(b'duplicate')
    try:
        module.pack(source, base/'bad.bin')
        raise AssertionError('case duplicate accepted')
    except ValueError:
        pass
    (boot/'bcd').unlink()
    (boot/'link').symlink_to(base/'files.bin')
    try:
        module.pack(source, base/'bad.bin')
        raise AssertionError('symlink accepted')
    except ValueError:
        pass
print('file packer: deterministic hierarchy, contents and invalid input refusal passed')
