import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import dungeon_switches as d
from tools.godot_text import NodePathValue
ROOT=Path(__file__).resolve().parents[1]
class DungeonSwitchesTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.review=json.loads(d.RECIPE.read_bytes());cls.data,cls.proof=d.extract();cls.blob=d.encode(cls.data)
 def altered_review(self,edit):
  value=copy.deepcopy(self.review);edit(value)
  with tempfile.TemporaryDirectory()as directory:
   file=Path(directory)/'review.json';file.write_text(json.dumps(value),encoding='utf8')
   with self.assertRaises(ValueError):d.extract(file)
 def bad_blob(self,changes):
  blob=bytearray(self.blob)
  for at,value in changes:struct.pack_into('<I',blob,at,value)
  struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]))
  with self.assertRaises(ValueError):d.parse(bytes(blob))
 def section(self,index):return struct.unpack_from('<I',self.blob,64+index*16+4)[0]
 def test_actual_graph_and_roundtrip(self):
  self.assertEqual(d.parse(self.blob),self.data);self.assertEqual((len(self.data['switches']),len(self.data['objects']),len(self.data['edges'])),(5,9,9));self.assertFalse(self.proof['whole_scripts_approved'])
  switches={n['path']:n for n in self.data['switches']};objects={n['path']:n for n in self.data['objects']}
  self.assertEqual(switches['Bottom2/switch']['flag'],'hidden_entrance_opened');self.assertEqual(switches['Bottom2/switch1']['flag'],'Catacombs/switch1')
  self.assertEqual([n['bits']for n in self.data['switches']],[112,5,97,17,17]);self.assertEqual(objects['Objects/Gate3']['bits'],1);self.assertEqual(objects['Objects/Gate8']['bits'],1)
  self.assertEqual(objects['Bottom2/Wall Door']['offset_y'],44);self.assertEqual(switches['Bottom2/switch1']['count'],4)
 def test_schema_pin_fields_types_source_coverage(self):
  for edit in(lambda p:p.update(schema=2),lambda p:p.update(schema=True),lambda p:p.update(commit='0'*40),lambda p:p.update(unknown=1),lambda p:p.update(scene_id=True),lambda p:p['sources'].pop(p['paths']['flag_script']),lambda p:p['sources'].update({p['paths']['flag_script']:'0'*64})):
   with self.subTest(edit=edit):self.altered_review(edit)
 def test_duplicate_json_rejected(self):
  with tempfile.TemporaryDirectory()as directory:
   p=Path(directory)/'review.json';p.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):d.read(p)
 def test_review_identity_kind_and_coverage(self):
  for edit in(lambda p:p['nodes'][0].update(id=True),lambda p:p['nodes'][0].update(path='../escape'),lambda p:p['nodes'][1].update(id=p['nodes'][0]['id']),lambda p:p['nodes'][0].update(kind='unknown'),lambda p:p['nodes'].reverse(),lambda p:p['nodes'].pop()):
   with self.subTest(edit=edit):self.altered_review(edit)
 def test_unknown_override_boolean_flag_and_target(self):
  original=d.instantiate
  for edit in(lambda n:n.props.update(unknown=1),lambda n:n.props.update(_is_one_way=1),lambda n:n.props.update(flag=False),lambda n:n.props.update(_controlled_objects=[NodePathValue('../../missing')]),lambda n:n.props.update(_controlled_objects=[NodePathValue('.')]),lambda n:n.props.update(_controlled_objects=['../Wall Door']),lambda n:n.props.update(_camera_delay=float('nan'))):
   def changed(load,source):
    nodes=copy.deepcopy(original(load,source));edit(next(n for n in nodes if n.path=='Bottom2/switch'));return nodes
   with self.subTest(edit=edit),patch.object(d,'instantiate',side_effect=changed),self.assertRaises(ValueError):d.extract()
 def test_unknown_synced_object_order_and_position(self):
  original=d.instantiate
  for edit in(lambda n:n.props.update(_is_synced_with_room=True),lambda n:n.props.update(_initially_on=0),lambda n:n.props.update(_update_order=True),lambda n:n.props.update(_update_order=32768),lambda n:n.props.update(_move_cam_offset=None)):
   def changed(load,source):
    nodes=copy.deepcopy(original(load,source));edit(next(n for n in nodes if n.path=='Bottom2/Wall Door'));return nodes
   with self.subTest(edit=edit),patch.object(d,'instantiate',side_effect=changed),self.assertRaises(ValueError):d.extract()
 def test_original_dispatch_order_and_inheritance(self):
  original=d.Extractor.text
  for target,old,new in(('abstract_script','emit_signal("switch_hit")','emit_signal("different")'),('two_state_script','state = (state != switch.is_on())','state = switch.is_on()'),('list_script','extends AbstractSwitch','extends Node'),('scene_script','create_timer(0.5)','create_timer(0.6)')):
   def changed(ex,name):return original(ex,name).replace(old,new)if name==self.review['paths'][target]else original(ex,name)
   with self.subTest(target=target),patch.object(d.Extractor,'text',new=changed),self.assertRaises(ValueError):d.extract()
 def test_header_versions_capability_pin_and_crc(self):
  for changes in([(0,0)],[(8,2)],[(12,len(self.blob)+1)],[(20,2)],[(24,0)],[(28,1)],[(32,0)],[(52,4)],[(56,1)],[(60,1)]):
   with self.subTest(changes=changes):self.bad_blob(changes)
  blob=bytearray(self.blob);blob[-1]^=1
  with self.assertRaises(ValueError):d.parse(bytes(blob))
  for length in(0,64,143,len(self.blob)-1):
   with self.subTest(length=length),self.assertRaises(ValueError):d.parse(self.blob[:length])
 def test_policy_floats_and_callback_identity(self):
  at=self.section(1)
  for changes in([(at+8,0)],[(at+8,0x7fc00000)],[(at+12,0)],[(at+16,self.data['switches'][0]['id'])],[(at+20,0)],[(at+24,0x7f800000)],[(at+28,1)],[(at+36,0)],[(at+44,0)]):
   with self.subTest(changes=changes):self.bad_blob(changes)
 def test_switch_record_flag_text_counter_and_kind(self):
  at=self.section(2)
  for changes in([(at,0)],[(at+64,self.data['switches'][0]['id'])],[(at+8,0)],[(at+20,128)],[(at+24,0x7fc00000)],[(at+28,0x7f800000)],[(at+36,1)],[(at+40,0)],[(at+52,3)],[(at+56,1)],[(at+60,1)]):
   with self.subTest(changes=changes):self.bad_blob(changes)
 def test_object_record_reserved_order_state_and_kind(self):
  at=self.section(3)
  for changes in([(at,self.data['switches'][0]['id'])],[(at+12,0)],[(at+16,32768)],[(at+20,4)],[(at+24,0x7fc00000)],[(at+40,1)],[(at+44,1)]):
   with self.subTest(changes=changes):self.bad_blob(changes)
 def test_edges_duplicate_missing_wrong_kind_and_owner(self):
  at=self.section(4);edges=self.data['edges']
  for changes in([(at,0)],[(at+4,self.data['switches'][0]['id'])],[(at+8,edges[0][0]),(at+12,edges[0][1])],[(at+4,0)]):
   with self.subTest(changes=changes):self.bad_blob(changes)
 def test_canonical_strings_and_encoded_types(self):
  at=self.section(0)
  for value in(0,255,ord(':')):
   b=bytearray(self.blob);b[at]=value;struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
   with self.subTest(value=value),self.assertRaises(ValueError):d.parse(bytes(b))
  for edit in(lambda p:p['switches'][0].update(bits=True),lambda p:p['objects'][0].update(order=False),lambda p:p.update(ready=True),lambda p:p['edges'][0].__setitem__(0,True)):
   p=copy.deepcopy(self.data);edit(p)
   with self.subTest(edit=edit),self.assertRaises(ValueError):d.encode(p)
 def test_cli_rejects_release_destination(self):
  target=ROOT/'romfs/world/catacombs/candidate.encdsw';self.assertFalse(target.exists())
  with patch.object(d.sys,'argv',['dungeon_switches.py','--out',str(target)]),self.assertRaisesRegex(ValueError,'under build'):d.main()
  self.assertFalse(target.exists())
if __name__=='__main__':unittest.main()
