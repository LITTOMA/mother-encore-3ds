"""Join the original sound owners and PCM bank to ENCDMP01 capability2.

Source-node requests remain separate from audio-thread mixing and actual
audibility. Capability1 geometry remains readable; saves are not changed.
"""
import argparse,hashlib,json,re,struct,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_mechanism_presentation as presentation,dungeon_mechanism_audio as audio,audio_asset
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,parse_project_value,node_path
from tools.podunk_field import pack_container,unpack_container
RECIPE=ROOT/'content/catacombs-mechanism-media.json'
DEPS={'presentation':'content/catacombs-mechanism-presentation.json','audio':'content/catacombs-mechanism-audio.json'}

def fields(p,s):require(type(p)is dict and set(p)==set(s.split()),'Mechanism media fields')
def read(file=RECIPE):
 def unique(rows):
  out={}
  for k,v in rows:require(k not in out,'Duplicate mechanism media review field');out[k]=v
  return out
 def invalid(v):raise ValueError('Nonfinite mechanism media review')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid);fields(p,'schema kind commit scope dependencies denied_owner_path manager engine')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-mechanism-media.reviewed-source'and p['commit']==PIN,'Mechanism media schema/pin')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048,'Mechanism media scope');fields(p['dependencies'],'presentation audio')
 for key,path in DEPS.items():
  r=p['dependencies'][key];fields(r,'path sha256');require(r['path']==path and r['sha256']==hashlib.sha256((ROOT/path).read_bytes()).hexdigest(),'Mechanism media reviewed dependency changed')
 presentation.bindings.wall.path(p['denied_owner_path']);manager=p['manager'];fields(manager,'autoload scene channels')
 for v in manager.values():presentation.bindings.wall.path(v)
 require('/'not in manager['autoload']and manager['scene'].endswith('.tscn'),'Mechanism manager source selector')
 engine=p['engine'];fields(engine,'commit source source_sha256 header_source header_sha256 functions review')
 require(engine['commit']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'and engine['source']=='https://github.com/godotengine/godot/blob/'+engine['commit']+'/scene/audio/audio_stream_player.cpp'and engine['header_source']=='https://github.com/godotengine/godot/blob/'+engine['commit']+'/scene/audio/audio_stream_player.h','Unreviewed mechanism audio engine source/version')
 require(type(engine['functions'])is dict and set(engine['functions'])=={'void AudioStreamPlayer::play(','void AudioStreamPlayer::stop(','void AudioStreamPlayer::_set_playing('}and type(engine['review'])is str and 0<len(engine['review'])<=2048,'Mechanism original audio method review coverage')
 for v in [engine['source_sha256'],engine['header_sha256'],*engine['functions'].values()]:require(type(v)is str and re.fullmatch('[0-9a-f]{64}',v)and int(v,16),'Mechanism original audio review digest')
 return p

def parse_sounds(rows,bank_bytes,nested,owners,text):
 require(0<len(rows)<=257*88 and 0<len(bank_bytes)<=audio_asset.MAX_META,'Mechanism sound section count');bank=audio_asset.parse_bank(bank_bytes);byasset={a['stable_id']:a for a in bank['assets']};byowner={o['owner']:o for o in owners};profiles={p['id']:p for p in nested['profiles']};switches={n['id']:n for n in nested['graph']['switches']};sound_owners=set();used=set();result=[];denied=0
 for at in range(0,len(rows),88):
  v=struct.unpack_from('<6I32sI2f5I',rows,at);owner,kind=v[:2];path=text(*v[2:4]);source=text(*v[4:6]);digest=v[6].hex();ident=v[7];gain,pitch=v[8:10];bus,pause,ca,cs,reserved=v[10:];channel=text(ca,cs)if cs else'';require(cs or ca==0,'Mechanism empty channel span')
  require(owner not in sound_owners and kind in(1,2)and source.startswith('Audio/')and any(v[6])and ident in byasset and bus==1 and pause==0 and not reserved,'Mechanism sound owner/type/source/policy');sound_owners.add(owner);used.add(ident);presentation.bindings.wall.real(gain,-120,24);presentation.bindings.wall.real(pitch,0.01,4)
  asset=byasset[ident];require(asset['source_path']=='res://'+source and asset['source_sha256']==digest and not asset['loop'],'Mechanism sound bank source/loop differs')
  if kind==2:
   require(owner==0 and source==nested['graph']['denied']and channel==nested['graph']['channel'],'Mechanism denial channel/source binding');denied+=1
  else:
   require(owner in byowner and'/'in path and path.rsplit('/',1)[0]==byowner[owner]['path']and not channel,'Mechanism source-node sound binding');profile=profiles[byowner[owner]['profile']]
   if profile['family']==4:
    w=profile['wall']['audio'];require((ident,'res://'+source,digest,gain,pitch,bus)==(w['id'],w['path'],w['sha'],w['gain'],w['pitch'],w['bus']),'Mechanism wall clock/sound binding differs')
   else:require(owner in switches and source==switches[owner]['audio'],'Mechanism switch clock/sound binding differs')
  result.append(dict(owner=owner,kind=kind,path=path,source=source,source_sha256=digest,stable_id=ident,gain_db=gain,pitch_scale=pitch,bus=bus,pause_mode=pause,channel=channel))
 required={o['owner']for o in owners if profiles[o['profile']]['family']!=3}|{0};require(sound_owners==required and denied==1 and used==set(byasset),'Mechanism missing/unowned audio source/owner')
 return dict(sounds=result,audio_bank=bytes(bank_bytes))

def encode(geometry,sounds,bank):
 scene,parts=unpack_container(geometry,presentation.MAGIC,presentation.STRIDES,presentation.LIMIT);strings=bytearray(parts[0]);rows=bytearray()
 def text(value):
  if not value:return 0,0
  raw=value.encode('ascii');at=len(strings);strings.extend(raw);return at,len(raw)
 for row in sounds:rows.extend(struct.pack('<6I32sI2f5I',row['owner'],row['kind'],*text(row['path']),*text(row['source']),bytes.fromhex(row['source_sha256']),row['stable_id'],row['gain_db'],row['pitch_scale'],row['bus'],row['pause_mode'],*text(row['channel']),0))
 blob=pack_container(presentation.MAGIC,scene,[strings,*parts[1:],rows,bank],presentation.MEDIA_STRIDES,presentation.LIMIT,capability=2);presentation.parse(blob);return blob

def build(out,file=RECIPE):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build','Mechanism media candidates stay under build');p=read(file);model,geometry,gproof=presentation.extract();recipe,aproof=audio.extract();ex=Extractor(ROOT)
 selector=p['manager'];manager=Document(selector['scene'],ex.text(selector['scene']));nodes={node_path(n.attrs):n for n in manager.nodes};require(parse_project_value(ex.text('project.godot'),'autoload',selector['autoload'])=='*res://'+selector['scene']and selector['channels']in nodes and nodes[selector['channels']].attrs.get('type')=='Node'and nodes['.'].attrs['name']==selector['autoload']and manager.script_identity(nodes['.'].props.get('script'))==recipe['manager_source']and p['denied_owner_path']==nodes['.'].attrs['name']+'/'+selector['channels'],'Mechanism shared audioManager ownership differs')
 with tempfile.TemporaryDirectory(prefix='mechanism-media-audio-',dir=ROOT/'build')as temporary:
  temporary=Path(temporary).resolve();require(temporary.is_relative_to((ROOT/'build').resolve()),'Mechanism temporary audio ownership');assets=temporary/'assets';audio_proof=audio.compile_assets(assets);bank=(assets/audio_proof['bank']['path']).read_bytes();parsed=audio_asset.parse_bank(bank);by_source={a['source_path']:a for a in parsed['assets']};sounds=[]
  for owner in aproof['owners']:
   asset=by_source[owner['source_path']];sounds.append(dict(owner=owner['owner'],kind=1,path=owner['path'],source=owner['source_path'][6:],source_sha256=asset['source_sha256'],stable_id=asset['stable_id'],gain_db=owner['gain_db'],pitch_scale=owner['pitch_scale'],bus=owner['bus'],pause_mode=owner['pause_mode'],channel=''))
  denied=aproof['denied'];asset=by_source[denied['source_path']];sounds.append(dict(owner=0,kind=2,path=p['denied_owner_path'],source=denied['source_path'][6:],source_sha256=asset['source_sha256'],stable_id=asset['stable_id'],gain_db=0,pitch_scale=1,bus=1,pause_mode=0,channel=denied['channel']))
  blob=encode(geometry,sounds,bank);out.mkdir(parents=True,exist_ok=True)
  for row in audio_proof['files']:
   data=(assets/row['path']).read_bytes();require(len(data)==row['bytes']and hashlib.sha256(data).hexdigest()==row['sha256'],'Mechanism candidate PCM changed before commit');target=out/row['path'];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 (out/'source.encdmp').write_bytes(blob);proof=dict(commit=PIN,scope=p['scope'],review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),adapter_sha256={Path(mod.__file__).name:hashlib.sha256(Path(mod.__file__).read_bytes()).hexdigest()for mod in(presentation,audio,audio_asset)},presentation=gproof,audio=audio_proof,source_ownership=ex.sources,engine=p['engine'],sounds=sounds,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),live_output_admission=False)
 (out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Mechanism media capability2:',len(blob),'bytes,',len(sounds),'sound owners; source-node requests distinct from actual audio mixing/output');return proof

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
