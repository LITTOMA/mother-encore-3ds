#!/usr/bin/env python3
"""Actual three DialogueBox AudioStreamPlayers; immutable source and PCM links."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,decode,require
SCENE='Nodes/Ui/DialogueBox.tscn';ABSTRACT='Scripts/UI/AbstractDialogueBox.gd';DIALOGUE='Scripts/UI/DialogueBox.gd';BUS='Audio/Audio Buses/default_bus_layout.tres'
IR=ROOT/'content/native-field-dialogue-audio.json';REVIEW=ROOT/'reports/field-dialogue-audio/source-review.json';OUT=ROOT/'romfs/data/podunk-dialogue-audio.encdaudio'
def extract(native,engine):
 recipe=read(ROOT/'content/dialogue-node-recipe.json');d=read(native);require(d['source']=='res://'+SCENE and len(d['nodes'])==47 and recipe['native_sha256']==sha(native)and recipe['commit']==PIN,'Dialogue audio exact recipe mismatch')
 nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};records={r['node']:r for r in recipe['records']};state=next(s for s in d['scene_states']if s['source']=='res://'+SCENE);serialized={n['path'].removeprefix('./'):decode(n['properties'])for n in state['nodes']};bank=read(ROOT/'content/native-audio.json');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources={p:recipe['sources'][p]for p in [SCENE,DIALOGUE,ABSTRACT]};sources[BUS]=bank['bus_sha256'];assets=[]
 for a in bank['assets']:
  if not a['source_path'].endswith('.mp3'):continue
  source=a['source_path'][6:];require(a['source_sha256']==inv[source]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/source),'Audio source bank mismatch '+source);assets.append(dict(id=a['stable_id'],source=a['source_path'],sha256=a['source_sha256']));sources[source]=a['source_sha256']
 require(len({a['id']for a in assets})==len(assets)and len({a['source']for a in assets})==len(assets),'Dialogue audio duplicate bank source')
 nodes=[]
 for path in ['AudioStreamPlayer','SoundEffect','InputSound']:
  n=nm[path];v=decode(n['properties']);r=records[path];original=serialized[path];require(n['class']=='AudioStreamPlayer'and not r['script']and not v['autoplay']and not v['stream_paused']and v['mix_target']==0,'Unknown dialogue audio native policy');source=rs[v['stream']['id']]['path'];asset=next((a for a in assets if a['source']==source),None);require(asset,'Dialogue audio default stream is not checked PCM');bus=original.get('bus',v['bus']);require(bus=='SFX'and v['bus']=='Master','Official quarantine getter/serialized bus fact changed')
  nodes.append(dict(id=r['id'],parent=r['parent'],ready=r['ready'],pause=r['pause'],priority=r['priority'],node=path,stream=asset['id'],volume_db=v['volume_db'],pitch=v['pitch_scale'],autoplay=v['autoplay'],paused=v['stream_paused'],mix_target=v['mix_target'],bus=bus,native_unconfigured_bus=v['bus']))
 a=(ROOT/'upstream/MOTHER-Encore'/ABSTRACT).read_text(encoding='utf8');g=(ROOT/'upstream/MOTHER-Encore'/DIALOGUE).read_text(encoding='utf8');pitch=re.search(r'set_pitch_scale\(rand_range\(([0-9.]+), ([0-9.]+)\)\)',a);require(pitch and '$AudioStreamPlayer.stream != null' in a and 'last_visible_char != TextTools.CHAR_DELAY'in a,'Dialogue character source gate differs');prefix=re.search(r'_curr_phrase\["sound"\] = "res://([^"\n]+)" \+',g);require(prefix and '$AudioStreamPlayer.stream = null'in g and '$AudioStreamPlayer.stream = load(_curr_phrase["sound"] +".mp3")'in g,'Dialogue source stream assignment differs')
 src=(engine/'audio_stream_player.cpp').read_text(encoding='utf8');header=engine/'audio_stream_player.h';require('NOTIFICATION_ENTER_TREE' in src and 'add_callback(_mix_audios, this)'in src and 'stop_has_priority.clear()'in src and 'set_process_internal(false);\n\t\t\temit_signal("finished")'in src and 'stream_paused && !stream_paused_fade'in src,'Audited native audio lifecycle changed');fade_stop=int(re.search(r'buffer_size = MIN\(buffer_size, ([0-9]+)\)',src)[1]);fade_replace=int(re.search(r'fadeout_buffer.resize\(([0-9]+)\)',src)[1]);silence=float(re.search(r'p_fadeout \? (-?[0-9.]+)',src)[1]);mp3=(engine/'audio_stream_mp3.cpp').read_text(encoding='utf8');require('if (p_time >= mp3_stream->get_length())'in mp3 and 'p_time = 0;'in mp3 and 'frames_mixed = uint32_t(mp3_stream->sample_rate * p_time);'in mp3,'Audited MP3 seek/wrap changed');engine_proof={f:sha(engine/f)for f in ['audio_stream_player.cpp','audio_stream_player.h','audio_stream_mp3.cpp']}
 for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Dialogue audio source changed '+p)
 write(IR,dict(schema=1,kind='encore.field-dialogue-audio.source-ir',commit=PIN,scene=SCENE,scene_id=recipe['scene_id'],source_sha256=recipe['source_sha256'],recipe_sha256=sha(ROOT/'content/dialogue-node-recipe.json'),native_sha256=sha(native),bank_source_sha256=sha(ROOT/'content/native-audio.json'),nodes=nodes,assets=assets,pitch_range=[float(pitch[1]),float(pitch[2])],text_prefix='res://'+prefix[1],extension='.mp3',finished='finished',source_silence_db=silence,fade_stop_frames=fade_stop,fade_replace_frames=fade_replace,sources=sources,engine=dict(version='3.6.2-stable',repo='godotengine/godot',proofs=engine_proof),scene_admitted=False,pending=['Real NDSP DSP service/checked independent PCM voices and actual source bus/AudioServer callback owner required','Only genuinely converted bank filenames are supported; unknown filenames reject before replacing a live stream','Source MP3 seek at/past duration wraps to frame0; source playback position uses decoder frames, not audible hardware position','Physical voices lease channels20/21; third simultaneous playback explicitly rejects hardware exhaustion, never steals music','Native asynchronous seek/stop and finished run at actual audio-mix/internal-process boundaries; no elapsed game-clock simulation']))
 write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),recipe_sha256=sha(ROOT/'content/dialogue-node-recipe.json'),producer_sha256=sha(Path(__file__)),engine=engine_proof,source_streams=len(assets),scope='Three actual AudioStreamPlayer native bodies and source stream/character endpoints; no DialogueBox script Ready grant'))
def load():
 d=read(IR);r=read(REVIEW);require(d['schema']==1 and d['commit']==PIN and not d['scene_admitted']and r['ir_sha256']==sha(IR)and r['producer_sha256']==sha(Path(__file__))and d['recipe_sha256']==sha(ROOT/'content/dialogue-node-recipe.json')and d['bank_source_sha256']==sha(ROOT/'content/native-audio.json'),'Dialogue audio review/bank mismatch');inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Dialogue audio changed '+p)
 return d
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 b.extend(bytes.fromhex(d['recipe_sha256']));b.extend(struct.pack('<2d',*d['pitch_range']));f(d['source_silence_db']);u(d['fade_stop_frames'],d['fade_replace_frames']);t(d['text_prefix']);t(d['extension']);t(d['finished'])
 for v in d['nodes']:u(v['id'],v['parent'],v['ready'],v['pause'],v['stream'],int(v['autoplay']),int(v['paused']),v['mix_target']);b.extend(struct.pack('<i',v['priority']));f(v['volume_db'],v['pitch']);t(v['node']);t(v['bus'])
 u(len(d['assets']))
 for a in d['assets']:u(a['id']);t(a['source']);b.extend(bytes.fromhex(a['sha256']))
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 for p in ['audio_stream_player.cpp','audio_stream_player.h','audio_stream_mp3.cpp']:b.extend(bytes.fromhex(d['engine']['proofs'][p]))
 struct.pack_into('<8s8I',b,0,b'ENCFDAU1',1,128,len(b),zlib.crc32(b[128:]),0x454e0049,1,len(d['nodes']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk-dialogue-audio.encdaudio').read_bytes()==raw,'Staged dialogue audio differs');return {Path('data/podunk-dialogue-audio.encdaudio'):raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native and a.engine,'Explicit original native and audited official engine sources required');extract(a.native,a.engine);return
 raw=encode(load())
 if a.action=='verify':require(OUT.read_bytes()==raw,'Dialogue audio stale')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Dialogue audio source resource:',len(raw),'bytes; 3 actual native players; no sceneReady grant')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD DIALOGUE AUDIO ERROR: '+str(e))
