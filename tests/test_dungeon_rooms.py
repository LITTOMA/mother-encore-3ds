import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import dungeon_rooms as d,run_dungeon_rooms_reference as native
from tests import prepare_dungeon_rooms as fixture
ROOT=Path(__file__).resolve().parents[1]
class DungeonRooms(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.model,cls.raw,cls.proof=d.extract()
 def test_native_runs_match_and_reviewed_sources_remain_fixed(self):
  left=fixture.REFERENCE;right=left.parent/'native-7'
  self.assertEqual((left/'observations.json').read_bytes(),(right/'observations.json').read_bytes())
  for folder in (left,right):
   doc,p=fixture.reference(folder);self.assertEqual((len(doc['rows']),len(doc['geometry'])),(140,36))
   self.assertEqual(p['producer_sha256'],hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest())
   self.assertEqual(p['source']['producer_sha256'],hashlib.sha256(Path(d.__file__).read_bytes()).hexdigest())
   self.assertEqual(p['outputs_sha256']['source.encdrooms'],hashlib.sha256(self.raw).hexdigest())
   for name,digest in p['adapter_sha256'].items():self.assertEqual(hashlib.sha256((folder/name).read_bytes()).hexdigest(),digest)
 def test_hidden_parent_indices_flags_and_roundtrip(self):
  self.assertFalse(self.proof['container_visible']);self.assertEqual([r['index']for r in self.model['rooms']],list(range(18)))
  self.assertEqual([r['flag']for r in self.model['rooms']],['Catacombs/room_'+str(i)for i in range(18)])
  self.assertEqual(d.parse(self.raw),self.model);self.assertEqual(len(self.raw),2092)
 def test_source_idle_order_cancellation_and_scalar_truncation_are_preserved(self):
  doc,p=fixture.reference();rows={(r['viewport'],r['action']):r for r in doc['rows']}
  self.assertEqual(rows[0,'idle/1']['limits'],[-848,64,-448,288]);self.assertEqual(rows[1,'idle/1']['limits'],[-848,56,-448,296])
  for vi in (0,1):
   self.assertEqual(rows[vi,'idle/8']['limits'][1],-591)
   self.assertEqual(rows[vi,'overlap_exit1_keeps_last_limits']['limits'],rows[vi,'overlap_idle']['limits'])
   self.assertEqual(len(rows[vi,'cancel_idle']['offset_calls']),0)
   self.assertEqual(len(rows[vi,'same_frame_reenter_idle_twice']['offset_calls']),2)
   self.assertEqual(rows[vi,'other_body_ignored']['camareas'],0)
   self.assertEqual(rows[vi,'reenter_visited']['flag_signals'],0)
 def test_unknown_version_capability_reserved_and_truncation_fail_closed(self):
  for n in range(len(self.raw)):
   with self.assertRaises(ValueError):d.parse(self.raw[:n])
  for at,v in ((8,2),(20,0),(20,2),(24,5),(28,1),(32,0),(52,2),(56,1),(60,1)):
   b=bytearray(self.raw);struct.pack_into('<I',b,at,v);struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
   with self.assertRaises(ValueError):d.parse(b)
 def test_invalid_model_ownership_geometry_and_filter_are_rejected(self):
  for key,value in (('id',0),('index',1),('flag','Other/room_0'),('path','../room'),('center',[float('nan'),0]),('extents',[0,1]),('size',[1,1]),('layer',0x100000),('bits',4)):
   m=copy.deepcopy(self.model);m['rooms'][0][key]=value
   with self.assertRaises(ValueError):d.encode(m)
  m=copy.deepcopy(self.model);m['rooms'][1]['id']=m['rooms'][0]['id']
  with self.assertRaises(ValueError):d.encode(m)
 def test_unknown_review_and_source_overrides_fail_closed(self):
  review=json.loads(d.RECIPE.read_bytes())
  with tempfile.TemporaryDirectory()as tmp:
   p=Path(tmp)/'review.json'
   for key,value in (('schema',2),('schema',True),('commit','0'*40)):
    candidate=copy.deepcopy(review);candidate[key]=value;p.write_text(json.dumps(candidate))
    with self.assertRaises(ValueError):d.read(p)
   p.write_text('{"schema":1,"schema":1}')
   with self.assertRaisesRegex(ValueError,'duplicate'):d.read(p)
  original=d.instantiate
  def unknown(*args,**kw):
   nodes=original(*args,**kw);target=next(i for i,n in enumerate(nodes)if n.path=='Rooms/0');node=copy.copy(nodes[target]);node.props=dict(node.props,unsupported_mode=True);nodes[target]=node;return nodes
  with patch.object(d,'instantiate',side_effect=unknown):
   with self.assertRaisesRegex(ValueError,'area override'):d.extract()
  def wrong_root(*args,**kw):
   nodes=original(*args,**kw);root=copy.copy(nodes[0]);root.script='Scripts/Unsupported.gd';nodes[0]=root;return nodes
  with patch.object(d,'instantiate',side_effect=wrong_root):
   with self.assertRaisesRegex(ValueError,'root inheritance'):d.extract()
 def test_fixture_rejects_unknown_observation(self):
  with self.assertRaisesRegex(ValueError,'Unknown'):fixture.actions('invented-room-action')
if __name__=='__main__':unittest.main()
