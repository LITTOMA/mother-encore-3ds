"""Extend the checked action packet with its real source sound and SFX node.

ENCSWA01 format1 capability2 retains all capability1 policy/directions/strings.
The added SHA joins source paths across packs; gain, pitch, bus and pause mode
remain binary data. Engine defaults are reviewed against official 3.6.2
AudioStreamPlayer constructor (3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8).
"""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing_action as action,field_swing,podunk_field as f
from tools.godot_text import Document,ExtRef
from tools.extract_battle_entry import require
RECIPE=ROOT/'content/field-player-audio.json';STRIDES=(*action.STRIDES,48)
def unique(pairs):
 out={}
 for key,value in pairs:require(key not in out,'Duplicate player audio binding');out[key]=value
 return out
def read(path=RECIPE):
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(p)is dict and set(p)==set('schema kind action_recipe source_node bus bus_kind pause_mode gain_db pitch autoplay output'.split()),'Player audio binding fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.player-audio.reviewed-bindings'and p['action_recipe']=='content/field-swing-action.json'and p['source_node']=='AudioStreamPlayer','Player audio binding schema/source')
 require(p['bus']=='SFX'and type(p['bus_kind'])is int and p['bus_kind']==1 and type(p['pause_mode'])is int and p['pause_mode']==2 and p['autoplay']is False,'Player audio bus/process binding')
 require(type(p['gain_db'])in(int,float)and math.isfinite(p['gain_db'])and -120<=p['gain_db']<=24 and type(p['pitch'])in(int,float)and math.isfinite(p['pitch'])and 0<p['pitch']<=4,'Player audio gain/pitch binding')
 require(type(p['output'])is str and re.fullmatch('world/[a-z0-9_/-]+\.encaction',p['output'])and '..'not in p['output']and '//'not in p['output'],'Player audio output binding')
 return p
def source(path=RECIPE):
 p=read(path);base,proof=action.source(ROOT/p['action_recipe']);recipe,ex=field_swing.read();doc=Document(recipe['scene'],ex.text(recipe['scene']));nodes=[n for n in doc.nodes if n.attrs['name']==p['source_node']]
 require(len(nodes)==1 and nodes[0].attrs.get('type')=='AudioStreamPlayer'and nodes[0].attrs.get('parent')=='.','Player audio source node identity');n=nodes[0]
 require(set(n.props)<={'pause_mode','stream','bus','volume_db','pitch_scale','autoplay'}and isinstance(n.props.get('stream'),ExtRef),'Player audio unknown node policy')
 require(type(n.props.get('pause_mode',0))is int and type(n.props.get('bus','Master'))is str and all(type(n.props.get(k,v))in(int,float)and math.isfinite(n.props.get(k,v))for k,v in [('volume_db',0),('pitch_scale',1)]),'Player audio source property types')
 require(n.props.get('pause_mode',0)==p['pause_mode']and n.props.get('bus','Master')==p['bus']and n.props.get('volume_db',0)==p['gain_db']and n.props.get('pitch_scale',1)==p['pitch']and n.props.get('autoplay',False)is p['autoplay'],'Player audio original node/defaults differ')
 digest=hashlib.sha256(ex.data(recipe['sound'])).hexdigest();model=dict(action=base,audio=dict(source_sha256=digest,gain_db=p['gain_db'],pitch=p['pitch'],bus=p['bus_kind'],pause_mode=p['pause_mode']))
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();proof.update(scope=__doc__,sources={**proof['sources'],**ex.sources},audio_source=recipe['sound'],node_properties=repr(n.props),engine_defaults=dict(volume_db=0,pitch_scale=1,autoplay=False,source='https://github.com/godotengine/godot/blob/3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8/scene/audio/audio_stream_player.cpp'),dependencies={**proof.get('dependencies',{}),Path(path).resolve().relative_to(ROOT).as_posix():sha(Path(path)),'tools/field_player_audio.py':sha(Path(__file__))})
 return model,proof
def encode(model):
 require(type(model)is dict and set(model)=={'action','audio'},'Player audio model fields');a=model['audio'];require(type(a)is dict and set(a)=={'source_sha256','gain_db','pitch','bus','pause_mode'},'Player audio record fields')
 require(type(a['source_sha256'])is str and re.fullmatch('[0-9a-f]{64}',a['source_sha256'])and a['source_sha256']!='0'*64,'Player audio source digest')
 require(type(a['gain_db'])in(int,float)and math.isfinite(a['gain_db'])and -120<=a['gain_db']<=24 and type(a['pitch'])in(int,float)and math.isfinite(a['pitch'])and 0<a['pitch']<=4 and type(a['bus'])is int and a['bus']==1 and type(a['pause_mode'])is int and a['pause_mode']==2,'Player audio record policy')
 _,parts=f.unpack_container(action.encode(model['action']),action.MAGIC,action.STRIDES,action.LIMIT);record=struct.pack('<32s2f2I',bytes.fromhex(a['source_sha256']),a['gain_db'],a['pitch'],a['bus'],a['pause_mode'])
 return f.pack_container(action.MAGIC,0,[*parts,record],STRIDES,action.LIMIT,capability=2)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);o=p.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh owned player audio candidate required');model,proof=source();blob=encode(model);out.mkdir(parents=True);(out/'swing.encaction').write_bytes(blob);proof.update(model=model,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest());(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Player audio action:',len(blob),'bytes; source SHA/SFX/gain/pitch/process metadata; candidate only')
