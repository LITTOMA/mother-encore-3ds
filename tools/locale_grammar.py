"""Reviewed TextTools affix data; no general translation VM or extraction side effect."""
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
IR = ROOT / 'content/native-locale-grammar.json'
KINDS = {'Elision': 1, 'Genitive': 2}
HEADER = struct.Struct('<8s6I')
PIN = '7d9246600fffe518408f5830d4848635019005a3'

def require(value, message):
    if not value:
        raise ValueError(message)

def load_ir(path=IR):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate grammar JSON field')
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)

def fields(value, expected):
    require(type(value) is dict and set(value) == set(expected), 'Unknown/missing grammar fields')

def validate(ir):
    fields(ir, ('schema', 'kind', 'commit', 'source', 'sha256', 'profiles'))
    require(type(ir['schema']) is int and ir['schema'] == 1 and ir['kind'] == 'encore.locale-affix.source-ir', 'Unsupported grammar schema/kind')
    require(ir['commit'] == PIN, 'Grammar source pin mismatch')
    require(ir['source'] == 'Scripts/global/text_tools.gd', 'Unreviewed grammar source')
    require(type(ir['sha256']) is str and re.fullmatch('[0-9a-f]{64}', ir['sha256']), 'Invalid grammar source digest')
    require(type(ir['profiles']) is list and len(ir['profiles']) == 2, 'Incomplete affix profiles')
    seen = set()
    for row in ir['profiles']:
        fields(row, ('kind', 'matching', 'lower_pairs', 'when_match', 'otherwise'))
        require(type(row['kind']) is str and row['kind'] in KINDS and row['kind'] not in seen, 'Unknown/duplicate affix profile')
        seen.add(row['kind'])
        for key in ('matching', 'lower_pairs', 'when_match', 'otherwise'):
            value = row[key]
            require(type(value) is str and len(value.encode('utf-8')) <= 256 and all(ord(c) >= 32 and c not in '[]' and not 0xd800 <= ord(c) <= 0xdfff for c in value), 'Malformed grammar string')
        matches, pairs = row['matching'], row['lower_pairs']
        require(matches and len(set(matches)) == len(matches), 'Empty/duplicate matching codepoint')
        require(len(pairs) % 2 == 0, 'Incomplete case pair')
        uppers = pairs[::2]
        require(len(set(uppers)) == len(uppers) and all(a != b and a not in matches and b in matches for a, b in zip(uppers, pairs[1::2])), 'Invalid/duplicate case pair')
        require(row['kind'] != 'Genitive' or not pairs, 'Case-sensitive genitive does not accept case mapping')
    return ir

def verify_source(ir, source_root=ROOT):
    validate(ir)
    source_root = Path(source_root)
    lock = json.loads((source_root / 'upstream.lock').read_text())
    inventory = json.loads((source_root / 'compatibility/upstream-inventory.json').read_text())
    require(lock['commit'] == inventory['commit'] == ir['commit'], 'Grammar source identity mismatch')
    source = (source_root / 'upstream/MOTHER-Encore' / ir['source']).read_bytes()
    require(hashlib.sha256(source).hexdigest() == ir['sha256'] == inventory['files'][ir['source']]['sha256'], 'Changed grammar source fingerprint')
    text = source.decode('utf-8')
    french = re.search(r'static func _get_french_elision\(.*?(?=\nstatic func)', text, re.S)[0]
    german = re.search(r'static func _get_german_genitive\(.*?(?=\nstatic func)', text, re.S)[0]
    vowels = re.search(r'var vowels := "([^"]+)"', french)[1]
    outputs = re.search(r'return "([^"]*)" if next_word and next_word\[0\]\.to_lower\(\) in vowels else "([^"]*)"', french)
    endings = re.search(r'return "%s([^"\n]*)" % name if name\.ends_with\(\'([^\']*)\'\) or name\.ends_with\(\'([^\']*)\'\) else "%s([^"\n]*)" % name', german)
    require(outputs and endings, 'Unreviewed TextTools affix execution')
    # This finite Unicode mapping covers uppercase partners of the exact
    # reviewed vowel set. It is serialized content, not a Unicode/locale VM.
    lower_pairs = ''.join(c.upper() + c for c in vowels)
    expected = [dict(kind='Elision', matching=vowels, lower_pairs=lower_pairs, when_match=outputs[1], otherwise=outputs[2]),
                dict(kind='Genitive', matching=endings[2] + endings[3], lower_pairs='', when_match=endings[1], otherwise=endings[4])]
    require(sorted(ir['profiles'], key=lambda x: x['kind']) == sorted(expected, key=lambda x: x['kind']), 'Affix IR differs from reviewed source behavior')

def encode(ir, source_root=ROOT):
    verify_source(ir, source_root)
    raw = bytearray(bytes.fromhex(ir['commit']) + struct.pack('<I', len(ir['profiles'])))
    for row in sorted(ir['profiles'], key=lambda x: KINDS[x['kind']]):
        raw += struct.pack('<2I', KINDS[row['kind']], 0)
        for key in ('matching', 'lower_pairs', 'when_match', 'otherwise'):
            value = row[key].encode('utf-8')
            raw += struct.pack('<I', len(value)) + value
    require(len(raw) + HEADER.size <= 2048, 'Affix block too large')
    return HEADER.pack(b'ENCAFX01', 1, len(raw) + HEADER.size, zlib.crc32(raw), 1, 0, 0) + raw
