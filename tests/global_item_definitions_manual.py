"""Manual-only malformed cap3 fixtures. Never invoked by automatic checks."""
import struct
import zlib
import argparse
import subprocess
import tempfile
from pathlib import Path


def malformed_fixtures(raw):
    """The caller submits these to the real typed loader, expecting rejection."""
    at = 64 + 36 + 24

    def number():
        nonlocal at
        value = struct.unpack_from('<I', raw, at)[0]
        at += 4
        return value

    def text():
        nonlocal at
        size = number()
        at += size

    number(); number()  # source GodStorage ID/type
    for _ in range(3): text()
    for _ in range(number()): text()  # checked constructor paths
    first_action = first_metadata = None
    for _ in range(struct.unpack_from('<I', raw, 28)[0]):
        at += 64
        for _ in range(8): text()
        for _ in range(3):
            for _ in range(number()): text()
        for _ in range(number()):
            if first_action is None: first_action = at
            at += 8; text(); text()
        at += 4  # literal source sorting score
        for _ in range(number()): text(); text()
        for _ in range(number()):
            if first_metadata is None: first_metadata = at
            at += 8; text(); text(); at += 12
    if first_action is None or first_metadata is None:
        raise ValueError("Actual resource lacks reviewed action/metadata negative targets")
    cases = {}
    for label, offset, value in [
        ('unknown-action-opcode', first_action, 99),
        ('unknown-metadata-field', first_metadata, 99),
        ('unknown-metadata-kind', first_metadata + 4, 99),
        ('unknown-format', 8, 99),
        ('unknown-capability', 20, 99),
        ('unknown-rules', 24, 99),
    ]:
        candidate = bytearray(raw)
        struct.pack_into('<I', candidate, offset, value)
        struct.pack_into('<I', candidate, 16, 0)
        struct.pack_into('<I', candidate, 16, zlib.crc32(candidate) & 0xffffffff)
        cases[label] = bytes(candidate)
    cases['truncated'] = raw[:-1]
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--loader', required=True, type=Path)
    parser.add_argument('--global-pack', required=True, type=Path)
    parser.add_argument('--scoped-pack', required=True, type=Path)
    parser.add_argument('--work', required=True, type=Path)
    args = parser.parse_args()
    loader = args.loader.resolve(strict=True)
    full = args.global_pack.resolve(strict=True)
    scoped = args.scoped_pack.resolve(strict=True)
    work = args.work.resolve()
    if work == full.parent or work == scoped.parent:
        parser.error('Manual malformed fixtures must use a separate build directory')
    work.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(loader), str(full), str(scoped)], check=True)
    # Every CRC-correct negative reaches the exact same production C++ loader;
    # a Python format validator is not substituted for the runtime consumer.
    with tempfile.TemporaryDirectory(prefix='global-items-manual-', dir=work) as directory:
        destination = Path(directory).resolve()
        if not destination.is_relative_to(work):
            raise ValueError('Manual fixture directory escaped requested work root')
        for label, binary in malformed_fixtures(full.read_bytes()).items():
            path = destination / (label + '.encfielditems')
            path.write_bytes(binary)
            subprocess.run([str(loader), '--expect-reject', str(path)], check=True)


if __name__ == '__main__':
    main()
