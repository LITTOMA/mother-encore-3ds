import copy,hashlib,json,struct,unittest,zlib
from pathlib import Path
from tools import native_storage as storage,storage_assets,catacombs_item_catalog as catalog,inventory_sort_reference as reference
from tools.extract_battle_entry import Extractor

ROOT=Path(__file__).resolve().parents[1]
LEGACY=ROOT/'tests/fixtures/item-admission-legacy'


class CatacombsStorageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original,cls.inputs=reference.read(ROOT/'reports/inventory-sort/original-1')
        cls.items=catalog.append_item(Extractor(ROOT),catalog.read(LEGACY/'native-items.json'))
        cls.ir=storage_assets.build(ROOT,cls.items)
        cls.receipt=catalog.read(LEGACY/'storage-texture-receipt.json')
        cls.tables=storage.lower(cls.ir,cls.receipt)

    def test_all_original_bilingual_comparisons_match_compiled_ranks(self):
        ranks={row['source_item']:row for row in self.ir['policies']}
        for row in self.original['pairs']:
            key='sort_rank_en'if row['locale']=='en'else'sort_rank_zh'
            self.assertEqual(ranks[row['a']][key]>ranks[row['b']][key],row['less'])
        self.assertEqual([row['max_count']for row in self.ir['policies']],[1,1,1,1,4,1,2])

    def test_actual_repeated_original_sorts_and_old_bytes(self):
        self.assertTrue(all(row['once']==row['twice']for row in self.original['rows']))
        other,_=reference.read(ROOT/'reports/inventory-sort/original-2')
        self.assertEqual(self.original,other)
        old=catalog.read(LEGACY/'native-storage.json');blob=storage.encode(storage.lower(old,self.receipt),version=1)
        proof=catalog.read(LEGACY/'source.json')
        self.assertEqual(hashlib.sha256(blob).hexdigest(),proof['files']['romfs/ui/opening.encstorage']['sha256'])

    def test_v2_roundtrip_and_legacy_quantity_guard(self):
        blob=storage.encode(self.tables);self.assertEqual(struct.unpack_from('<I',blob,8)[0],2)
        self.assertEqual(storage.encode(storage.parse_pack(blob)),blob)
        with self.assertRaises(ValueError):storage.encode(self.tables,version=1)
        for version in (0,3,True):
            with self.subTest(version=version),self.assertRaises(ValueError):storage.encode(self.tables,version=version)
        for version,capability in ((1,1),(2,1),(2,3),(3,3)):
            changed=bytearray(blob);struct.pack_into('<I',changed,8,version);struct.pack_into('<I',changed,20,capability)
            struct.pack_into('<I',changed,16,0);struct.pack_into('<I',changed,16,zlib.crc32(changed))
            with self.subTest(version=version,capability=capability),self.assertRaises(ValueError):storage.parse_pack(changed)

    def test_source_quantities_types_and_ordering_reject(self):
        for column,value in ((3,0),(3,81),(3,True),(3,4.0),(4,self.tables['Policies'][0][4]),(5,self.tables['Policies'][0][5])):
            tables=copy.deepcopy(self.tables);tables['Policies'][4][column]=value
            with self.subTest(column=column,value=value),self.assertRaises(ValueError):storage.encode(tables)
        for identity in (0,True,65536):
            ir=copy.deepcopy(self.ir);ir['policies'][0]['definition_id']=identity
            with self.subTest(identity=identity),self.assertRaises(ValueError):storage.lower(ir,self.receipt)

    def test_observation_unknown_fields_versions_types_and_ownership_reject(self):
        changes=[lambda v:v.update(schema=True),lambda v:v.update(schema=2),lambda v:v.update(extra=1),
                 lambda v:v['engine'].update(extra=1),lambda v:v['engine'].update(major=True),lambda v:v['engine'].update(hash='0'*40),
                 lambda v:v['pairs'].pop(),lambda v:v['pairs'][0].update(less=1),lambda v:v['pairs'][0].update(locale='zh_CN'),
                 lambda v:v['pairs'][0].update(a='Unknown'),lambda v:v['pairs'][0].update(extra=1),
                 lambda v:v['rows'].pop(),lambda v:v['rows'][0].update(index=True),lambda v:v['rows'][0].update(extra=1),
                 lambda v:v['rows'][0]['before'][0].update(uid=True),lambda v:v['rows'][0]['once'][0].update(doses=0),
                 lambda v:v['rows'][0]['twice'][0].update(equipped=True),lambda v:v['rows'][0]['once'][0].update(extra=1)]
        for change in changes:
            value=copy.deepcopy(self.original);change(value)
            with self.subTest(change=change),self.assertRaises(ValueError):reference.validate(value,self.inputs)


if __name__=='__main__':unittest.main()
