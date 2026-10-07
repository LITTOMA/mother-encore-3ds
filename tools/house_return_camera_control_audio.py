#!/usr/bin/env python3
"""Actual complete House native audio metadata for the retained SceneAudio owner."""
import json,sys,hashlib,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import read,write,sha,decode,require,PIN
from tools import field_scene_audio as codec
IR=ROOT/'content/native-house-return-camera-control-audio.json'
PACK=ROOT/'romfs/data/house-return-camera-control.encnativeaudio'
REVIEW=ROOT/'reports/house-return-camera-control/audio-source-review.json'
def derive():
 native=ROOT/'reports/cloud-world/house-exact.json';d=read(native);tree=read(ROOT/'content/native-house-node-tree.json');base=read(ROOT/'content/podunk-scene-audio.json');camera=read(ROOT/'content/native-house-return-camera-control.json')
 require(d['source']=='res://'+tree['scene']and d['godot']['string']=='3.6.2-stable (official)'and tree['commit']==camera['commit']==PIN and len(d['nodes'])==497,'House audio complete source export differs')
 resources={x['id']:x for x in d['resources']};states={x['source']:x for x in d['scene_states']};serialized={}
 def append(scene,prefix,stack):
  require(scene in states and scene not in stack,'Unknown/cyclic House source SceneState')
  for n in states[scene]['nodes']:
   relative=n['path'].removeprefix('./');path=prefix if relative=='.'else (prefix+'/'if prefix else '')+relative
   instance=decode(n['instance'])
   if instance is not None:
    resource=resources[instance['id']];require(resource['class']=='PackedScene','Source instance is not actual PackedScene');append(resource['path'],path,stack+[scene])
   serialized.setdefault(path,{}).update(decode(n['properties']))
 append(d['source'],'',[])
 banks=[read(ROOT/'content/native-audio.json'),read(ROOT/'content/podunk-bundle-audio.json')];sources=dict(tree['sources']);streams=[];actual_streams={};seen=set()
 def stream(source,native_class):
  if source in actual_streams:return actual_streams[source]
  matches=[(i+1,a)for i,b in enumerate(banks)for a in b['assets']if a['source_path']=='res://'+source]
  require(len(matches)==1,'House audio requires one explicit checked asset: '+source);bank,a=matches[0]
  require(a['source_sha256']==sha(ROOT/'upstream/MOTHER-Encore'/source),'Audio source fingerprint changed')
  sid=a['stable_id'];require(sid not in seen,'Audio stable asset identity collision');seen.add(sid)
  sources[source]=a['source_sha256'];sources[source+'.import']=sha(ROOT/'upstream/MOTHER-Encore'/(source+'.import'))
  streams.append(dict(id=sid,asset_id=sid,bank=bank,source=source,native_class=native_class,source_sha256=a['source_sha256']));actual_streams[source]=sid;return sid
 for r in d['resources']:
  if r['class'].startswith('AudioStream'):
   require(r['class']in('AudioStreamSample','AudioStreamMP3'),'Unknown actual audio Resource');stream(r['path'][6:],r['class'])
 # This stream is genuinely loaded by roomshaker.gd Ready, not by the stripped
 # native export. Metadata admission does not construct the Resource early.
 stream(camera['sound'],'AudioStreamSample')
 nodes=[];by_path={x['node']:x for x in tree['records']}
 for n in d['nodes']:
  if n['class']not in('AudioStreamPlayer','AudioStreamPlayer2D'):continue
  p=decode(n['properties']);original=serialized[n['path']];kind=1 if n['class']=='AudioStreamPlayer'else 2
  require(p['script']is None and by_path[n['path']]['script']==''and p['process_priority']==p['pause_mode']==p['physics_interpolation_mode']==0 and not p['unique_name_in_owner'],'House native Audio source is unmapped')
  source_stream=p['stream'];sid=0 if source_stream is None else actual_streams[resources[source_stream['id']]['path'][6:]]
  # get_bus() in the stripped sandbox falls back to Master without the original
  # bus layout. SceneState retains the actual serialized assignment unchanged.
  bus=original.get('bus',p['bus']);require(isinstance(bus,str)and bus,'Actual source bus missing')
  for field in ['volume_db','pitch_scale','autoplay','stream_paused','mix_target','max_distance','attenuation','panning_strength','area_mask']:
   if field in original:require(original[field]==p[field],'Unreviewed native-vs-serialized audio field '+field)
  row=dict(id=by_path[n['path']]['id'],path=n['path'],kind=kind,stream=sid,bus=bus,volume_db=p['volume_db'],pitch=p['pitch_scale'],autoplay=p['autoplay'],paused=p['stream_paused'],mix_target=p.get('mix_target',0),max_distance=p.get('max_distance',0),attenuation=p.get('attenuation',0),panning=p.get('panning_strength',0),area_mask=p.get('area_mask',0))
  require(not row['autoplay']and not row['paused']and row['mix_target']==0 and row['pitch']>0,'Unknown House audio native property');nodes.append(row)
 require({n['id']for n in nodes}=={n['id']for n in tree['records']if tree['classes'][n['class_index']]in('AudioStreamPlayer','AudioStreamPlayer2D')} and len(streams)==len([r for r in d['resources']if r['class'].startswith('AudioStream')])+1,'Complete House audio scope changed')
 for p,h in sources.items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h,'House audio actual source closure stale '+p)
 return dict(schema=1,format=1,family=codec.FAMILY,capability=1,rules=1,commit=PIN,scene=tree['scene'],scene_id=tree['scene_id'],source_sha256=tree['source_sha256'],tree_ir_sha256=sha(ROOT/'content/native-house-node-tree.json'),native_sha256=sha(native),producer_sha256=sha(Path(__file__)),codec_sha256=sha(ROOT/'tools/field_scene_audio.py'),sources=dict(sorted(sources.items())),scene_bank=base['scene_bank'],bank_sha256=sha(ROOT/'romfs'/base['scene_bank']),global_panning=base['global_panning'],engine=base['engine'],nodes=nodes,streams=streams,admission_ready=False,semantics=['Complete actual native House audio closure, including the RoomShaker Ready-loaded stream','Source SceneState restores serialized bus assignments; no sandbox Master fallback substituted','Stable stream identity is the existing checked AudioAsset ID, separate from runtime Resource/ObjectIDs','Reuse original immutable audio banks/PCM; no copied samples, extra AudioServer, channel owner or clock','RoomShaker Ready constructs its actual AudioStreamSample Resource lazily before same native set_stream/play'],unverified=['Manual cases not run','No SDK build or hardware run'])
def encode(d):return codec.encode(d,IR)
def bundle_context(d):return dict(scene=d['scene'],scene_id=d['scene_id'],source_sha256=d['source_sha256'])
def stage_files(root):
 d=read(IR);b=encode(d);p=Path('data/house-return-camera-control.encnativeaudio');require((Path(root)/p).read_bytes()==b,'House native audio staged pack differs');return{p:b}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);a=p.parse_args();d=derive()
 if a.action=='extract':write(IR,d)
 else:require(read(IR)==d,'House native audio reviewed IR stale')
 write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),sources=d['sources'],native_sha256=d['native_sha256'],bank_sha256=d['bank_sha256'],semantics=d['semantics'],unverified=d['unverified']))
 PACK.parent.mkdir(parents=True,exist_ok=True);b=encode(d);PACK.write_bytes(b);print('Actual House native audio:'+str(len(d['nodes']))+' nodes,'+str(len(d['streams']))+' streams,'+str(len(b))+' bytes; existing PCM/owner reused')
if __name__=='__main__':
 try:main()
 except(ValueError,OSError,KeyError,TypeError)as e:sys.exit('HOUSE AUDIO: '+str(e))
