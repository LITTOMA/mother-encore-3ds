"""The formal source composition must use the complete reviewed programme."""
import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_story,graveyard_shovel_programme as shovel,graveyard_shovel,podunk_field as field,native_content as nc

class FormalShovelSource(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.policy,cls.source=field_story.read();cls.recipe=field.load_recipe();cls.model=field.Model(cls.recipe)
  cls.room=nc.parse_pack(field.build_room(cls.recipe,cls.model,cls.recipe['house']['room_ir']))
 def test_complete_formal_room_has_real_delete_and_source_actor_domain(self):
  room=self.room;self.assertEqual((room['rules'],room['capabilities'],len(room['sections']['Program'])),(7,14,10))
  row=room['sections']['Program'][7];self.assertEqual(room['strings'][row['source_path_string']],'Podunk/cutscenes/gravedigger_getshovel');self.assertEqual((row['command_count'],row['phrase_count']),(68,10))
  commands=room['sections']['Command'][row['first_command']:row['first_command']+row['command_count']]
  deletes=[c for c in commands if c['opcode']==49];self.assertEqual(len(deletes),1);self.assertEqual(room['strings'][deletes[0]['target_index']],'Objects/Items/Item')
  self.assertEqual([c['actor_index']for c in commands if c['opcode']==nc.OPCODES.index('RestoreActor')],[0]);self.assertEqual([c['actor_index']for c in commands if c['opcode']==nc.OPCODES.index('EraseActor')],[3]);self.assertFalse(any(c['opcode']==48 for c in commands))
 def test_formal_metadata_contains_source_boundary_and_both_locales(self):
  blob=field_story.build();_,sections=field.unpack_container(blob,field_story.MAGIC,(1,24,36,40,12,32,1,52),128*1024,capability=5)
  self.assertEqual(len(sections[1])//24,10);self.assertEqual(len(sections[2])//36,70)
  rows=shovel.source_texts();self.assertEqual(len(rows),10);self.assertEqual({r['id']for r in rows},set(range(3200,3205)));self.assertEqual({r['locale']for r in rows},{'en','zh_Hans_CN'});self.assertIn(b'Cutscenes/Cutscene Area7',sections[0])
 def test_v5_source_reader_preserved_without_shovel_and_unknown_paths_reject(self):
  with tempfile.TemporaryDirectory(prefix='formal-shovel-',dir=field.ROOT/'build')as folder:
   path=Path(folder)/'source.json';legacy=copy.deepcopy(self.policy);legacy['schema']=5;legacy.pop('shovel');legacy.pop('prop_dialogues');path.write_text(json.dumps(legacy),encoding='utf8')
   with patch.object(field_story,'POLICY',path):self.assertEqual(field_story.read()[0],legacy)
   for key,value in [('schema',8),('schema',True),('schema',5),('shovel',True),('shovel',''),('shovel','../escape.json'),('shovel','content/../escape.json'),('shovel','content\\escape.json'),('shovel','C:/escape.json'),('shovel','content/podunk-story.json')]:
    candidate=copy.deepcopy(self.policy);candidate[key]=value;path.write_text(json.dumps(candidate),encoding='utf8')
    with self.subTest(key=key,value=value),patch.object(field_story,'POLICY',path),self.assertRaises(ValueError):field_story.read()
 def test_composition_rejects_wrong_scene_or_duplicate_dialogue_namespace(self):
  policy,source=graveyard_shovel.read();room=copy.deepcopy(self.room)
  for key,value in [('scene',3),('dialogue_first_id',3100)]:
   bad=copy.deepcopy(policy);bad[key]=value;sections=copy.deepcopy(room['sections']);strings=list(room['strings'])
   def S(text):
    if text not in strings:strings.append(text)
    return strings.index(text)
   with self.subTest(key=key),patch.object(shovel.review,'read',return_value=(bad,source)),self.assertRaises(ValueError):shovel.add_room(graveyard_shovel.RECIPE,2,sections,strings,S)

if __name__=='__main__':unittest.main()
