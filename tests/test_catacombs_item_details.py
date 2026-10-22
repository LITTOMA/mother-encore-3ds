"""Versioned literal-percent and historical description resource checks."""
import copy,hashlib,json,struct,unittest,zlib
from pathlib import Path
from tools import item_details as details

ROOT=Path(__file__).resolve().parents[1]
LEGACY=ROOT/'tests/fixtures/item-admission-legacy'


class ExtendedItemDetails(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir=json.loads((LEGACY/'native-item-details.json').read_text(encoding='utf-8'))
        cls.receipt=json.loads((LEGACY/'details-texture-receipt.json').read_text(encoding='utf-8'))
        cls.tables=details.lower(cls.ir,cls.receipt)

    def test_historical_v1_exact_bytes(self):
        source=json.loads((LEGACY/'source.json').read_text())
        blob=details.encode(self.tables,version=1)
        self.assertEqual(hashlib.sha256(blob).hexdigest(),source['files']['romfs/ui/opening.encdetails']['sha256'])
        self.assertEqual(details.resource_version(self.tables),1)
        self.assertEqual(details.parse_pack(blob)['Resources'][0][0],1)

    def test_source_numeric_percentage_and_poison_selector(self):
        for raw in ('99% Beef.','含量99%的牛肉。'):
            tokens=details.tokenize(raw)
            self.assertEqual([t['kind'] for t in tokens],['Text','LiteralPercent','Text'])
            self.assertEqual(tokens[1]['value'],0)
        self.assertEqual(details.tokenize('[Poisoned]'),[dict(kind='InlineImage',value=1,color=0)])
        self.assertEqual(details.tokenize('[Asthma]'),[dict(kind='InlineImage',value=0,color=0)])

    def test_unknown_percent_and_status_controls(self):
        for raw in ('%q','99%q','99%%','99%99','[poisoned]','[Poison]','%','% Beef.'):
            with self.subTest(raw=raw),self.assertRaises(ValueError):details.tokenize(raw)

    def extended(self):
        tables=copy.deepcopy(self.tables)
        tables['Tokens'][0]=[7,0,0,0]
        return tables

    def test_new_opcode_selects_v2_and_v1_rejects(self):
        tables=self.extended();blob=details.encode(tables)
        self.assertEqual(struct.unpack_from('<I',blob,8)[0],2)
        self.assertEqual(details.parse_pack(blob)['Tokens'][0],(7,0,0,0))
        with self.assertRaises(ValueError):details.encode(tables,version=1)

    def test_new_opcode_operand_and_version_domains(self):
        for column,value in ((0,8),(1,1),(2,2),(3,1)):
            tables=self.extended();tables['Tokens'][0][column]=value
            with self.subTest(column=column),self.assertRaises(ValueError):details.encode(tables)
        for version in (0,3,True):
            with self.subTest(version=version),self.assertRaises(ValueError):details.encode(self.tables,version=version)

    def test_v2_header_cannot_claim_legacy_or_unknown_capability(self):
        blob=details.encode(self.extended())
        for version,capability in ((1,1),(2,1),(2,3),(3,3)):
            changed=bytearray(blob);struct.pack_into('<I',changed,8,version);struct.pack_into('<I',changed,24,capability)
            struct.pack_into('<I',changed,16,0);struct.pack_into('<I',changed,16,zlib.crc32(changed))
            with self.subTest(version=version,capability=capability),self.assertRaises(ValueError):details.parse_pack(changed)

    def test_multidose_source_format_cannot_contain_literal_percent(self):
        tables=copy.deepcopy(self.tables)
        row=next(p for p in tables['Presentations'] if p[0]==1)
        tables['Tokens'][row[2]]=[7,0,0,0]
        with self.assertRaises(ValueError):details.encode(tables)

    def test_text_literals_still_reject_unparsed_control_characters(self):
        tables=copy.deepcopy(self.tables);tables['Tokens'][0][1]=len(tables['Strings'])
        tables['Strings']+=b'99% Beef.\0'
        with self.assertRaises(ValueError):details.encode(tables,version=2)


if __name__=='__main__':unittest.main()
