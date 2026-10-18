import copy,unittest
from pathlib import Path
from tools import native_content as nc,field_scene_npcs as n,world_scene_routes as w

class PartyActor(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.room=nc.parse_pack((n.ROOT/'tests/fixtures/mayor-party-actor.encroom').read_bytes());cls.policy,cls.rows,cls.sources=n.build()
 def test_source_profiles_and_clips(self):
  r=self.room;s=r['sections'];self.assertEqual((r['rules'],r['capabilities'],r['scene_id']),(7,10,3))
  for row in self.rows:
   index=row['binding']['id']-1;p=s['ActorProfile'][index];i=s['ActorInstance'][index];t=s['Resource'][p['primary_resource']]
   self.assertEqual(p['execution_kind'],4 if row['directions']==8 else 3);self.assertEqual(p['direction_count'],row['directions']);self.assertEqual(i['binding_kind'],2)
   self.assertEqual(i['position'],row['ir']['actor']['position']);self.assertEqual(p['sprite_offset'],[row['ir']['actor']['sprite_position'][j]-i['position'][j]for j in range(2)])
   self.assertEqual(p['flags'],int(row['shadow_visible']));self.assertEqual(r['strings'][t['path_string']],row['binding']['texture'])
   bindings=s['AnimationBinding'][p['animation_binding_first']:p['animation_binding_first']+p['animation_binding_count']]
   for binding in bindings:
    name={0:'Idle',4:'Walk',5:'Talk'}[binding['motion_state']];keys=row['source_animations'][name]['directions'][binding['direction']];clip=s['Clip'][binding['clip_index']]
    compiled=s['Key'][clip['first_key']:clip['first_key']+clip['key_count']];self.assertEqual([k['frame']for k in compiled],[k[0]-1 for k in keys[1:]])
    self.assertAlmostEqual(clip['length'],keys[0]+sum(k[1]for k in keys[1:]),places=5)
 def test_versions_and_profile_negative(self):
  for cap in (7,8,9,11):
   r=copy.deepcopy(self.room);r.pop('build_input_sha256');r['capabilities']=cap;r['provenance']={'sources':{'test':'0'*64}}
   with self.assertRaises(ValueError):nc.validate_ir(r)
  for mutate in [lambda s:s['ActorProfile'][5].update(direction_count=4),lambda s:s['ActorProfile'][5].update(execution_kind=5),lambda s:s['ActorInstance'][5].update(binding_kind=1),lambda s:s['ActorProfile'][5].update(animation_binding_count=23)]:
   s=copy.deepcopy(self.room['sections']);mutate(s)
   with self.assertRaises(ValueError):nc.validate_tables(self.room['strings'],s,3,7,10)
  old=nc.parse_pack((n.ROOT/'romfs/world/house/opening.encroom').read_bytes())
  with self.assertRaises(ValueError):nc.validate_tables(old['strings'],old['sections'],old['scene_id'],7,10)
 def test_source_route_fixture(self):self.assertEqual(w.build()[0],(n.ROOT/'tests/fixtures/mayor-actor-routes.enclinks').read_bytes())

if __name__=='__main__':unittest.main()
