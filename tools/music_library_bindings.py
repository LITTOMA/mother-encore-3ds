"""Compile explicit music metadata/bank file bindings; PCM stays in existing paths."""
from pathlib import Path
import argparse,hashlib,json,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import music_library as library,audio_asset
from tools.extract_battle_entry import PIN,require
RECIPE=ROOT/'content/global-music-bindings.json'
LIMIT=65536
def path(value,suffix):
 require(type(value)is str and 0<len(value)<=256 and value.endswith(suffix)and len(value)>len(suffix)and all(33<=ord(c)<=126 and c not in '\\:?#'for c in value)and all(p not in ('','.','..')for p in value.split('/')),'Music binding file path/type')
 return value
def checked(doc):
 require(type(doc)is dict and set(doc)=={'index','bindings'},'Music binding descriptor fields')
 def ref(f,suffix):
  require(type(f)is dict and set(f)=={'path','bytes','crc'},'Music binding file fields');path(f['path'],suffix)
  require(type(f['bytes'])is int and 0<f['bytes']<=LIMIT and type(f['crc'])is int and 0<=f['crc']<2**32,'Music binding file fingerprint')
 ref(doc['index'],'.encmusic');require(type(doc['bindings'])is list and 1<=len(doc['bindings'])<=64,'Music binding count');ids=set();paths={doc['index']['path']}
 for b in doc['bindings']:
  require(type(b)is dict and set(b)=={'track','metadata','audio'}and type(b['track'])is int and 0<b['track']<2**32 and b['track']not in ids,'Music binding track identity');ids.add(b['track'])
  for key,suffix in [('metadata','.encmusic'),('audio','.encaudio')]:
   ref(b[key],suffix);require(b[key]['path']not in paths,'Music binding duplicate file');paths.add(b[key]['path'])
 return doc
def encode(doc):
 checked(doc);blob=bytearray(32)
 def ref(f):
  raw=f['path'].encode('ascii');blob.extend(struct.pack('<3I',f['bytes'],f['crc'],len(raw))+raw)
 ref(doc['index'])
 for b in doc['bindings']:
  blob.extend(struct.pack('<I',b['track']));ref(b['metadata']);ref(b['audio'])
 require(len(blob)<=LIMIT,'Music binding descriptor limit');struct.pack_into('<8s6I',blob,0,b'ENCLIB01',1,len(blob),0,1,len(doc['bindings']),0);struct.pack_into('<I',blob,16,zlib.crc32(blob));return bytes(blob)
def decode(raw):
 require(type(raw)is bytes and 32<=len(raw)<=LIMIT,'Music binding descriptor size');magic,version,total,crc,cap,count,reserved=struct.unpack_from('<8s6I',raw);require(magic==b'ENCLIB01'and version==cap==1 and total==len(raw)and 1<=count<=64 and reserved==0,'Music binding descriptor header');blob=bytearray(raw);struct.pack_into('<I',blob,16,0);require(zlib.crc32(blob)==crc,'Music binding descriptor CRC');offset=32
 def integer():
  nonlocal offset
  require(offset+4<=len(raw),'Music binding truncated integer');value=struct.unpack_from('<I',raw,offset)[0];offset+=4;return value
 def ref():
  nonlocal offset
  size,crc,length=integer(),integer(),integer();require(0<length<=256 and offset+length<=len(raw),'Music binding truncated path')
  try:value=raw[offset:offset+length].decode('ascii')
  except UnicodeDecodeError as e:raise ValueError('Music binding non-ASCII path')from e
  offset+=length;return dict(path=value,bytes=size,crc=crc)
 doc=dict(index=ref(),bindings=[])
 for _ in range(count):doc['bindings'].append(dict(track=integer(),metadata=ref(),audio=ref()))
 require(offset==len(raw),'Music binding trailing records');return checked(doc)
def source(recipe=None):
 p=json.loads(RECIPE.read_bytes())if recipe is None else recipe
 require(type(p)is dict and set(p)=={'schema','kind','source_commit','output','index','tracks'},'Music bindings recipe fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.music-library-bindings.candidate'and p['source_commit']==PIN,'Music bindings recipe identity');path(p['output'],'.enclib');path(p['index'],'.encmusic')
 _,metadata,records,bank,original=library.source();require(original==json.loads((ROOT/'compatibility/reviews/music-library-v0410.json').read_bytes()),'Music library source review changed');require(p['index']=='sound/banks/global.encmusic','Music index producer binding changed')
 require(type(p['tracks'])is list and len(p['tracks'])==len(metadata['tracks']),'Music bindings must cover entire reviewed library');files={p['index']:library.encode(metadata)};bindings=[]
 def ref(name):return dict(path=name,bytes=len(files[name]),crc=zlib.crc32(files[name]))
 for row,track,asset in zip(p['tracks'],metadata['tracks'],records):
  require(type(row)is dict and set(row)=={'track','metadata','audio'}and type(row['track'])is int and row['track']==track['id'],'Music bindings source order/identity')
  path(row['metadata'],'.encmusic');path(row['audio'],'.encaudio');stem='sound/banks/global/'+str(track['id']);require(row['metadata']==stem+'.encmusic'and row['audio']==stem+'.encaudio','Music track producer paths changed')
  files[row['metadata']]=library.encode({**metadata,'tracks':[track]});files[row['audio']]=audio_asset.build_bank([asset],bank['master_db'],bank['silence_db']);bindings.append(dict(track=row['track'],metadata=ref(row['metadata']),audio=ref(row['audio'])))
 doc=checked(dict(index=ref(p['index']),bindings=bindings));files[p['output']]=encode(doc);require(decode(files[p['output']])==doc,'Music bindings codec disagreement')
 proof=dict(source_commit=PIN,release_staged=False,source_library_review_sha256=hashlib.sha256((ROOT/'compatibility/reviews/music-library-v0410.json').read_bytes()).hexdigest(),recipe_sha256=hashlib.sha256(RECIPE.read_bytes()).hexdigest(),bindings=doc,outputs={name:dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())for name,data in files.items()},mapped_implementation={'descriptor_parser_and_admission':'runtime/music_library_bindings.cpp','existing_library_source_and_policy':'runtime/music_regions.cpp'},scope='Explicit checked index and one-track metadata/bank file bindings for existing 13-song root library; all 27 prior metadata files retain identical producer bytes, no PCM copied. Source library review and original root AudioBank remain required. Candidate only, no catalog/root program integration or device pass.')
 return p,doc,files,proof
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',required=True,type=Path);a=ap.parse_args();out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Music bindings candidates remain under build');p,doc,files,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/music-library-bindings-v0410.json').read_bytes()),'Music bindings semantic review changed');out.mkdir(parents=True,exist_ok=True)
 for name,data in files.items():
  target=(out/name).resolve();require(target.is_relative_to(out),'Music bindings output escaped owner');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 (out/'source.json').write_bytes((json.dumps(dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()),indent=2)+'\n').encode());print('Music library bindings:',len(doc['bindings']),'explicit tracks;',len(files),'metadata files; no PCM copied; not staged')
if __name__=='__main__':main()
