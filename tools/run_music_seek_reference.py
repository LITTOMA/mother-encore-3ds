"""Original manager forwards start offsets and deferred loops reset them to zero."""
from pathlib import Path
import argparse,hashlib,json,math,re,shutil,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_music_finished_reference as base,audio_asset
from tools.extract_battle_entry import Extractor,require
from tools.run_scene_reference import invoke
CASES=[
 dict(name='pending_nonzero_start',actions=[['play',0,1,2,1.25]]),
 dict(name='pending_layer_fraction',actions=[['play',0,4,4,1.375],['add'],['play',1,5,5,1.375]]),
 dict(name='float32_forwarding',actions=[['play',0,3,3,'0.123456789']]),
 dict(name='deferred_loop_resets_start',actions=[['play',0,1,2,1.25],['emit',0],['flush']]),
 dict(name='same_stream_replacement_offset',actions=[['play',0,4,4,1.375],['play',0,4,4,.125]]),
 dict(name='stop_preserves_pending_seek',actions=[['play',0,4,4,2.5],['stop',0],['emit',0],['flush']]),
 dict(name='deferred_overwrites_replacement_offset',actions=[['play',0,1,2,1.25],['emit',0],['play',0,3,3,2.5],['flush']]),
 dict(name='pending_offset_beyond_length',actions=[['play',0,4,4,2000]]),
]
PROBE=base.PROBE.replace('"remove":remove}', '"remove":remove,"position":"%.17f"%player.get_playback_position()}')
PROBE=PROBE.replace('"offset":stream.loop_offset}', '"offset":stream.loop_offset,"length":stream.get_length()}')
PROBE=PROBE.replace('manager.play_music(track,loop,int(action[1]))','manager.get_audio_player(int(action[1])).stream_paused=true\n                    manager.play_music(track,loop,int(action[1]),float(action[4]))')
def source():
 files,proof=base.source();ex=Extractor(ROOT);recipe=json.loads((ROOT/'content/native-audio.json').read_bytes());imports=list(proof['imports'])
 for ident in [3080041499,3778949439]:
  row=next(v for v in recipe['assets']if v['stable_id']==ident);path=row['source_path'];raw=ex.data(path[6:]);settings=ex.data(path[6:]+'.import');require(hashlib.sha256(raw).hexdigest()==row['source_sha256']and hashlib.sha256(settings).hexdigest()==row['import_sha256'],'Reviewed layer source/import changed');loop,offset=audio_asset.import_settings(settings,path);imports.append(dict(source=path,loop=loop,offset=offset))
 files['probe.gd']=PROBE;files['input.json']=json.dumps(dict(imports=imports,cases=CASES))
 proof=dict(source_commit=proof['source_commit'],whole_gameplay_approved=False,sources={**proof['sources'],**ex.sources},manager_methods=proof['manager_methods'],imports=imports,cases=CASES,parameters=proof['parameters'],generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},mapped_implementation={'forward_float_start':'runtime/music_players.cpp:play_track','deferred_zero_start':'runtime/music_players.cpp:dispatch_finished','MP3_source_frame_admission':'runtime/audio_data.cpp:music_start_frame','physical_wave_position':'platform/ctr/music_region_player.cpp:playback_position'},scope='Unchanged selected original manager methods and real player/signal bindings. Player stream_paused is set before each play to freeze the queued seek and make immediate get_playback_position deterministic; this is an explicit property adapter, not original gameplay pause behavior. Deferred finished callbacks reset their saved start to zero. Engine MP3 seek/frame conversion is reviewed separately. No natural audio clock, perceptual phase, non-MP3 nonzero start, integrated game or hardware pass.')
 return files,proof
def validate_result(doc,proof=None):
 if proof is None:proof=source()[1]
 require(type(doc)is dict and set(doc)=={'schema','godot','rows','imports'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(CASES),'Music seek result schema/count')
 require(type(doc['imports'])is list and len(doc['imports'])==len(proof['imports']),'Music seek imports')
 for actual,expected in zip(doc['imports'],proof['imports']):
  require(type(actual)is dict and set(actual)=={'source','loop','offset','length'}and actual['source']==expected['source']and type(actual['loop'])is bool and actual['loop']==expected['loop']and type(actual['offset'])in(int,float)and math.isfinite(actual['offset'])and abs(actual['offset']-expected['offset'])<.00001 and type(actual['length'])in(int,float)and math.isfinite(actual['length'])and 0<actual['length']<3600,'Music seek import/source dimensions')
 paths={v['source']for v in proof['imports']}|{''}
 for row,case in zip(doc['rows'],CASES):
  require(type(row)is dict and set(row)=={'case','views'}and json.dumps(row['case'],sort_keys=True)==json.dumps(case,sort_keys=True)and type(row['views'])is list and len(row['views'])==len(case['actions'])+1,'Music seek case/order')
  for view in row['views']:
   require(type(view)is list and 1<=len(view)<=16,'Music seek voice count');ids=[]
   for v in view:
    require(type(v)is dict and set(v)=={'id','stream','playing','queued','gain','loop','index','remove','position'}and type(v['id'])is int and 0<=v['id']<64 and v['id']not in ids and type(v['stream'])is str and v['stream']in paths and type(v['loop'])is str and v['loop']in paths and type(v['playing'])is bool and(not v['playing']or v['stream'])and type(v['queued'])is bool and type(v['remove'])is bool and type(v['gain'])in(int,float)and math.isfinite(v['gain'])and -120<=v['gain']<=24 and type(v['index'])is int and(0<=v['index']<16 if v['loop']else v['index']==-1)and type(v['position'])is str and re.fullmatch(r'[0-9]+\.[0-9]{17}',v['position'])and math.isfinite(float(v['position']))and 0<=float(v['position'])<=2000,'Music seek voice/callback shape');ids.append(v['id'])
 return doc
def fixture(doc,proof=None):
 if proof is None:proof=source()[1]
 validate_result(doc,proof);ids={v['source']:i+1 for i,v in enumerate(proof['imports'])};ids['']=0;lines=['# original pending seek offsets; paused property adapter, no audio clock claim']
 for path,index in ids.items():
  if index:lines.append('# stream '+str(index)+' '+path)
 p=proof['parameters'];lines.append('# policy '+str(p['silence_db'])+' '+str(p['fade_to_seconds'])+' '+str(doc['rows'][0]['views'][0][0]['gain']))
 for i,row in enumerate(doc['rows']):
  lines.append('case '+str(i)+' '+row['case']['name'])
  for step,view in enumerate(row['views']):
   op=['initial']if step==0 else row['case']['actions'][step-1];lines.append('op '+' '.join(str(v)for v in op));values=[len(view)]
   for v in view:values.extend([v['id'],ids[v['stream']],int(v['playing']),int(v['queued']),v['gain'],ids[v['loop']],v['index'],int(v['remove']),v['position']])
   lines.append('state '+' '.join(str(v)for v in values))
 return '\n'.join(lines)+'\n'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',required=True,type=Path);ap.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');a=ap.parse_args();out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned music seek reference required');files,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/music-seek-v0410.json').read_bytes()),'Music seek source review changed');out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-seek').write_bytes(b'music-seek-v1')
 try:
  for name,value in files.items():(project/name).write_bytes(value.encode())
  ex=Extractor(ROOT)
  for settings in proof['imports']:
   name=settings['source'][6:];target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
  engine=a.godot.resolve();invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'godot.log');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);(out/'seek.tsv').write_bytes(fixture(doc,proof).encode());receipt=dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest());(out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Original music queued seek cases:',len(doc['rows']))
 finally:
  if project.resolve().is_relative_to(out)and(project/'.owned-seek').read_bytes()==b'music-seek-v1':shutil.rmtree(project)
if __name__=='__main__':main()
