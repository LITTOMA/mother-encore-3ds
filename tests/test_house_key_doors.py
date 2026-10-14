import copy
import json
import unittest
from tools.extract_battle_entry import Extractor, ROOT
from tools.house_key_doors import BINDING, texts, append_house


class KeyDoorBindings(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binding=json.loads((ROOT/BINDING).read_text(encoding='utf8'))
        cls.house=json.loads((ROOT/'content/native-house.json').read_text(encoding='utf8'))

    def build(self,binding):
        return texts(Extractor(ROOT),self.house,binding)

    def test_original_key_and_localized_text(self):
        rows=self.build(self.binding)
        self.assertEqual([r['id'] for r in rows],[73,74])
        self.assertEqual(rows[0]['localized'],{'en':"[@]It's locked.",'zh_Hans_CN':'[@]锁住了。'})
        self.assertEqual(rows[1]['localized'],{'en':'[@][PartyLead] used the Basement Key.','zh_Hans_CN':'[@][PartyLead]使用了地下室钥匙。'})
        prefix=copy.deepcopy(self.house)
        prefix['dialogues']=prefix['dialogues'][:-2]
        prefix['segments']=prefix['segments'][:-2]
        result=append_house(Extractor(ROOT),prefix)
        self.assertEqual(result['dialogues'],self.house['dialogues'])
        self.assertEqual(result['segments'],self.house['segments'])
        self.assertEqual(prefix['segments'],self.house['segments'][:-2])

    def test_prompt_and_naming_dependency_reuse_rejects_semantic_changes(self):
        from tools import house_button_prompt_assets as prompts,new_game_assets as naming
        recipe=json.loads(prompts.IR.read_text());previous=copy.deepcopy(self.house)
        candidate=prompts.rebound_house_dependency(recipe,previous,self.house,'a'*64)
        self.assertEqual(prompts.encode(candidate),prompts.PACK.read_bytes())
        for key in ('npcs','openable_doors','interaction'):
            bad=copy.deepcopy(self.house);bad[key]={}
            with self.assertRaises(ValueError):prompts.rebound_house_dependency(recipe,previous,bad,'a'*64)
        round_ir=json.loads((ROOT/'content/native-round.json').read_text())
        changed=copy.deepcopy(round_ir);changed['dependencies']={}
        naming.rebound_checked_dependency(round_ir,changed)
        changed['skills']=[]
        with self.assertRaises(ValueError):naming.rebound_checked_dependency(round_ir,changed)
        with self.assertRaises(ValueError):naming.rebound_checked_dependency({'unknown':1},{'unknown':2})
        room=json.loads((ROOT/'content/native-opening.json').read_text());changed=copy.deepcopy(room);changed['provenance']={}
        naming.rebound_checked_dependency(room,changed)
        changed['sections']={}
        with self.assertRaises(ValueError):naming.rebound_checked_dependency(room,changed)

    def test_font_catalogue_reuse_requires_identical_glyphs(self):
        from tools.source_font_reuse import catalogue_reuse
        catalog={'commit':'pinned','locales':[{'name':'English','code':'en','font':'source'}],'records':[{'values':{'en':'abc'}}]}
        manifest={'catalog_sha256':'old','pages':[{'sha256':'checked'}]}
        result=catalogue_reuse(catalog,copy.deepcopy(catalog),manifest,'a'*64)
        self.assertEqual(result['pages'],manifest['pages']);self.assertEqual(manifest['catalog_sha256'],'old')
        changed=copy.deepcopy(catalog);changed['records'][0]['values']['en']='abc中'
        with self.assertRaises(ValueError):catalogue_reuse(catalog,changed,manifest,'a'*64)
        changed=copy.deepcopy(catalog);changed['locales'][0]['font']='unknown'
        with self.assertRaises(ValueError):catalogue_reuse(catalog,changed,manifest,'a'*64)
        changed=copy.deepcopy(catalog);changed['commit']='unknown'
        with self.assertRaises(ValueError):catalogue_reuse(catalog,changed,manifest,'a'*64)
        with self.assertRaises(ValueError):catalogue_reuse(catalog,catalog,manifest,'unknown')

    def test_unknown_binding_and_source_rejected(self):
        for mutate in (lambda b:b.update(schema=2),lambda b:b.update(extra=1),lambda b:b.update(commit='0'*40),
                       lambda b:b['sources'].update({'Data/Items/KeyBasement.yaml':'0'*64}),
                       lambda b:b.update(doors=[]),lambda b:b.update(prefix_text_id=71),
                       lambda b:b['doors'].append(copy.deepcopy(b['doors'][0])),
                       lambda b:b['doors'][0].update(key='Unknown'),lambda b:b['doors'][0].update(flag='Unknown'),
                       lambda b:b['doors'][0].update(id=999),lambda b:b['doors'][0].update(extra=1),
                       lambda b:b['doors'][0].update(opened_text_id=73)):
            b=copy.deepcopy(self.binding);mutate(b)
            with self.assertRaises((ValueError,KeyError)):
                self.build(b)

    def test_unmapped_door_policy_and_item_rejected(self):
        for policy in (1,2,4,8):
            house=copy.deepcopy(self.house);house['openable_doors'][2]['policy']=policy
            with self.assertRaises(ValueError):texts(Extractor(ROOT),house,self.binding)
        b=copy.deepcopy(self.binding);del b['sources']['Data/Items/KeyBasement.yaml']
        with self.assertRaises(ValueError):self.build(b)
        house=copy.deepcopy(self.house);house['openable_doors'][2]['opened_dialogue']='Data/Dialogue/Reusable/unknown.yaml'
        with self.assertRaises(ValueError):texts(Extractor(ROOT),house,self.binding)


if __name__=='__main__':unittest.main()
