import copy,json,struct,tempfile,unittest
from pathlib import Path
from tools import graveyard_story as story,native_content as nc

ROOT=Path(__file__).resolve().parents[1]
POLICY=ROOT/'content/graveyard-story.json'

class GraveyardStoryTests(unittest.TestCase):
 def test_reviewed_owner_graph_text_and_explicit_boundary(self):
  p,ex,owner,selections=story.read(POLICY)
  self.assertEqual(owner['binding']['id'],4)
  self.assertEqual(selections,[['','Podunk/iggy_forget'],['got_shovel','Podunk/iggy_return_shovel']])
  room=nc.parse_pack((ROOT/'romfs/world/house/opening.encroom').read_bytes())
  flags={room['strings'][r['name_string']]:i for i,r in enumerate(room['sections']['Flag'])}
  _,programmes,texts,rules,choices=story.payload(POLICY,flags,4)
  self.assertEqual(len(programmes),1);self.assertEqual(len(texts),6)
  self.assertEqual({r['id']for r in texts},{3000,3001,3002})
  self.assertEqual([(r['actor'],r['programme'],r['status'])for r in rules],[(3,4,0),(3,0xffffffff,1)])
  self.assertTrue(any('[ui_cancel]'in r['body']for r in texts))
  self.assertTrue(all(r['voice']=='Audio/Sound effects/text/Adult.mp3'for r in texts))
  self.assertEqual(choices[:8],b'ENCCHOIC')
 def test_unknown_schema_owner_source_and_document_fail_closed(self):
  original=json.loads(POLICY.read_text(encoding='utf8'))
  mutations=[lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(scene_id=False),lambda p:p.update(npc_path='Objects/NPCS/npc19'),lambda p:p.update(dialogue_first_id=True),lambda p:p.update(ignore=True),lambda p:p.update(locales=['en']),lambda p:p.update(programmes=p['programmes']*2),lambda p:p.update(unavailable=[]),lambda p:p['sources'].update({'Scripts/Main/npc.gd':'0'*64}),lambda p:p['documents'][p['programmes'][0]]['0'].update(wait=1)]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'recipe.json'
   for mutate in mutations:
    with self.subTest(mutation=mutate):
     p=copy.deepcopy(original);mutate(p);path.write_text(json.dumps(p),encoding='utf8')
     with self.assertRaises(ValueError):story.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):story.read(path)
 def test_unknown_markup_and_conditional_fail_closed(self):
  source='[@][color][ui_cancel][/color] [if input:gamepad]button[else]key[/if].[WAIT@]Try!'
  self.assertEqual(story.checked_text(source),source)
  for text in ('[unknown]','[ui_accept]','[if input:keyboard]x[else]y[/if]','[if input:gamepad]x[/if]','[if input:gamepad]x[else][Ninten][/if]','[if input:gamepad][if input:gamepad]x[else]y[/if][else]z[/if]','[else]','[/if]','[','x]',''):
   with self.subTest(text=text),self.assertRaises(ValueError):story.checked_text(text)

if __name__=='__main__':unittest.main()
