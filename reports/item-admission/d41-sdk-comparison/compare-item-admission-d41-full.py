from pathlib import Path
import hashlib,json,subprocess,urllib.request,urllib.error,zipfile
root=Path(__file__).resolve().parents[2];base=root/'build/full-port/item-admission-d41-full-official';base.mkdir(exist_ok=False)
head='d41c04698df543e40b5c19788faa5b0d9e2c684f';run=39771295093;artifact=12422724217
meta=json.loads(subprocess.check_output(['gh','api','repos/LITTOMA/mother-encore-3ds/actions/artifacts/'+str(artifact)],cwd=root));assert meta['workflow_run']['head_sha']==head and not meta['expired']
(base/'artifacts-verified.json').write_text(json.dumps(dict(artifacts=[meta]),indent=2)+'\n');size=meta['size_in_bytes']
proxy=urllib.request.ProxyHandler({'http':'http://127.0.0.1:7890','https':'http://127.0.0.1:7890'})
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args):return None
token=subprocess.check_output(['gh','auth','token'],cwd=root,text=True).strip()
try:
    urllib.request.build_opener(proxy,NoRedirect()).open(urllib.request.Request(meta['archive_download_url'],headers={'Authorization':'Bearer '+token,'User-Agent':'selected-console-validation'},method='HEAD'),timeout=45);raise RuntimeError('Missing redirect')
except urllib.error.HTTPError as error:
    if error.code not in(302,303):raise RuntimeError('Artifact HTTP '+str(error.code))from None
    url=error.headers['Location']
del token
opener=urllib.request.build_opener(proxy)
def get(lo,hi):
    with opener.open(urllib.request.Request(url,headers={'Range':f'bytes={lo}-{hi}'}),timeout=45)as response:
        assert response.status==206 and response.headers.get('Content-Range')==f'bytes {lo}-{hi}/{size}';raw=response.read(hi-lo+2)
    assert len(raw)==hi-lo+1;return raw
class Remote:
    def __init__(self):self.pos=0;self.tail=get(max(0,size-131072),size-1)
    def seek(self,n,whence=0):self.pos=n if whence==0 else self.pos+n if whence==1 else size+n;return self.pos
    def tell(self):return self.pos
    def seekable(self):return True
    def read(self,n=-1):
        n=min(size-self.pos,n if n>=0 else size-self.pos);start=self.pos;self.pos+=n
        if not n:return b''
        if start>=size-len(self.tail):return self.tail[start-size+len(self.tail):start-size+len(self.tail)+n]
        return get(start,start+n-1)
dest=base/'verified-artifact';dest.mkdir()
with zipfile.ZipFile(Remote())as archive:
    for info in archive.infolist():
        if info.filename.endswith(('receipt.json','SHA256SUMS','BUILD_INFO.txt')):
            path=(dest/info.filename).resolve();assert dest.resolve()in path.parents and info.file_size<1024*1024
            path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(archive.read(info))
receipt=json.loads((dest/'build-receipt.json').read_bytes());assert receipt['source_commit']==head
game=root/'build/full-port/item-admission-bb-official/verified-artifact/3ds/encore-native/encore-native.3dsx';actual=hashlib.sha256(game.read_bytes()).hexdigest();identity=receipt['artifacts']['encore-native.3dsx']
assert receipt['staged_files']==383 and game.stat().st_size==identity['bytes']and actual==identity['sha256']=='a51308775df4f125a0d09dfa434079a619733bf98298ce19f944cb89383a24cf'
proof=dict(source=head,run=run,artifact=artifact,actual_retained_bb_game_sha256=actual,official_d41_game_sha256=identity['sha256'],byte_identical=True,game_bytes=identity['bytes'],staged_files=receipt['staged_files'],full_archive_downloaded=False,game_downloaded_again=False,scope='Official d41 receipt compared to actually retained and independently ZIP/CRC/source-verified BBgame. Emulator result remains the actual run only; none declared here.')
(base/'byte-equivalence.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof),flush=True)
