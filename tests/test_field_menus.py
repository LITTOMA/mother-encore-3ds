import copy,json,unittest,tempfile
from unittest import mock
from pathlib import Path
from tools import field_menus as menus
from tools.extract_battle_entry import require

class FieldMenuSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.ir=menus.load()
    def test_source_and_staged_bytes(self):
        self.assertEqual(self.ir,menus.build())
        raw=menus.encode(self.ir)
        self.assertEqual((menus.ROOT/'romfs'/menus.PACK).read_bytes(),raw)
        self.assertEqual(menus.stage_files(menus.ROOT/'romfs'),{menus.PACK:raw})
    def test_semantics_are_bound_to_source(self):
        items={i['id']:i for i in self.ir['items']}
        self.assertEqual(items['AsthmaSpray']['action'],2)
        self.assertEqual(items['AsthmaSpray']['doses'],3)
        self.assertIn('{target}',items['AsthmaSpray']['result']['en'])
        for i in items.values():
            if i['key']:self.assertFalse(i['drop']);self.assertNotIn(i['action'],(1,2))
        self.assertEqual(items['MapPodunk']['action'],3)
        self.assertEqual(self.ir['map_offset'],[98,132])
        self.assertEqual(self.ir['map_home'],[99,127])
    def test_unknown_source_or_rule_does_not_rebind(self):
        for key,value in [('schema',2),('schema',True),('commit','0'*40),('map_divisor',8),('rows',100),('logical_buttons',[1]*8)]:
            bad=copy.deepcopy(self.ir);bad[key]=value
            with tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);(root/'content').mkdir();(root/'content/native-field-menus.json').write_text(json.dumps(bad),encoding='utf8')
                with mock.patch.object(menus,'build',return_value=self.ir),self.assertRaises(ValueError):menus.load(root)
    def test_new_game_default_bytes_are_preserved(self):
        from tools import native_session as session
        r=session.read(session.IR)
        self.assertEqual(r['schema'],9)
        self.assertTrue(r['description_toggle'])
        self.assertEqual({i['item_id'] for i in r['field_items']},{i['item_id'] for i in r['storage_policies']})

if __name__=='__main__':unittest.main()
