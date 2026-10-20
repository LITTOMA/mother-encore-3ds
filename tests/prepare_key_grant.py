"""Typed key-grant execution fixture, NOT a playable Iggy programme.

Uses the exact source grant and its later completion flag. The published field
packet remains the immutable fixture base; a separate compiler owns the new
candidate. No pack or manifest is modified by hand.
"""
import argparse,copy,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import native_content as c
from tools.extract_battle_entry import Extractor,PIN,require
from tools.programme_lowering_recipe import document
from tools.graveyard_tutorial import read as review

def build():
 policy,ex=review()
 require(type(policy['schema'])is int and policy['schema']==1 and policy['commit']==PIN and policy['kind']=='encore.graveyard-tutorial.reviewed-source','Key grant review identity')
 for name,digest in policy['sources'].items():require(hashlib.sha256(ex.data(name)).hexdigest()==digest,'Key grant source changed: '+name)
 source=policy['source'];doc=document(policy,source,ex.yaml(source));require(len(policy['item_grants'])==1,'Typed key-grant fixture scope')
 grant=policy['item_grants'][0];require(doc[grant['label']]['item']==grant['item']and doc[grant['completion_label']]['setflags']==grant['completion_flag']and list(doc).index(grant['label'])<list(doc).index(grant['completion_label']),'Source grant/flag ordering')
 require('setflags'not in doc[grant['label']]and ex.yaml('Data/Items/'+grant['item']+'.yaml')['keyitem']is True and grant['doses']==1,'Source KEY item semantics')
 ir=c.parse_pack((ROOT/'tests/fixtures/pre-key-grant-room.encroom').read_bytes());ir=copy.deepcopy(ir);require(ir['scene_id']==policy['scene'],'Typed key fixture source scene differs');strings=ir['strings'];sections=ir['sections'];ir['capabilities']=13
 def text(s):
  if s not in strings:strings.append(s)
  return strings.index(s)
 flags={strings[f['name_string']]:i for i,f in enumerate(sections['Flag'])};require(grant['completion_flag']in flags,'Source completion flag missing')
 first=len(sections['Command']);commands=[]
 def emit(name,phrase=0,target=c.NONE,value=0,duration=0):commands.append(dict(opcode=c.OPCODES.index(name),actor_index=c.NO_ACTOR,phrase=phrase,target_index=target,auxiliary_index=c.NONE,flags=0,vector=[0,0],value=value,duration=duration))
 emit('BeginCutscene');emit('GrantKeyItem',target=text(grant['item']),value=grant['doses']);emit('YieldIdle');emit('SetFlag',phrase=1,target=flags[grant['completion_flag']],value=1);emit('CutsceneEnded',phrase=1);emit('DialogueDone',phrase=1,duration=.5)
 sections['Command'].extend(commands);programme=len(sections['Program']);sections['Program'].append(dict(stable_id=max(p['stable_id']for p in sections['Program'])+1,first_command=first,command_count=len(commands),phrase_count=2,source_path_string=text('fixture/ordered-source-key-grant')))
 # Extra reviewed source identities supplement, never erase, the base receipt.
 ir.pop('build_input_sha256')
 ir['provenance']={'sources':{**{'upstream/MOTHER-Encore/'+p:h for p,h in ex.sources.items()},**{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in ['tests/fixtures/pre-key-grant-room.encroom','content/graveyard-tutorial.json','tests/prepare_key_grant.py','tools/graveyard_tutorial.py']}}}
 packet,_=c.compile_ir(ir);c.parse_pack(packet)
 return packet,dict(scope=__doc__,source=source,source_sha256=policy['sources'][source],grant=grant,programme=programme,flag_index=flags[grant['completion_flag']],base_sha256=hashlib.sha256((ROOT/'tests/fixtures/pre-key-grant-room.encroom').read_bytes()).hexdigest())

if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Owned key-grant fixture directory required');out.mkdir(parents=True,exist_ok=True);packet,proof=build();(out/'source.encroom').write_bytes(packet);(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Typed source key grant fixture:',len(packet),'bytes; no playable tutorial claim')
