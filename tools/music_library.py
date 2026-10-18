"""Candidate source music library and one-track banks; never stage implicitly."""
from pathlib import Path
import argparse,hashlib,json,math,re,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import audio_asset,run_music_finished_reference as finished
from tools.extract_battle_entry import Extractor,PIN,require
RECIPE=ROOT/'content/global-music.json'
def source(recipe=None):
 p=json.loads(RECIPE.read_bytes())if recipe is None else recipe
 require(type(p)is dict and set(p)=={'schema','kind','source_commit','audio_recipe','audio_bank','default_gain_db','tracks'},'Music library recipe fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.music-library.candidate'and p['source_commit']==PIN and p['audio_recipe']=='content/native-audio.json'and p['audio_bank']=='romfs/sound/banks/opening.encaudio','Music library recipe identity')
 require(type(p['default_gain_db'])in(int,float)and math.isfinite(p['default_gain_db'])and -120<=p['default_gain_db']<=24,'Music library empty player gain')
 require(type(p['tracks'])is list and 1<=len(p['tracks'])<=64 and all(type(v)is int and 0<v<2**32 for v in p['tracks'])and len(set(p['tracks']))==len(p['tracks']),'Music library stable track IDs')
 original=json.loads((ROOT/p['audio_recipe']).read_bytes());audio_asset.exact(original,['schema','upstream_commit','bus_source','bus_sha256','manager_source','manager_sha256','assets'],'Source audio recipe');require(type(original['schema'])is int and original['schema']==1 and original['upstream_commit']==PIN,'Source audio recipe identity');wanted=[a for a in original['assets']if a['source_path'].startswith('res://Audio/Music/')]
 require(p['tracks']==[a['stable_id']for a in wanted],'Music library must cover the reviewed current root music scope in source order')
 _,reference=finished.source();require(reference==json.loads((ROOT/'compatibility/reviews/music-finished-v0410.json').read_bytes()),'Music default/source reference changed')
 observed=finished.validate_result(json.loads((ROOT/'reports/mayor-office-import/music-finished-native.json').read_bytes()),reference)
 require(all(row['views'][0][0]['gain']==p['default_gain_db']for row in observed['rows']),'Music empty player gain differs from original engine')
 bank=audio_asset.parse_bank((ROOT/p['audio_bank']).read_bytes());require(bank['silence_db']==reference['parameters']['silence_db'],'Music library bank/manager silence mismatch')
 ex=Extractor(ROOT);tracks=[];records=[]
 for name,digest in [(original['manager_source'],original['manager_sha256']),(original['bus_source'],original['bus_sha256'])]:require(hashlib.sha256(ex.data(name)).hexdigest()==digest,'Source audio manager/bus identity changed')
 buses=ex.data(original['bus_source']).decode();master=re.search(r'^bus/0/volume_db = (-?[0-9.]+)$',buses,re.M);require(master and bank['master_db']==struct.unpack('<f',struct.pack('<f',float(master[1])))[0],'Music library master bus mismatch')
 for declared in wanted:
  path=declared['source_path'];raw=ex.data(path[6:]);imported=ex.data(path[6:]+'.import')
  require(hashlib.sha256(raw).hexdigest()==declared['source_sha256']and hashlib.sha256(imported).hexdigest()==declared['import_sha256'],'Music library reviewed source/import changed')
  looping,offset=audio_asset.import_settings(imported,path);rows=[a for a in bank['assets']if a['stable_id']==declared['stable_id']];require(len(rows)==1,'Music library source bank ID missing or ambiguous');row=rows[0]
  require(row['source_path']==path and row['source_sha256']==declared['source_sha256']and row['pcm_path']==declared['pcm_path']and row['gain_db']==declared['gain_db']and row['loop']==looping and row['loop_start']==(int(offset*row['sample_rate'])if looping else 0),'Music library bank differs from original source/import/PCM binding')
  tracks.append(dict(id=row['stable_id'],source_path=path,source_sha256=declared['source_sha256'],import_sha256=declared['import_sha256'],builtin_loop=looping));records.append(row)
 metadata=dict(silence_db=bank['silence_db'],fade_to_seconds=reference['parameters']['fade_to_seconds'],empty_player_gain_db=p['default_gain_db'],tracks=tracks)
 proof=dict(source_commit=PIN,release_staged=False,sources=ex.sources,recipe_sha256=hashlib.sha256(RECIPE.read_bytes()).hexdigest(),native_audio_sha256=hashlib.sha256((ROOT/p['audio_recipe']).read_bytes()).hexdigest(),source_bank_sha256=hashlib.sha256((ROOT/p['audio_bank']).read_bytes()).hexdigest(),defaults_manager_methods=reference['manager_methods'],metadata=metadata,mapped_implementation={'library_version_2':'runtime/music_regions.cpp:MusicRegionData::load','source_builtin_loop_match':'runtime/music_regions.cpp:MusicRegionData::matches'},scope='Current root music assets only; original builtin stream loop policy is distinct from manager finished-loop callbacks. Version 1 scene resources unchanged. Version 2 has no regions, stores the observed empty gain and explicit builtin flags. One-track banks reuse existing checked PCM paths without copying payloads. No source programs, natural EOF, live scene, emulator or hardware pass claim.')
 return p,metadata,records,bank,proof
def encode(metadata):
 require(type(metadata)is dict and set(metadata)=={'silence_db','fade_to_seconds','empty_player_gain_db','tracks'},'Music library metadata fields')
 for key,low,high in [('silence_db',-120,-20),('fade_to_seconds',0,60),('empty_player_gain_db',-120,24)]:
  value=metadata[key];require(type(value)in(int,float)and math.isfinite(value)and low<=value<=high,'Music library tuning')
 tracks=metadata['tracks'];require(type(tracks)is list and 1<=len(tracks)<=64,'Music library track count');blob=bytearray(64);ids=set();paths=set()
 for track in tracks:
  require(type(track)is dict and set(track)=={'id','source_path','source_sha256','import_sha256','builtin_loop'},'Music library track fields');ident=track['id'];path=track['source_path']
  require(type(ident)is int and 0<ident<2**32 and ident not in ids,'Music library duplicate/invalid ID');ids.add(ident)
  require(type(path)is str and path.startswith('res://')and path not in paths and 6<len(path)<=1024 and all(32<=ord(c)<127 for c in path)and all(part not in ('','.','..')for part in path[6:].split('/'))and '\\'not in path and ':'not in path[6:],'Music library source URI');paths.add(path)
  for key in ['source_sha256','import_sha256']:
   value=track[key];require(type(value)is str and len(value)==64 and all(c in '0123456789abcdef'for c in value)and int(value,16),'Music library digest')
  require(type(track['builtin_loop'])is bool,'Music library builtin flag');raw=path.encode('ascii');blob.extend(struct.pack('<I',ident)+bytes.fromhex(track['source_sha256'])+bytes.fromhex(track['import_sha256'])+struct.pack('<I',len(raw))+raw+struct.pack('<I',int(track['builtin_loop'])))
 require(len(blob)<=65536,'Music library byte limit');struct.pack_into('<8s6I3f',blob,0,b'ENCMUS01',2,len(blob),0,0,len(tracks),0,metadata['silence_db'],metadata['fade_to_seconds'],metadata['empty_player_gain_db']);struct.pack_into('<I',blob,16,zlib.crc32(blob));return bytes(blob)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Music library candidates stay under build');p,metadata,records,bank,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/music-library-v0410.json').read_bytes()),'Music library semantic review changed');out.mkdir(parents=True,exist_ok=True);outputs={}
 def write(name,data):
  file=(out/name).resolve();require(file.is_relative_to(out),'Music library path escaped owner');file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(data);outputs[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
 write('sound/banks/global.encmusic',encode(metadata))
 for track,row in zip(metadata['tracks'],records):
  subset={**metadata,'tracks':[track]};stem='sound/banks/global/'+str(track['id']);write(stem+'.encmusic',encode(subset));write(stem+'.encaudio',audio_asset.build_bank([row],bank['master_db'],bank['silence_db']))
 receipt=dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),outputs=outputs);(out/'source.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Music library candidate:',len(records),'tracks,',len(outputs),'metadata files; PCM not copied; not staged')
if __name__=='__main__':main()
