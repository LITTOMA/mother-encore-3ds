import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_npcs as npc,mick_treats as m,field_npc_motion as motion
from tools.godot_text import Vec2

class FieldNpcTests(unittest.TestCase):
    def test_policy_rejects_unknown_and_duplicate_bindings(self):
        policy=npc.load_policy()
        for mutate in (lambda p:p.update(extra=True),lambda p:p['actors'].append(copy.deepcopy(p['actors'][0])),lambda p:p['actors'][0].update(motion='../escape.encmick'),lambda p:p['actors'][0].update(extra=1)):
            candidate=copy.deepcopy(policy);mutate(candidate)
            with tempfile.TemporaryDirectory()as folder:
                path=Path(folder)/'policy.json';path.write_text(json.dumps(candidate),encoding='utf-8')
                with self.assertRaises(ValueError):npc.load_policy(path)
        with tempfile.TemporaryDirectory()as folder:
            path=Path(folder)/'duplicate.json';path.write_text('{"schema":1,"schema":1}',encoding='utf-8')
            with self.assertRaises(ValueError):npc.load_policy(path)
    def test_source_owners_and_programme_separation(self):
        policy,records,sources=npc.build()
        self.assertEqual([r['binding']['alias']for r in records],['lindgren','dog','iggy','zombie1','zombie2'])
        self.assertEqual([r['features']for r in records],[2,5,2,0,0])
        for record in records:
            ir=record['ir'];self.assertEqual((ir['texts'],ir['commands'],ir['programmes']),([],[],[]))
            self.assertIsNone(ir['actor']['item'])
            if record['binding'].get('interaction_policy'):motion.parse_policy(record['blob'])
            else:m.parse(record['blob'],allow_motion=True)
            with self.assertRaises(ValueError):m.parse(record['blob'])
        self.assertIn('Scripts/Main/npc.gd',sources)
        legacy=json.loads((npc.ROOT/'content/mick-treats.json').read_text(encoding='utf-8'))
        self.assertEqual(m.lower(legacy),(npc.ROOT/'romfs/world/podunk/podunk.encmick').read_bytes())
        # Schema1 remains a real frozen baseline, not a projection of new rows.
        old=npc.load_policy(npc.ROOT/'tests/fixtures/pre-graveyard-npcs.json')
        with patch.object(npc,'load_policy',return_value=old):_,previous,_=npc.build()
        self.assertEqual([r['blob']for r in previous],[r['blob']for r in records[:2]])
        self.assertEqual(records[2]['ir']['actor']['position'],[1432,4744])
        self.assertEqual(records[2]['ir']['actor']['sprite_position'],[1432,4736])
        self.assertEqual(records[2]['ir']['rng'],[])
        self.assertEqual(records[2]['disappear'],'returned_shovel')
        old=npc.load_policy(npc.ROOT/'tests/fixtures/pre-cemetery-npcs.json')
        with patch.object(npc,'load_policy',return_value=old):_,previous,_=npc.build()
        self.assertEqual([r['blob']for r in previous],[r['blob']for r in records[:3]])
        self.assertEqual([r['ir']['actor']['sprite_position']for r in records[3:]],[[580,4530],[672,4530]])
        self.assertEqual([r['disappear']for r in records[3:]],['cem_roots_grown']*2)
        self.assertEqual(records[3]['ir']['texture'],records[4]['ir']['texture'])
    def test_motion_encoder_rejects_programmes(self):
        legacy=json.loads((npc.ROOT/'content/mick-treats.json').read_text(encoding='utf-8'))
        with self.assertRaises(ValueError):m.lower(legacy,motion_owner=2,motion_flags=2)
    def test_unknown_source_actor_rejected(self):
        policy=copy.deepcopy(npc.load_policy());policy['actors'][0]['path']='Objects/missing'
        with patch.object(npc,'load_policy',return_value=policy),self.assertRaises(ValueError):npc.build()

    def test_interaction_policy_versions_and_flags_reject(self):
        original=npc.load_policy()
        mutations=[lambda p:p.update(schema=True),lambda p:p.update(schema=0),lambda p:p.update(schema=4),lambda p:p.update(schema=1),lambda p:p['actors'][2].update(interaction_policy=1),lambda p:p['actors'][2].update(interaction_policy='yes'),lambda p:[a.update(interaction_policy=False)for a in p['actors']]]
        for mutate in mutations:
            p=copy.deepcopy(original);mutate(p)
            with tempfile.TemporaryDirectory()as folder:
                file=Path(folder)/'policy.json';file.write_text(json.dumps(p),encoding='utf8')
                with self.assertRaises(ValueError):npc.load_policy(file)

    def test_ready_overwritten_sprite_properties_are_limited(self):
        image='Graphics/Character Sprites/Npcs/4dir/gravedigger.png';animation='Data/Animations/4dir.yaml'
        base=dict(visible=False,position=Vec2(0,0),offset=Vec2(0,0),_auto_offset=True)
        actual=dict(base,visible=True,offset=Vec2(0,-17),sprite='res://'+image,yaml='res://'+animation)
        npc.validate_sprite_overrides(actual,base,image,animation,image)
        for edit in [lambda p:p.update(position=Vec2(1,0)),lambda p:p.update(offset=Vec2(1,-17)),lambda p:p.update(offset=Vec2(0,float('nan'))),lambda p:p.update(_auto_offset=False),lambda p:p.update(visible=1),lambda p:p.update(sprite='res://wrong.png'),lambda p:p.update(yaml='res://wrong.yaml'),lambda p:p.update(material='unknown')]:
            p=copy.deepcopy(actual);edit(p)
            with self.assertRaises(ValueError):npc.validate_sprite_overrides(p,base,image,animation,image)
        with self.assertRaises(ValueError):npc.validate_sprite_overrides(actual,base,image,animation,'other.png')

    def test_sprite_ready_source_setup_order_is_checked(self):
        from tools.extract_battle_entry import Extractor
        ex=Extractor(npc.ROOT);code=ex.text('Scripts/Main/npc.gd');sprite=ex.text('Scripts/Main/character_sprite.gd')
        npc.validate_sprite_ready_source(code,sprite)
        for a,b in [(code.replace('_update_sprite_and_animations()','missing()',1),sprite),(code.replace('character_sprite.set_spritesheet()','missing()'),sprite),(code,sprite.replace('offset = _default_offset + sprite_offset','offset = sprite_offset')),(code,sprite.replace('show()','hide()'))]:
            with self.assertRaises(ValueError):npc.validate_sprite_ready_source(a,b)

    def test_explicit_texture_aliases_are_required_and_source_checked(self):
        policy,records,_=npc.build()
        changes=[lambda p:p.update(texture_aliases=[]),lambda p:p.update(schema=2),lambda p:p['texture_aliases'][0].update(owner=6),lambda p:p['texture_aliases'][0].update(owner=999),lambda p:p['texture_aliases'][0].update(actor=5),lambda p:p['texture_aliases'].append(dict(owner=5,actor=6)),lambda p:p['texture_aliases'][0].update(extra=0),lambda p:p['actors'][4].update(texture='graphics/world/unused.t3x')]
        for change in changes:
            p=copy.deepcopy(policy);change(p)
            with tempfile.TemporaryDirectory()as folder:
                file=Path(folder)/'p.json';file.write_text(json.dumps(p))
                with self.assertRaises(ValueError):npc.load_policy(file)
        for key,value in [('source','other.png'),('size',[100,100]),('columns',1),('rows',1)]:
            bad=copy.deepcopy(records);bad[4]['ir']['texture'][key]=value
            with self.assertRaises(ValueError):npc.validate_texture_aliases(policy,bad)

if __name__=='__main__':unittest.main()
