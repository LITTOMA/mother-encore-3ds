import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import bridge_entry as entry,bridge_round as bridge,native_round as native
ROOT=Path(__file__).resolve().parents[1]

class BridgeRoundTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.ir=bridge.read(bridge.IR)
    def test_fresh_source_and_reproducible_binary(self):
        bridge.validate(self.ir)
        blob=native.encode(native.lower(self.ir));self.assertEqual(blob,bridge.PACK.read_bytes())
        tables=native.parse_pack(blob);self.assertEqual(tables.version,6)
        self.assertEqual(len(tables['Loot']),1);self.assertEqual(len(tables['PreHit']),1)
        self.assertEqual(len(tables['ActionMedia']),5)
        self.assertEqual(self.ir['loot']['item'],'FleaBag')
        self.assertEqual(self.ir['victory']['reward_exp'],8)
        self.assertEqual(self.ir['victory']['reward_cash'],20)
        self.assertEqual([x['weight']for x in self.ir['enemy_choices']],[2,1])
    def test_locale_binding_guards_and_display_templates_both_match_catalog_contract(self):
        import sys
        sys.path.insert(0,str(ROOT/'tools'))
        from tools import localization_assets as locale
        from tools.localization_house import unescape
        catalog=locale.extract();records={r['key']:r for r in catalog['records']}
        bindings={b['identity']:b for b in catalog['bindings']}
        for i,row in enumerate(self.ir['texts']):
            if row['role']==0:continue
            guard=bindings['battle.expected/4/'+str(i)];display=bindings['battle/4/'+str(i)]
            self.assertEqual(guard['expected'],row['text'])
            for binding in (guard,display):self.assertEqual(unescape(records[binding['key']]['values']['en']),binding['expected'])
    def test_changed_reviewed_content_is_rejected(self):
        for section,key,value in [('binding','win_flag','invented_flag'),('victory','reward_cash',999),('loot','chance',100),('loot','item','InventedItem')]:
            candidate=copy.deepcopy(self.ir);candidate[section][key]=value
            with self.subTest(section=section,key=key),self.assertRaises(ValueError):native.verify_sources(candidate)
    def test_new_schema_rejects_unsupported_paths(self):
        for edit in (lambda i:i['loot'].update(rare=True),lambda i:i['pre_hit'][0].update(unknown=1),lambda i:i.update(schema=7)):
            candidate=copy.deepcopy(self.ir);edit(candidate)
            with self.assertRaises(ValueError):native.lower(candidate)
        base=native.lower(self.ir)
        for chance in (-1,0,101,float('nan')):
            tables=copy.deepcopy(base);tables['Loot'][0][1]=chance
            with self.assertRaises(ValueError):native.parse_pack(native.encode(tables))
        for skill,media,time in [(99,0,.25),(1,0,.25),(1,base['PreHit'][0][1],0),(1,base['PreHit'][0][1],1)]:
            tables=copy.deepcopy(base);tables['PreHit']=[[skill,media,time]]
            with self.assertRaises(ValueError):native.parse_pack(native.encode(tables))
        for edit in ((0,0,99),(0,1,0xfffffffe),(0,2,2),(1,0,0)):
            tables=copy.deepcopy(base);row=list(tables['ActionMedia'][edit[0]]);row[edit[1]]=edit[2];tables['ActionMedia'][edit[0]]=row
            with self.assertRaises(ValueError):native.parse_pack(native.encode(tables))
    def test_strict_recipe_json_and_entry_semantics(self):
        folder=ROOT/'build/podunk-routes';folder.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=folder)as temporary:
            p=Path(temporary)/'bad.json'
            for text in ('{"schema":1,"schema":1}','{"schema":NaN}'):
                p.write_text(text,encoding='utf8')
                with self.assertRaises(ValueError):entry.read(p)
        ir=entry.read(ROOT/entry.policy()['ir']);candidate=copy.deepcopy(ir);candidate['enemy']['data']['hp']+=1
        with self.assertRaises(ValueError):entry.validate(candidate)
    def test_bite_visible_frames_and_source_bounds(self):
        from tools.extract_battle_entry import Extractor
        spec=bridge.effect(Extractor(ROOT));self.assertEqual(spec['size'],[1001,85]);self.assertEqual(spec['grid'],[11,1]);self.assertEqual(spec['apply_time'],.25)
        self.assertEqual([f for _,f in spec['frames']],list(range(11)));self.assertEqual(spec['length'],.55)

if __name__=='__main__':unittest.main()
