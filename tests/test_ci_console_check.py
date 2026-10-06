import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from ci_console_check import embedded_romfs, embedded_files


class ConsoleHeaderTests(unittest.TestCase):
    def filesystem(self):
        sentinel = 0xffffffff
        raw = bytearray(111)
        struct.pack_into('<10I', raw, 0, 40, 40, 4, 44, 24, 68, 4, 72, 36, 108)
        struct.pack_into('<6I', raw, 44, 0, sentinel, sentinel, 0, sentinel, 0)
        struct.pack_into('<IIQQII', raw, 72, 0, sentinel, 0, 3, sentinel, 2)
        raw[104:106] = 'a'.encode('utf-16le')
        raw[108:111] = b'xyz'
        blob = self.fixture()[:64]
        blob.extend(raw)
        return blob

    def test_named_files(self):
        files = embedded_files(self.filesystem())
        self.assertEqual(set(files), {'a'})
        self.assertEqual(bytes(files['a']), b'xyz')

    def test_filesystem_rejects_bad_references_and_bounds(self):
        for offset, fmt, value in ((0, 'I', 39), (36, 'I', 9999),
                                   (44 + 12, 'I', 1), (72, 'I', 1),
                                   (72 + 4, 'I', 0), (72 + 8, 'Q', 9999),
                                   (72 + 16, 'Q', 9999), (72 + 28, 'I', 3)):
            blob = self.filesystem()
            struct.pack_into('<' + fmt, blob, 64 + offset, value)
            with self.subTest(offset=offset):
                with self.assertRaises(ValueError):
                    embedded_files(blob)
        blob = self.filesystem()
        blob[64 + 104:64 + 106] = '/'.encode('utf-16le')
        with self.assertRaises(ValueError):
            embedded_files(blob)

    def fixture(self):
        blob = bytearray(80)
        blob[:4] = b'3DSX'
        struct.pack_into('<HHII', blob, 4, 44, 8, 0, 0)
        struct.pack_into('<III', blob, 32, 44, 16, 64)
        blob[44:48] = b'SMDH'
        return blob

    def test_valid(self):
        blob = self.fixture()
        self.assertEqual(embedded_romfs(blob), blob[64:])

    def test_rejected(self):
        bad = [b'', self.fixture()[:43]]
        for offset, fmt, value in ((0, '4s', b'NOPE'), (4, 'H', 32),
                                   (6, 'H', 4), (8, 'I', 1), (12, 'I', 1),
                                   (32, 'I', 40), (36, 'I', 0),
                                   (36, 'I', 40), (40, 'I', 80),
                                   (44, '4s', b'NOPE')):
            blob = self.fixture()
            struct.pack_into('<' + fmt, blob, offset, value)
            bad.append(blob)
        for blob in bad:
            with self.subTest(blob=bytes(blob[:48])):
                with self.assertRaises(ValueError):
                    embedded_romfs(blob)


if __name__ == '__main__':
    unittest.main()
