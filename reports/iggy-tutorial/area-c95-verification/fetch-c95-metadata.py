from pathlib import Path
import urllib.request,urllib.error,subprocess,json,zipfile,hashlib
r=Path(__file__).resolve().parents[2];out=r/'build/full-port/c95-console-metadata';out.mkdir(parents=True,exist_ok=True)
meta=json.loads(subprocess.check_output(['gh','api','repos/LITTOMA/mother-encore-3ds/actions/artifacts/12214386780']));commit='c95ade5a0ba689d1ffcdaf835ad806431e844080';assert meta['workflow_run']['head_sha']==commit;size=meta['size_in_bytes']
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args):return None
token=subprocess.check_output(['gh','auth','token'],text=True).strip()
try:urllib.request.build_opener(NoRedirect()).open(urllib.request.Request(meta['archive_download_url'],headers={'Authorization':'Bearer '+token},method='HEAD'),timeout=45);raise RuntimeError('Expected official redirect')
except urllib.error.HTTPError as error:
 if error.code not in(302,303):raise RuntimeError('Artifact metadata HTTP '+str(error.code))from None
 url=error.headers['Location']
del token
def part(lo,hi):
 assert 0<=lo<=hi<size and hi-lo<1024*1024
 with urllib.request.urlopen(urllib.request.Request(url,headers={'Range':'bytes='+str(lo)+'-'+str(hi)}),timeout=45)as response:
  assert response.status==206 and response.headers['Content-Range']=='bytes '+str(lo)+'-'+str(hi)+'/'+str(size);raw=response.read(hi-lo+2)
 assert len(raw)==hi-lo+1;return raw
class Remote:
 def __init__(self):self.pos=0;self.tail=part(size-131072,size-1)
 def seek(self,n,whence=0):self.pos=n if whence==0 else self.pos+n if whence==1 else size+n;return self.pos
 def tell(self):return self.pos
 def seekable(self):return True
 def read(self,n=-1):
  n=min(size-self.pos,n if n>=0 else size-self.pos);start=self.pos;self.pos+=n
  if n==0:return b''
  if start>=size-len(self.tail):return self.tail[start-size+len(self.tail):start-size+len(self.tail)+n]
  return part(start,start+n-1)
with zipfile.ZipFile(Remote())as archive:
 for info in archive.infolist():
  if info.filename in('build-receipt.json','BUILD_INFO.txt','SHA256SUMS'):
   assert info.file_size<65536;(out/info.filename).write_bytes(archive.read(info))
receipt=json.loads((out/'build-receipt.json').read_bytes());assert receipt['source_commit']==commit
game=r/'build/full-port/iggy-f029-current/verified-artifact/3ds/encore-native/encore-native.3dsx'
with game.open('rb')as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
expected=receipt['artifacts']['encore-native.3dsx'];equal=game.stat().st_size==expected['bytes']and digest==expected['sha256']
proof=dict(source_commit=commit,run=39305476368,artifact=12214386780,selected_metadata_zip_crc_verified=True,full_archive_downloaded=False,archive_digest_declared_by_github=meta['digest'],actual_f029_game_sha256=digest,c95_declared_game=expected,identical_actual_game=equal,scope='Only official build receipt/BUILD_INFO/SHA256SUMS fetched. Actual accepted f029 game compared to c95 receipt; no separate c95game download or fullarchivehash claim.')
(out/'identity.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('c95 game identical to actual emulator-tested f029 game:',equal,expected)
