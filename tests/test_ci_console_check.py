import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from ci_console_check import embedded_romfs


class ConsoleHeaderTests(unittest.TestCase):
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
