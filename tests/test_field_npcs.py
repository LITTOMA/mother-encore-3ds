import copy,json,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_npcs as npc,mick_treats as m

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
        self.assertEqual([r['binding']['alias']for r in records],['lindgren','dog'])
        self.assertEqual([r['features']for r in records],[2,5])
        for record in records:
            ir=record['ir'];self.assertEqual((ir['texts'],ir['commands'],ir['programmes']),([],[],[]))
            self.assertIsNone(ir['actor']['item']);m.parse(record['blob'],allow_motion=True)
            with self.assertRaises(ValueError):m.parse(record['blob'])
        self.assertIn('Scripts/Main/npc.gd',sources)
        legacy=json.loads((npc.ROOT/'content/mick-treats.json').read_text(encoding='utf-8'))
        self.assertEqual(m.lower(legacy),(npc.ROOT/'romfs/world/podunk/podunk.encmick').read_bytes())
    def test_motion_encoder_rejects_programmes(self):
        legacy=json.loads((npc.ROOT/'content/mick-treats.json').read_text(encoding='utf-8'))
        with self.assertRaises(ValueError):m.lower(legacy,motion_owner=2,motion_flags=2)
    def test_stationary_no_collision_capability(self):
        _,records,_=npc.build();ir=records[0]['ir']
        legacy=m.lower(ir,motion_owner=2,motion_flags=2)
        self.assertEqual(legacy,records[0]['blob']);self.assertEqual(struct.unpack_from('<I',legacy,20)[0],4)
        candidate=m.lower(ir,motion_owner=2,motion_flags=10)
        self.assertEqual(struct.unpack_from('<I',candidate,20)[0],5);m.parse(candidate,allow_motion=True)
        with self.assertRaises(ValueError):m.parse(candidate)
        for flags in [9,11,15,16]:
            with self.subTest(flags=flags),self.assertRaises(ValueError):m.lower(ir,motion_owner=2,motion_flags=flags)
        for at,value in [(20,4),(20,6),(28,2),(28,9),(28,16)]:
            bad=bytearray(candidate);struct.pack_into('<I',bad,at,value)
            with self.subTest(at=at,value=value),self.assertRaises(ValueError):m.parse(bad,allow_motion=True)
    def test_unknown_source_actor_rejected(self):
        policy=copy.deepcopy(npc.load_policy());policy['actors'][0]['path']='Objects/missing'
        with patch.object(npc,'load_policy',return_value=policy),self.assertRaises(ValueError):npc.build()

if __name__=='__main__':unittest.main()
