"""Produce detached wall fixtures and frozen engine command projections.

The tiny audio bank is an explicitly synthetic metadata lookup fixture. It has
no PCM file and must never be staged or described as original audio preparation.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_wall_timeline as d,audio_asset,dungeon_switches
SOURCE=ROOT/'reports/prop-animation-continuation/source-oracle-1'
def build(out):
 out=Path(out).resolve();d.require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build','Wall fixture must stay under build')
 d.build(out);data=d.parse((out/'source.encwall').read_bytes());recipe,_=d.read();ids={r['name']:r['id']for r in recipe['roles']}
 proof=json.loads((SOURCE/'verification.json').read_bytes())
 for name in('wall-timeline-3/observations.json','wall-timeline-3/input.json'):d.require(hashlib.sha256((SOURCE/name).read_bytes()).hexdigest()==proof['evidence_sha256'][name],'Frozen wall reference changed')
 observed=json.loads((SOURCE/'wall-timeline-3/observations.json').read_bytes());config=json.loads((SOURCE/'wall-timeline-3/input.json').read_bytes());lines=['# Frozen original property/audio/deferred-method commands; no physical/audio playback assertion.']
 for case,test in zip(observed['cases'],config):
  d.require(case['name']==test['name']and len(case['rows'])==len(test['actions']),'Wall fixture source case mismatch');lines.append('case '+case['name']);assigned=2**32-1
  for row,action in zip(case['rows'],test['actions']):
   if action['op']=='play':value=ids[action['clip']];assigned=value
   elif action['op']=='advance':value=action['delta']
   else:d.require(action['op']=='flush','Wall fixture unknown operation');value=0
   events=[]
   for event in row['events']:
    if event['kind']=='audio':events.append('1:'+str(int(event['playing'])))
    elif event['kind']=='shake':
     d.require({k:event[k]for k in('magnitude','direction','interval','weight','diminish')}==data['camera'],'Wall fixture camera parameters differ');events.append('2:'+repr(event['duration']))
    elif event['kind']=='finished':events.append('3:'+str(ids[event['clip']]))
    else:raise ValueError('Wall fixture unknown observed event')
   lines.append(' '.join(map(str,[action['op'],repr(value),row['frame'],int(row['body']),int(row['playing_request']),int(row['animation_playing']),assigned,row['position'],','.join(events)or'-'])))
 (out/'reference.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8')
 graph,graph_proof=dungeon_switches.extract();(out/'graph.encdsw').write_bytes(dungeon_switches.encode(graph))
 a=data['audio'];record=dict(stable_id=a['id'],source_path=a['path'],source_sha256=a['sha'],sample_rate=44100,channels=1,loop=a['loop'],frames=1,loop_start=0,pcm_bytes=2,pcm_crc32=0,pcm_path='sound/fixtures/wall-metadata-only.pcm',gain_db=a['gain'])
 (out/'metadata-only.encaudio').write_bytes(audio_asset.build_bank([record],0,-60))
 (out/'fixture-boundary.json').write_text(json.dumps(dict(scope=__doc__,original_rows=sum(len(c['rows'])for c in observed['cases']),synthetic_audio_record=record,no_pcm_created=True,graph_source=graph_proof,source_reference_sha256=hashlib.sha256((SOURCE/'wall-timeline-3/observations.json').read_bytes()).hexdigest()),indent=2)+'\n',encoding='utf8')
 print('Wall fixture:948-byte source candidate,309 frozen observations, real graph, explicitly synthetic audio metadata only; no PCM/live scene.')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);build(p.parse_args().out)
