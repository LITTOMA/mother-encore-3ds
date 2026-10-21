"""Checked original Catacombs loop music and rectangles;existing codecs only.

This does not import the complete scene or admit its root,actors or save state.
"""
import argparse,copy,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,Vec2,node_path
from tools import field_scene_music as codec,audio_asset,podunk_music
from tools.dungeon_key_indicator import unique
from tools.dungeon_keys import fields,path as safe
from tools.run_dungeon_key_contact_reference import player_source
RECIPE=ROOT/'content/catacombs-music.json';CHANGER='Nodes/Overworld/MusicChanger.tscn';MANAGER='Scripts/global/audioManager.gd'

def read(file=RECIPE):
 def invalid(v):raise ValueError('Nonfinite Catacombs music JSON')
 p=json.loads(Path(file).read_text('utf-8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scope scene areas sources output pcm_prefix license_review','Catacombs music review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.catacombs-music.reviewed-source'and p['commit']==PIN,'Catacombs music review identity')
 safe(p['scene']);require(all(type(p[k])is str and 0<len(p[k])<=4096 for k in ['scope','license_review']),'Catacombs music scope/license')
 require(type(p['areas'])is list and 1<=len(p['areas'])<=16,'Catacombs music area count');ids=set();paths=set()
 for row in p['areas']:
  fields(row,'path id','music area');safe(row['path']);require(row['path']not in paths and type(row['id'])is int and 0<row['id']<2**32 and row['id']not in ids and row['id']==podunk_music.stable(p['scene']+':'+row['path']),'Catacombs music stable area identity');ids.add(row['id']);paths.add(row['path'])
 fields(p['output'],'binding zones audio','music outputs')
 for key,ext in [('binding','.encmusic'),('zones','.enczones'),('audio','.encaudio')]:safe(p['output'][key]);require(p['output'][key].startswith('sound/banks/')and p['output'][key].endswith(ext),'Catacombs music output binding')
 require(type(p['pcm_prefix'])is str and re.fullmatch('[a-z][a-z0-9-]{0,31}',p['pcm_prefix']),'Catacombs music PCM owner')
 require(type(p['sources'])is dict and {p['scene'],CHANGER,MANAGER,'Scripts/global/globalData.gd','Nodes/Reusables/Player.tscn','project.godot','LICENSE'}<=set(p['sources']),'Catacombs music source coverage');ex=Extractor(ROOT)
 for name,digest in p['sources'].items():safe(name);require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(ex.data(name)).hexdigest()==digest,'Catacombs music source changed:'+name)
 return p,ex

def source(file=RECIPE):
 p,ex=read(file);doc=Document(p['scene'],ex.text(p['scene']));nodes={node_path(n.attrs):n for n in doc.nodes};root=nodes['.'];parent=nodes.get('Music')
 require(root.attrs.get('type')=='Node2D'and set(root.props)=={'script','_region_name'}and parent is not None and parent.attrs.get('type')=='Node2D'and parent.props=={'visible':False},'Catacombs music canvas ancestry changed')
 area_nodes=[n for n in doc.nodes if n.attrs.get('parent')=='Music'];require([node_path(n.attrs) for n in area_nodes]==[v['path']for v in p['areas']],'Catacombs music must cover all original areas in source order')
 changer=Document(CHANGER,ex.text(CHANGER));base=changer.nodes[0];require(base.attrs.get('type')=='Area2D'and set(base.props)=={'collision_layer','collision_mask','monitorable','script','__meta__'}and base.props['collision_layer']==4096 and base.props['collision_mask']==0 and base.props['monitorable']is False,'Catacombs MusicChanger class/filter changed')
 script=changer.sub_resource(base.props['script'])['script/source'];defaults={}
 for name in ['music','loop','appear_flag','disappear_flag','diegetic','volume_db','fadein_length','fadeout_length','disabled']:
  match=re.search(r'^export(?: [(][^)]*[)])? var '+name+r' = (.+)$',script,re.M);require(match,'Catacombs MusicChanger default missing:'+name);defaults[name]=json.loads(match[1])
 player,player_proof=player_source();require(player['mask']&base.props['collision_layer'],'Catacombs music/player collision filter differs');manager=ex.text(MANAGER);floor=re.search(r'^const SILENT_SOUND_THRESHOLD = (-?[0-9.]+)$',manager,re.M);require(floor and 'func music_fadeto(index, volume = 0, duration = 1):'in manager,'Catacombs music manager policy changed')
 regions=[];tracks=[];polygons=[];imports=[];seen=set()
 for binding,node in zip(p['areas'],area_nodes):
  require(doc.ext_path(node.attrs['instance'],'PackedScene')==CHANGER and set(node.props)<=set(defaults)|{'position'},'Catacombs MusicChanger override changed');values={k:node.props.get(k,v)for k,v in defaults.items()}
  require(values['music']==''and values['diegetic']is False and type(values['loop'])is str and values['loop'],'Catacombs music supports reviewed loop-only non-diegetic areas');safe(values['loop']);stream='res://Audio/Music/'+values['loop'];ident=podunk_music.stable(stream);require(ident,'Catacombs music zero track ID')
  require({stream[6:],stream[6:]+'.import'}<=set(p['sources']),'Catacombs music source/import not reviewed')
  if stream not in seen:
   seen.add(stream);raw=ex.data(stream[6:]);imported=ex.data(stream[6:]+'.import');loop,offset=audio_asset.import_settings(imported,stream);require(loop,'Catacombs music non-loop source requires separate implementation')
   tracks.append(dict(stable_id=ident,source_path=stream,source_sha256=hashlib.sha256(raw).hexdigest(),import_sha256=hashlib.sha256(imported).hexdigest(),pcm_path='sound/music/'+p['pcm_prefix']+'-'+str(ident)+'.pcm',gain_db=0));imports.append(dict(source=stream,loop=loop,offset=offset))
  children=[n for n in doc.nodes if n.attrs.get('parent')==node_path(node.attrs)];require(children,'Catacombs music rectangle missing');shapes=[];parts=[];position=node.props.get('position',Vec2(0,0));require(isinstance(position,Vec2),'Catacombs music area position')
  for child in children:
   require(child.attrs.get('type')=='CollisionShape2D'and set(child.props)<={'position','shape'}and 'shape'in child.props,'Catacombs music rectangle override outside reviewed scope');shape=child.props['shape'];require(doc.sub_type(shape)=='RectangleShape2D'and set(doc.sub_resource(shape))=={'extents'},'Catacombs music non-rectangle requires separate review');extents=doc.sub_resource(shape)['extents'];offset=child.props.get('position',Vec2(0,0));require(isinstance(extents,Vec2)and isinstance(offset,Vec2)and extents.x>0 and extents.y>0,'Catacombs music rectangle geometry')
   center=Vec2(position.x+offset.x,position.y+offset.y);points=[[center.x+x,center.y+y]for x,y in [(-extents.x,-extents.y),(extents.x,-extents.y),(extents.x,extents.y),(-extents.x,extents.y)]];parts.append([[struct.unpack('<f',struct.pack('<f',v))[0]for v in pair]for pair in points]);shapes.append(node_path(child.attrs))
  regions.append(dict(id=binding['id'],source_path=node_path(node.attrs),track_id=ident,volume_db=values['volume_db'],fadein_seconds=values['fadein_length'],fadeout_seconds=values['fadeout_length'],appear_flag=values['appear_flag'],disappear_flag=values['disappear_flag'],disabled=values['disabled'],shape_paths=shapes));polygons.append(dict(source=node_path(node.attrs),polygons=parts))
 metadata=dict(silence_db=float(floor[1]),fade_to_seconds=1,tracks=tracks,regions=regions);codec.validate_metadata(metadata,polygons)
 require("purpose you're using them is related to the game"in ex.text('LICENSE'),'Catacombs music asset permission changed');require(set(ex.sources)<=set(p['sources']),'Catacombs music unreviewed source dependency')
 proof=dict(source_commit=PIN,scope=p['scope'],sources=dict(sorted(ex.sources.items())),review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),imports=imports,metadata=metadata,polygons=polygons,player=player_proof,formats=['ENCMUS01 v1','ENCZON01 v1','ENCAUD01 existing'],mapped_implementation={'binding':'runtime/music_regions.cpp','geometry':'runtime/music_zones.cpp','callbacks':'runtime/music_scene_graph.cpp'},release_staged=False)
 return p,metadata,polygons,proof

def compile(out,file=RECIPE,*,audio=False,ffmpeg='ffmpeg',ffprobe='ffprobe'):
 out=Path(out).resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh Catacombs music candidate under build required');p,metadata,polygons,proof=source(file);binding,zones=codec.encode(metadata,polygons);out.mkdir(parents=True);(out/'.owned-catacombs-music').write_bytes(b'catacombs-music-v1');outputs={}
 def save(name,data):
  target=codec.npc_target(out,name);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);outputs[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
 save(p['output']['binding'],binding);save(p['output']['zones'],zones)
 if audio:
  recipe=json.loads((ROOT/'content/native-audio.json').read_bytes());recipe['assets']=copy.deepcopy(metadata['tracks']);stage=out/'audio-stage';stage.mkdir();(stage/'.owned-catacombs-audio-stage').write_bytes(b'catacombs-music-audio-stage-v1');manifest=audio_asset.compile_assets(recipe,ROOT/'upstream/MOTHER-Encore',stage,ffmpeg=ffmpeg,ffprobe=ffprobe)
  require({r['path']for r in manifest['files']}=={t['pcm_path']for t in metadata['tracks']}|{'sound/banks/opening.encaudio'},'Catacombs audio emitted unexpected content')
  for row in manifest['files']:
   data=(stage/row['path']).read_bytes();require(len(data)==row['size']and hashlib.sha256(data).hexdigest()==row['sha256'],'Catacombs audio fingerprint mismatch');save(p['output']['audio']if row['path']=='sound/banks/opening.encaudio'else row['path'],data)
  proof['audio']=manifest
 proof.update(outputs=outputs,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest());(out/'source.json').write_bytes((json.dumps(proof,indent=2)+'\n').encode());print('Catacombs music candidate:',len(metadata['regions']),'areas,',len(metadata['tracks']),'original loop tracks;',sum(v['bytes']for v in outputs.values()),'bytes;not staged/no scene or audible claim');return binding,zones
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--audio',action='store_true');ap.add_argument('--ffmpeg',default='ffmpeg');ap.add_argument('--ffprobe',default='ffprobe');a=ap.parse_args();compile(a.out,audio=a.audio,ffmpeg=a.ffmpeg,ffprobe=a.ffprobe)
