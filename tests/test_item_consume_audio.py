import copy,unittest
from pathlib import Path
from tools import item_consume_audio as audio,item_consumption as consume,catacombs_item_catalog as catalog

ROOT=Path(__file__).resolve().parents[1]


class ItemConsumeAudioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model,cls.bus=audio.source(ROOT)
        cls.document=catalog.read(ROOT/audio.IR)

    def test_actual_source_slot_and_bus_are_distinct(self):
        self.assertEqual(self.model['slot'],'menu');self.assertEqual(self.bus,'SFX')
        rows=audio.bindings(ROOT)
        self.assertEqual(len(rows),1);self.assertEqual(rows[0]['source'],self.model['sound'])
        self.assertEqual(rows[0]['gain_db'],0);self.assertIsNone(rows[0]['conversion'])

    def test_source_fields_versions_types_and_effect_lane_reject(self):
        changes=[lambda v:v.update(extra=1),lambda v:v.update(schema=True),lambda v:v.update(schema=2),
                 lambda v:v.update(commit='0'*40),lambda v:v.update(consumption_ir='../outside.json'),
                 lambda v:v.update(consumption_sha256='0'*64),lambda v:v.update(consumption_sha256=True),
                 lambda v:v.update(source_slot='SFX'),lambda v:v.update(source_bus='menu'),
                 lambda v:v['identity'].update(kind='other'),lambda v:v['identity'].update(value=True),
                 lambda v:v['identity'].update(value=0),lambda v:v['identity'].update(value=2**32),
                 lambda v:v['identity'].update(extra=1),lambda v:v.update(pcm='../outside.pcm'),
                 lambda v:v.update(pcm='sound/effects/../outside.pcm'),lambda v:v.update(pcm=True),
                 lambda v:v.update(gain_db=True),lambda v:v.update(gain_db=1),lambda v:v.update(gain_db=float('nan')),
                 lambda v:v.update(conversion={}),lambda v:v.update(license_review='')]
        for change in changes:
            value=copy.deepcopy(self.document);change(value)
            with self.subTest(change=change),self.assertRaises(ValueError):audio.validate(value,self.model,self.bus)
        model=copy.deepcopy(self.model);model['lane']=2
        with self.assertRaises(ValueError):audio.validate(self.document,model,self.bus)

    def test_changed_consumption_binding_rejects(self):
        value=copy.deepcopy(self.document);value['consumption_sha256']='1'*64
        with self.assertRaises(ValueError):audio.bindings(ROOT,value)


if __name__=='__main__':unittest.main()
