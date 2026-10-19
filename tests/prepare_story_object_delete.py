"""Original getshovel flag/delete ordering fixture, NOT a playable programme.

The source sets got_shovel at0 and queues Objects/Items/Item at3. Only those
mapped side effects are compiled here; no dialogue, actor route or grant is
inferred. Shipped resources are unchanged.
"""
from pathlib import Path
import argparse,copy,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import native_content as c
from tools.extract_battle_entry import Extractor,require

def build():
 ex=Extractor();audit=json.loads((ROOT/'reports/iggy-tutorial/shovel-preparation/shovel-next-source-audit.json').read_bytes());source='Data/Dialogue/Podunk/cutscenes/gravedigger_getshovel.yaml'
 for name in [source,'Scripts/Main/DroppedItem.gd','Scripts/misc/sparkles.gd']:
  require(hashlib.sha256(ex.data(name)).hexdigest()==audit['sources'][name],'Source object deletion changed: '+name)
 lowering=json.loads((ROOT/'content/podunk-story.json').read_bytes());handler='Scripts/UI/DialogueBox.gd';raw_handler=ex.text(handler)
 require(hashlib.sha256(ex.data(handler)).hexdigest()==lowering['sources'][handler]and 'object.call_deferred(objects[i])'in raw_handler,'Original object method is deferred')
 doc=ex.yaml(source);require(len(doc)==10 and doc['0']['setflags']=='got_shovel'and doc['3']['objectsfunction']=={'Objects/Items/Item':'queue_free'},'Original object/flag ordering differs')
 require(not any('item'in row for row in doc.values()),'No item grant is mapped in getshovel')
 base=ROOT/'romfs/world/podunk/podunk.encroom';ir=copy.deepcopy(c.parse_pack(base.read_bytes()));require(ir['scene_id']==2 and ir['capabilities']==13,'Expected published scene base');ir.pop('build_input_sha256');ir['capabilities']=14;strings=ir['strings'];s=ir['sections']
 def text(value):
  if value not in strings:strings.append(value)
  return strings.index(value)
 flags={strings[f['name_string']]:i for i,f in enumerate(s['Flag'])};require('got_shovel'in flags,'Original object flag is undeclared')
 first=len(s['Command']);commands=[]
 def emit(op,phrase=0,target=c.NONE,value=0,duration=0):commands.append(dict(opcode=c.OPCODES.index(op),actor_index=c.NO_ACTOR,phrase=phrase,target_index=target,auxiliary_index=c.NONE,flags=0,vector=[0,0],value=value,duration=duration))
 emit('BeginCutscene');emit('SetFlag',target=flags['got_shovel'],value=1);emit('YieldIdle');emit('QueueFreeStoryObject',phrase=3,target=text('Objects/Items/Item'));emit('YieldIdle',phrase=3);emit('CutsceneEnded',phrase=3);emit('DialogueDone',phrase=3,duration=.5)
 programme=len(s['Program']);s['Command'].extend(commands);s['Program'].append(dict(stable_id=max(x['stable_id']for x in s['Program'])+1,first_command=first,command_count=len(commands),phrase_count=4,source_path_string=text('fixture/ordered-source-object-deletion')))
 ir['provenance']={'sources':{**{'upstream/MOTHER-Encore/'+n:h for n,h in ex.sources.items()},**{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in ['romfs/world/podunk/podunk.encroom','reports/iggy-tutorial/shovel-preparation/shovel-next-source-audit.json','content/podunk-story.json','tests/prepare_story_object_delete.py','tools/native_content.py']}}}
 blob,_=c.compile_ir(ir);c.parse_pack(blob)
 return blob,dict(scope=__doc__,source=source,source_sha256=ex.sources[source],flag_label='0',delete_label='3',path='Objects/Items/Item',programme=programme,base_sha256=hashlib.sha256(base.read_bytes()).hexdigest())
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve();require(out!=ROOT/'build'and(ROOT/'build').resolve()in out.parents,'Owned fixture directory required');out.mkdir(parents=True,exist_ok=True);blob,proof=build();(out/'source.encroom').write_bytes(blob);(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Source object-delete component fixture:',len(blob),'bytes; not full getshovel')
