import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools import item_consumption as consume,catacombs_item_catalog as catalog,field_menus


class ItemConsumptionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe=catalog.read(consume.ROOT/consume.IR)
        cls.model=consume.load()
        files,_,_=consume.original.source()
        cls.inputs=json.loads(files['input.json'])
        cls.first=consume.ROOT/consume.REFERENCES[0]
        cls.document=catalog.read(cls.first/'reference.json')
        cls.temp=consume.ROOT/'build/tests'
        cls.temp.mkdir(parents=True,exist_ok=True)

    def temporary(self):
        folder=tempfile.TemporaryDirectory(prefix='item-consume-',dir=self.temp)
        self.addCleanup(folder.cleanup)
        return Path(folder.name)

    def test_two_actual_originals_and_source_mapping(self):
        self.assertEqual((self.first/'reference.json').read_bytes(),(consume.ROOT/consume.REFERENCES[1]/'reference.json').read_bytes())
        self.assertEqual(consume.validate_reference(self.document,self.inputs),self.document)
        self.assertEqual(len(self.document['rows']),240)
        self.assertEqual(self.model['refresh_hp'],999)
        self.assertEqual(self.model['statuses'],['asthma','poisoned'])
        self.assertEqual([row['hp']for row in self.model['effects']],[50,20,0,0])
        self.assertEqual(self.model['sound'],'Audio/Sound effects/EB/eat.wav')
        self.assertEqual(self.model['bus'],'menu')

    def test_model_unknown_domains_types_messages_and_ownership_reject(self):
        changes=[lambda v:v.update(extra=1),lambda v:v.update(actor=True),lambda v:v.update(refresh_hp=True),
            lambda v:v.update(refresh_hp=0),lambda v:v.update(refresh_hp=65536),lambda v:v.update(lane=True),
            lambda v:v.update(lane=0),lambda v:v.update(lane=2),lambda v:v.update(bus='?'),
            lambda v:v.update(sound='/outside.wav'),lambda v:v.update(sound='../outside.wav'),
            lambda v:v.update(sound='Audio/\neat.wav'),lambda v:v.update(statuses=[]),
            lambda v:v['statuses'].append(v['statuses'][0]),lambda v:v['effects'].clear(),
            lambda v:v['effects'][0].update(hp=True),lambda v:v['effects'][0].update(hp=0),
            lambda v:v['effects'][0].update(hp=65536),lambda v:v['effects'][0].update(cure='asthma'),
            lambda v:v['effects'][2].update(cure='unknown'),lambda v:v['effects'][0].update(extra=1),
            lambda v:v['effects'][1].update(id=v['effects'][0]['id']),
            lambda v:v['effects'][0]['messages'][0].update(extra='x'),
            lambda v:v['effects'][0]['messages'][0].update(en='{button:0}'),
            lambda v:v['effects'][0]['messages'][0].update(en='{unknown}'),
            lambda v:v['effects'][0]['messages'][0].update(en='[image]'),
            lambda v:v['effects'][0]['messages'][2].update(en='unowned'),
            lambda v:v['effects'][0]['message_keys'].__setitem__(2,'UNUSED'),
            lambda v:v['effects'][0]['message_keys'].pop()]
        for change in changes:
            value=copy.deepcopy(self.model);change(value)
            with self.subTest(change=change),self.assertRaises(ValueError):consume.encode_extension(value)

    def test_result_unknown_version_fields_types_and_events_reject(self):
        changes=[lambda v:v.update(schema=True),lambda v:v.update(schema=2),lambda v:v.update(extra=1),
            lambda v:v['engine'].update(extra=1),lambda v:v['engine'].update(major=True),
            lambda v:v['engine'].update(hash='0'*40),lambda v:v['rows'].pop(),
            lambda v:v['rows'][0].update(index=True),lambda v:v['rows'][0].update(index=1),
            lambda v:v['rows'][0].update(extra=1),lambda v:v['rows'][0]['before'].update(extra=1),
            lambda v:v['rows'][0]['before'].update(hp=True),
            lambda v:v['rows'][0]['before']['inventory'][0].update(item_name=[]),
            lambda v:v['rows'][0]['after']['inventory'][0].update(uid=True),
            lambda v:v['rows'][0].update(actions={'99':True}),lambda v:v['rows'][0].update(actions={'2':1}),
            lambda v:v['rows'][0].update(signals=[['unknown']]),
            lambda v:v['rows'][0]['requests'][0].__setitem__(2,'unknown'),
            lambda v:v['rows'][0]['queued_messages'][0].__setitem__(2,1)]
        for change in changes:
            value=copy.deepcopy(self.document);change(value)
            with self.subTest(change=change),self.assertRaises(ValueError):consume.validate_reference(value,self.inputs)

    def test_reviewed_recipe_and_receipt_cannot_be_rebound(self):
        file=self.temporary()/'candidate.json'
        changes=[lambda v:v.update(schema=True),lambda v:v.update(extra=1),lambda v:v.update(commit='0'*40),
                 lambda v:v['model'].update(refresh_hp=1),lambda v:v['model'].update(actor='other'),
                 lambda v:v['model']['effects'].reverse(),lambda v:v['source'].update(extra=1)]
        with patch.object(consume,'derive',return_value=self.recipe):
            for change in changes:
                value=copy.deepcopy(self.recipe);change(value);file.write_text(json.dumps(value),encoding='utf-8')
                with self.subTest(change=change),self.assertRaises(ValueError):consume.load(file)
        receipt=catalog.read(self.first/'receipt.json')
        receipt['producer_sha256']='0'*64
        real_read=catalog.read
        with patch.object(catalog,'read',side_effect=lambda path:receipt if Path(path)==self.first/'receipt.json'else real_read(path)),self.assertRaises(ValueError):
            consume.reference_records(self.recipe)

    def test_source_function_boundaries_preserve_column_zero_comments(self):
        raw=b'func a():\n\tif true:\n# source comment\n\t\tpass\n\nfunc b():\n\treturn 1\n'
        methods=consume.original.source_methods(raw)
        self.assertIn('# source comment\n\t\tpass',methods['a'])
        self.assertNotIn('func b',methods['a'])
        for raw in (b'no function',b'func a():\n',b'func a():\n\t# comment only\n',b'func a():\n\tpass\nfunc a():\n\tpass\n'):
            with self.assertRaises(ValueError):consume.original.source_methods(raw)

    def test_menu_extension_versions_are_explicit_and_legacy_bytes_stay_identical(self):
        source=catalog.read(consume.ROOT/'content/native-field-menus.json')
        self.assertEqual(field_menus.encode(source),(consume.ROOT/'romfs/ui/field.encmenus').read_bytes())
        invalid=copy.deepcopy(source);invalid['consumption']=self.model
        with self.assertRaises(ValueError):field_menus.encode(invalid)
        invalid.pop('consumption');invalid['schema']=3
        with self.assertRaises(ValueError):field_menus.encode(invalid)
        invalid['schema']=4
        with self.assertRaises(ValueError):field_menus.encode(invalid)


if __name__=='__main__':unittest.main()
