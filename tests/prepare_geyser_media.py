"""Restore source Geyser media and checked original automatic-idle observations."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_geyser_media as media,run_geyser_idle_reference as native
REVIEW=ROOT/'compatibility/reviews/geyser-media-v0410.json'
def build(out):
 out=Path(out).resolve();marker=out/'.owned-geyser-fixture';media.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not out.is_symlink()and marker.read_bytes()==b'geyser-media-fixture-v1'),'Owned Geyser media fixture under build required');review=json.loads(REVIEW.read_bytes(),object_pairs_hook=media.g.unique);media.g.fields(review,'schema source producer_sha256 native_producer_sha256 results mapping unverified','geyser media semantic review');_,_,_,_,_,source=media.source();producer=hashlib.sha256(Path(media.__file__).read_bytes()).hexdigest();native_producer=hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest();media.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['producer_sha256']==producer and review['native_producer_sha256']==native_producer and type(review['results'])is list and len(review['results'])==2,'Geyser media source/producer review changed');docs=[]
 for row in review['results']:
  media.g.fields(row,'path sha256','geyser idle original result');media.g.safe(row['path']);folder=(ROOT/row['path']).resolve();media.require(folder.is_relative_to(ROOT/'reports/geyser-media')and not folder.is_symlink(),'Geyser idle frozen result escaped');raw=(folder/'reference.json').read_bytes();proof=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=media.g.unique);media.require(hashlib.sha256(raw).hexdigest()==row['sha256']and proof['result_sha256']==row['sha256']and proof['producer_sha256']==native_producer,'Geyser idle frozen result/producer changed');docs.append(native.validate_result(json.loads(raw),proof))
 media.require(docs[0]==docs[1],'Geyser idle originals differ');doc=docs[0];lines=[str(len(doc['rows']))];_,timeline,_=native.g.source();ids={'res://'+s['source']:s['id']for s in timeline['sounds']};last_sound=0
 def state(row):
  nonlocal last_sound
  lines.append(' '.join(map(str,[row['time'],row['frame'],int(row['monitoring']),int(row['disabled']),int(row['active']),int(row['paused']),row['gain'],int(row['queued']),int(bool(row['finished']))])));commands=[]
  for c in row['commands']:
   if c[0]=='stream':last_sound=ids[c[1]];commands.append([1,last_sound,0])
   elif c[0]=='play':commands.append([2,last_sound,c[1]])
   elif c[0]=='stop':commands.append([3,0,0])
   else:raise ValueError('Unknown original idle audio command')
  for c in row['global_calls']:commands.append([4,ids[c[0]],0])
  lines.append(str(len(commands)));lines.extend(' '.join(map(str,c))for c in commands)
 state(doc['initial'])
 for row in doc['rows']:
  b,a=row['before'],row['after'];lines.append(' '.join(map(str,[row['delta'],int(row['tree_paused']),int(b['on_screen']),*[int(v)for v in b['ui']],int(a['on_screen']),*[int(v)for v in a['ui']],len(row['actions'])])));state(b)
  for op in row['actions']:lines.append(' '.join(map(str,[{'player_pause':1,'tree_pause':2,'visible':3,'ui':4}[op[0]],int(op[1])])))
  state(a)
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'geyser-media-fixture-v1');path=out/'idle.tsv';value=('\n'.join(lines)+'\n').encode();media.require(not path.exists()or path.is_file()and not path.is_symlink()and path.read_bytes()==value,'Geyser idle cached fixture changed');path.write_bytes(value);(out/'geyser.encgeyser').write_bytes(native.g.encode(timeline))
 candidate=out/'media'
 if not candidate.exists():media.compile(candidate)
 else:
  proof=json.loads((candidate/'source.json').read_bytes(),object_pairs_hook=media.g.unique);media.require((candidate/'.owned-geyser-media').read_bytes()==b'geyser-media-v1'and proof['producer_sha256']==producer and proof['review_sha256']==source['review_sha256'],'Geyser cached candidate owner/producer changed')
  policy=media.read();target=candidate/policy['output'];media.require(hashlib.sha256(target.read_bytes()).hexdigest()==proof['metadata_sha256'],'Geyser cached metadata changed');model=media.parse(target.read_bytes());bank=media.audio_asset.parse_bank(model['bank']);expected={policy['bank']}|{a['pcm_path']for a in bank['assets']};media.require(type(proof['output_bindings'])is list and len(proof['output_bindings'])==len(expected),'Geyser cached exact binding count');seen=set()
  for row in proof['output_bindings']:
   media.g.fields(row,'conversion_path candidate_path bytes sha256','Geyser cached output binding');media.g.safe(row['candidate_path']);media.require(row['candidate_path']in expected and row['candidate_path']not in seen and row['conversion_path']==('sound/banks/opening.encaudio'if row['candidate_path']==policy['bank']else row['candidate_path']),'Geyser cached output identity');seen.add(row['candidate_path']);target=candidate/row['candidate_path'];media.require(target.is_file()and not target.is_symlink()and target.resolve().is_relative_to(candidate)and type(row['bytes'])is int and target.stat().st_size==row['bytes']and hashlib.sha256(target.read_bytes()).hexdigest()==row['sha256'],'Geyser cached PCM/bank changed')
 tree_value=('\n'.join([str(len(doc['tree_lifecycle']))]+[' '.join(str(int(row[key]))for key in ['paused','playing','can_process'])for row in doc['tree_lifecycle']])+'\n').encode();tree=out/'tree.tsv';media.require(not tree.exists()or tree.is_file()and not tree.is_symlink()and tree.read_bytes()==tree_value,'Geyser cached tree lifecycle changed');tree.write_bytes(tree_value)
 print('Checked Geyser media,224 automatic frames and8 named tree states;not new engine/NDSP execution')
 return candidate
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
