"""Compile checked room metadata and translate unchanged native observations."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_rooms as rooms
REFERENCE=ROOT/'reports/dungeon-rooms/native-6'

def reference(folder=REFERENCE):
 proof=json.loads((folder/'receipt.json').read_bytes());doc=json.loads((folder/'observations.json').read_bytes())
 if proof['commit']!=rooms.PIN or doc['engine']['hash']!='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8':raise ValueError('Dungeon room reference source/engine')
 for name,digest in proof['outputs_sha256'].items():
  if name=='source.encdrooms':continue
  if hashlib.sha256((folder/name).read_bytes()).hexdigest()!=digest:raise ValueError('Dungeon room reference changed: '+name)
 for name,digest in proof['source']['sources'].items():
  if hashlib.sha256((ROOT/'upstream/MOTHER-Encore'/name).read_bytes()).hexdigest()!=digest:raise ValueError('Dungeon room reference source changed: '+name)
 if len(doc['rows'])!=140 or len(doc['geometry'])!=36:raise ValueError('Dungeon room reference coverage')
 return doc,proof

def actions(name):
 if name=='ready':return 0,0
 if name=='other_body_ignored':return 4,0
 for prefix,code in (('enter/',1),('idle/',2),('exit/',3)):
  if name.startswith(prefix):return code,int(name[len(prefix):])
 table=dict(overlap_enter0=(1,0),overlap_enter1=(1,1),overlap_idle=(2,0),overlap_exit1_keeps_last_limits=(3,1),overlap_exit0_resets_limits=(3,0),cancel_enter=(1,2),cancel_exit=(3,2),cancel_idle=(2,0),reenter_visited=(1,0),reenter_idle=(2,0),reenter_exit=(3,0),same_frame_reenter=(5,0),same_frame_reenter_idle_twice=(2,0),same_frame_reenter_exit=(3,0))
 if name not in table:raise ValueError('Unknown native dungeon-room observation')
 return table[name]

def build(out):
 out=Path(out);out.mkdir(parents=True,exist_ok=True);model,raw,source=rooms.extract();doc,proof=reference()
 if hashlib.sha256(raw).hexdigest()!=proof['outputs_sha256']['source.encdrooms']:raise ValueError('Dungeon room native resource changed')
 (out/'source.encdrooms').write_bytes(raw);rows=[]
 for r in doc['rows']:
  code,index=actions(r['action']);inside=sum(int(v)<<i for i,v in enumerate(r['inside']));visited=sum(int(v)<<i for i,v in enumerate(r['flags']));relative=r['relative']or['0','0']
  rows.append([r['viewport'],code,index,r['camareas'],r['room'],r['flag_signals'],inside,visited,*r['limits'],*r['camera_position'],int(r['relative']is not None),*relative,len(r['offset_calls'])])
 (out/'reference.tsv').write_text(''.join('\t'.join(map(str,row))+'\n'for row in rows),encoding='utf8')
 geometry=[]
 for r in doc['geometry']:geometry.append([r['viewport'],r['index'],*r['center'],*r['size'],r['layer'],r['mask'],int(r['monitoring']),int(r['monitorable']),int(r['visible'])])
 (out/'geometry.tsv').write_text(''.join('\t'.join(map(str,row))+'\n'for row in geometry),encoding='utf8');print('Dungeon room fixture:140 unchanged observations,36 native shapes;',len(raw),'bytes')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
