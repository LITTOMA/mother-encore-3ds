import copy
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import locale_grammar as grammar
SOURCE_ROOT = Path(os.environ.get('ENCORE_SOURCE_ROOT', str(ROOT)))

class LocaleGrammarAssetsTests(unittest.TestCase):
    def setUp(self):
        self.ir = grammar.load_ir()
        self.fixture = ROOT / 'build/locale-grammar-tests'
        self.fixture.mkdir(parents=True, exist_ok=True)

    def reject(self, edit):
        ir = copy.deepcopy(self.ir)
        edit(ir)
        with self.assertRaises(ValueError):
            grammar.validate(ir)

    def test_source_semantics_and_reproducible_binary(self):
        grammar.verify_source(self.ir, SOURCE_ROOT)
        first = grammar.encode(self.ir, SOURCE_ROOT)
        reverse = copy.deepcopy(self.ir)
        reverse['profiles'].reverse()
        self.assertEqual(first, grammar.encode(reverse, SOURCE_ROOT))
        self.assertEqual(grammar.HEADER.unpack_from(first),
                         (b'ENCAFX01', 1, len(first), zlib.crc32(first[32:]), 1, 0, 0))
        self.assertEqual(first[32:52].hex(), self.ir['commit'])
        self.assertEqual(struct.unpack_from('<I', first, 52)[0], 2)

    def test_unknown_version_fields_source_and_profile(self):
        for key, value in [('schema', 0), ('schema', 2), ('schema', True),
                           ('kind', 'other'), ('commit', '0' * 40),
                           ('source', '../text_tools.gd'), ('sha256', 'X' * 64),
                           ('unknown', 1), ('profiles', [])]:
            with self.subTest(key=key, value=value):
                self.reject(lambda ir: ir.update({key: value}))
        self.reject(lambda ir: ir['profiles'][0].update(kind='Unknown'))
        self.reject(lambda ir: ir['profiles'][0].update(kind=[]))
        self.reject(lambda ir: ir['profiles'][0].update(unknown=1))
        self.reject(lambda ir: ir['profiles'].append(copy.deepcopy(ir['profiles'][0])))
        self.reject(lambda ir: ir['profiles'][1].update(kind=ir['profiles'][0]['kind']))

    def test_invalid_strings_and_case_mappings(self):
        for key, value in [('matching', ''), ('matching', 'aa'), ('matching', '\0'),
                           ('matching', '['), ('when_match', '\n'),
                           ('otherwise', 'x' * 257), ('lower_pairs', 'A'),
                           ('lower_pairs', 'AaAa'), ('lower_pairs', 'aa'),
                           ('lower_pairs', 'Az'), ('matching', None)]:
            with self.subTest(key=key, value=value):
                self.reject(lambda ir: ir['profiles'][0].update({key: value}))
        self.reject(lambda ir: ir['profiles'][1].update(lower_pairs='Ss'))

    def test_source_disagreement_cannot_compile(self):
        for edit in [lambda ir: ir.update(sha256='0' * 64),
                     lambda ir: ir['profiles'][0].update(matching='aeiou', lower_pairs='AaEeIiOoUu'),
                     lambda ir: ir['profiles'][0].update(when_match='!'),
                     lambda ir: ir['profiles'][1].update(matching='sxS')]:
            ir = copy.deepcopy(self.ir)
            edit(ir)
            with self.assertRaises(ValueError):
                grammar.encode(ir, SOURCE_ROOT)
        with tempfile.TemporaryDirectory(dir=self.fixture) as temporary:
            root = Path(temporary)
            for path in ('upstream.lock', 'compatibility/upstream-inventory.json',
                         'upstream/MOTHER-Encore/' + self.ir['source']):
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((SOURCE_ROOT / path).read_bytes())
            target.write_bytes(target.read_bytes() + b'\n#changed\n')
            with self.assertRaisesRegex(ValueError, 'fingerprint'):
                grammar.encode(self.ir, root)

    def test_duplicate_ir_json_is_rejected(self):
        with tempfile.TemporaryDirectory(dir=self.fixture) as temporary:
            path = Path(temporary) / 'duplicate.json'
            path.write_text('{"schema":1,"schema":1}', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Duplicate'):
                grammar.load_ir(path)

    def test_actual_localized_consumer_has_no_embedded_affix_data(self):
        source = (ROOT / 'runtime/localized_presentation.cpp').read_text(encoding='utf-8')
        self.assertIn('->affix(', source)
        for value in ('"aeiou"', '"e "', "name.back()=='s'", "name.back()=='x'"):
            self.assertNotIn(value, source)

if __name__ == '__main__':
    unittest.main()
