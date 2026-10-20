"""Original same-animation play/continuation observations on numeric switch copies."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_switch_clips as d
from tools.godot_text import Document
from tools.run_dungeon_switch_clips_reference import numeric,PROBE
from tools.run_scene_reference import invoke

def actions(model):
 out=[]
 for i,clip in enumerate(model['clips']):
  other=model['clips'][(i+1)%len(model['clips'])]
  def play(c):return dict(op='play',name=c['name'],id=c['id'])
  def advance(v):return dict(op='advance',delta=v)
  duration=clip['duration']
  out.extend([play(clip),advance(duration/4),play(clip),advance(0),advance(duration/4),play(clip),advance(0),advance(duration/4),advance(duration),play(clip),advance(0),advance(duration*.4),play(other),advance(0),play(clip),advance(0),advance(duration/4),play(clip),advance(duration),play(clip),advance(0)])
 return out

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and engine.is_file(),'Fresh owned resume reference and actual engine required')
 data,proof=d.extract();review,ex=d.read();project=out/'project';project.mkdir(parents=True);(project/'.owned-prop-resume-reference').write_text('source-prop-resume-reference-v1',encoding='utf8');config=[];originals=out/'source-scenes';originals.mkdir();quarantine={};copied={}
 for row in data:
  profile=row['profile'];raw=ex.data(profile['scene']);(originals/(profile['name']+'.tscn')).write_bytes(raw)
  text,boundary=numeric(raw.decode(),profile);name='numeric-'+profile['name']+'.tscn';(project/name).write_text(text,encoding='utf8');quarantine[profile['scene']]=boundary
  for source,kind in Document(profile['scene'],raw.decode()).ext.values():
   if kind=='Script':continue
   d.require(kind in('Texture','AudioStream'),'Resume unknown external resource');target=project/source;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(source));copied[source]=hashlib.sha256(target.read_bytes()).hexdigest()
  config.append(dict(scene=name,file=profile['name']+'.encpcl',frame_owner=profile['frame_owner'],body_owner=profile['body_owner'],actions=actions(row['model']),ids={c['name']:c['id']for c in row['model']['clips']}))
 (project/'input.json').write_text(json.dumps(dict(profiles=config),indent=2)+'\n',encoding='utf8');(project/'probe.gd').write_text(PROBE,encoding='utf8');(project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original same-animation play"\n[rendering]\nquality/driver/driver_name="GLES2"\n[logging]\nfile_logging/enable_logging=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'engine.log')
 observed=json.loads((project/'observations.json').read_bytes());v=observed['engine'];d.require((v['major'],v['minor'],v['patch'],v['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Resume reference engine identity')
 expected=[(profile,i,action)for profile in config for i,action in enumerate(profile['actions'])];d.require(len(observed['rows'])==len(expected),'Resume reference observations incomplete')
 lines=['file op value frame visible body playing current position completed']
 for row,(profile,index,action)in zip(observed['rows'],expected):
  d.require(row['file']==profile['file']and row['index']==index and row['op']==action['op'],'Resume reference action order');ids=profile['ids'];lines.append(' '.join(map(str,[row['file'],row['op'],action['id']if action['op']=='play'else repr(action['delta']),row['frame'],int(row['visible']),int(row['body']),int(row['playing']),ids[row['current']]if row['current']else 0,row['position'],','.join(str(ids[n])for n in row['completed'])or'-'])))
 for source,digest in copied.items():d.require(hashlib.sha256((project/source).read_bytes()).hexdigest()==digest,'Resume source asset modified')
 (out/'states.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8');(out/'observations.json').write_bytes((project/'observations.json').read_bytes());(out/'input.json').write_bytes((project/'input.json').read_bytes())
 proof.update(scope=__doc__,quarantine=quarantine,rows=len(expected),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),shared_probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),outputs={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in [out/'states.tsv',out/'observations.json',out/'input.json']})
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original play/continuation:',len(expected),'observations; three numeric profiles, all eleven animations; no scene/audio admission')
 return proof
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
