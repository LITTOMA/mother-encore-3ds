import copy
import hashlib
import struct
import tempfile
import unittest
from pathlib import Path
from tools import native_session as native

class NativeSessionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.recipe=native.read(native.IR)
    def test_checked_source_and_staging(self):
        native.verify_recipe(self.recipe)
        self.assertEqual(native.PACK.read_bytes(),native.encode(self.recipe))
        self.assertEqual(native.stage_files(native.ROOT/'romfs'),{Path('data/opening.encsession'):native.PACK.read_bytes()})
    def test_source_defaults_and_derived_values(self):
        r=self.recipe;s=r['defaults'];c=s['characters'][0]
        self.assertEqual((r['saved_flag'],r['earned_cash_flag']),('saved','earned_cash'))
        self.assertEqual((s['cash'],s['bank'],s['earned_cash']),(0,0,0))
        self.assertEqual(c['affinity_multipliers'],[dict(id='asthma',value=1)])
        self.assertEqual(c['permanent_boosts'],[])
        self.assertEqual(r['levels'][1]['stats'],[65,27,12,12,6,6,8])
        self.assertEqual(r['levels'][1]['skills'],['strike','splitShot','telepathy'])
        self.assertIn('mimmie_door_opened',r['mutable_flags'])
        self.assertNotIn('ninten_basement_door',r['mutable_flags'])
        self.assertEqual(s['key_items'][0]['item_id'],'CashCard')
        self.assertNotEqual(s['key_items'][0]['uid'],c['inventory'][0]['uid'])
    def test_additive_startup_roster_and_legacy_bytes(self):
        r=self.recipe;startup=r['startup'];characters=startup['characters']
        self.assertEqual((r['schema'],struct.unpack_from('<I',native.encode(r),8)[0],struct.unpack_from('<I',native.encode(r),20)[0]),(4,4,4))
        self.assertEqual(hashlib.sha256(native.encode_snapshot(r['defaults'],r['compatibility'])).hexdigest(),
                         'a9cfbb498a20cb2bbb4a0a56dc5a87f9f13e37bac7ebbef14c875b67c93e7e9f')
        self.assertEqual(startup['party'],['ninten'])
        self.assertEqual(startup['favorite_food'],'')
        self.assertEqual([c['character_id']for c in characters],['ninten','ana','lloyd','pippi','teddy'])
        self.assertEqual([c['nickname']for c in characters],['']*5)
        self.assertEqual([(c['hp'],c['pp'])for c in characters],[(62,26),(42,31),(82,0),(83,0),(83,0)])
        self.assertEqual([c['learned_skills']for c in characters],[['strike','splitShot'],[],['spy'],[],[]])
        self.assertEqual(characters[2]['inventory'],[
            dict(item_id='KickMeNote',equipped=True,doses=1,uid=3),
            dict(item_id='GlassesCelluloid',equipped=True,doses=1,uid=4)])
        all_ids=[item['uid']for c in characters for item in c['inventory']]+[item['uid']for item in startup['key_items']]
        self.assertEqual(len(all_ids),len(set(all_ids)))
        self.assertEqual(r['startup_derivation'][2]['equipment_boosts']['defense'],2)
        self.assertEqual(r['startup_derivation'][2]['effective_stats']['iq'],13)
        normalized=copy.deepcopy(startup);normalized['characters']=r['defaults']['characters']
        self.assertEqual(normalized,r['defaults'])
        legacy=copy.deepcopy(r);legacy['schema']=2;older=native.encode(legacy)
        self.assertEqual(older[-len(native.encode_snapshot(startup,r['compatibility'])):],native.encode_snapshot(startup,r['compatibility']))
        self.assertEqual(native.encode(r)[24:len(older)],older[24:])
        self.assertEqual(r['settings_choices']['speeds'],[.02,.028,.035])
        self.assertEqual(r['settings_choices']['prompts'],['Both','Objects','NPCs','None'])
    def test_frozen_rules6_resource_encoding_is_unchanged(self):
        frozen=native.ROOT/'content/legacy-rules6'
        legacy=native.read(frozen/'native-session.json')
        self.assertEqual(legacy['schema'],1)
        self.assertEqual(native.encode(legacy),(frozen/'opening.encsession').read_bytes())
        bad=copy.deepcopy(self.recipe);bad['schema']=5
        with self.assertRaises(ValueError):native.encode(bad)
    def test_startup_clamps_are_derived_from_serialized_hp_and_equipment(self):
        ex=native.Extractor(native.ROOT);save=ex.yaml('Data/save_new_game.yaml');overrides=ex.yaml('Data/save_overrides.yaml')
        save['ana']['hp']=9;save['ana']['pp']=3
        original=ex.yaml
        def equipment(path):
            value=original(path)
            if path=='Data/Items/KickMeNote.yaml':value['boost']['maxhp']=7
            return value
        ex.yaml=equipment
        characters,derived=native.source_startup_characters(ex,save,overrides,self.recipe['defaults'])
        self.assertEqual((characters[1]['hp'],characters[1]['pp']),(9,3))
        self.assertEqual(characters[2]['hp'],89)
        self.assertEqual(derived[2]['effective_stats']['maxhp'],89)
    def test_acquisition_policy_is_source_derived_and_additive(self):
        from tools.drawer_program import load, IR
        drawer=load(native.ROOT);r=self.recipe
        self.assertEqual(len(r['acquisitions']),len(drawer['templates']))
        for policy,template in zip(r['acquisitions'],drawer['templates']):
            self.assertEqual((policy['item_id'],policy['doses'],policy['max_count']),
                             (template['source_item'],template['doses'],1))
            self.assertIn(policy['flag_id'],r['mutable_flags'])
            self.assertTrue(any(command['opcode']=='SetFlag' and command['a']==policy['flag_id'] and command['b']==1 for command in drawer['commands']))
            self.assertFalse(next(flag['value'] for flag in r['defaults']['flags'] if flag['id']==policy['flag_id']))
        self.assertEqual(r['dependencies'][IR],hashlib.sha256((native.ROOT/IR).read_bytes()).hexdigest())
        previous=copy.deepcopy(r);previous['schema']=3
        old=native.encode(previous);current=native.encode(r)
        self.assertEqual(old[24:],current[24:len(old)])
        for field,value in [('item_id','unknown'),('doses',0),('max_count',2),('flag_id','unknown')]:
            bad=copy.deepcopy(r);bad['acquisitions'][0][field]=value
            with self.assertRaises(ValueError):native.verify_recipe(bad)
    def test_recipe_drift_fails_closed(self):
        for edit in [lambda r:r.update(unknown=1),lambda r:r['defaults']['settings'].update(text_speed=.1),lambda r:r['levels'][1]['stats'].__setitem__(3,17),lambda r:r['defaults']['characters'][0]['affinity_multipliers'].clear(),lambda r:r.update(saved_flag='unknown'),lambda r:r['sources'].clear(),lambda r:r['camera_area_ids'].append(2),lambda r:r['startup']['characters'].pop(),lambda r:r['startup']['characters'][1].update(hp=43),lambda r:r['startup']['characters'][2]['inventory'].reverse(),lambda r:r['startup_derivation'][2]['equipment_boosts'].update(defense=3)]:
            bad=copy.deepcopy(self.recipe);edit(bad)
            with self.assertRaises(ValueError):native.verify_recipe(bad)
    def test_stale_or_escaping_stage_rejected(self):
        with tempfile.TemporaryDirectory()as name:
            root=Path(name);(root/'data').mkdir();target=root/'data/opening.encsession';target.write_bytes(b'stale')
            with self.assertRaises(ValueError):native.stage_files(root)
            target.unlink();target.symlink_to(native.PACK)
            with self.assertRaises(ValueError):native.stage_files(root)
if __name__=='__main__':unittest.main()
