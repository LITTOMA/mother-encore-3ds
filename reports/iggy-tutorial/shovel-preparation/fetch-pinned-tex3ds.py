from pathlib import Path
import urllib.request,urllib.error,json,hashlib,tarfile,time
r=Path(__file__).resolve().parents[2];root=r/'build/tooling/pinned-tex3ds';root.mkdir(parents=True,exist_ok=True)
manifest=json.loads((r/'tools/cloud-sdk/reference/devkitarm-amd64-manifest.json').read_bytes());desc=manifest['layers'][-1]
assert desc['digest']=='sha256:3c5026e520aeec9e208399f6d0a62098d5ee4c13d8cca97cdb9adbc6bb56a1c0'and desc['size']==215133357
expected=json.loads((r/'content/asset-receipts/graphics/world/podunk/props/source.json').read_bytes())['tex3ds_sha256']
repo='devkitpro/devkitarm'
with urllib.request.urlopen('https://auth.docker.io/token?service=registry.docker.io&scope=repository:'+repo+':pull',timeout=30)as response:token=json.load(response)['token']
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args):return None
url='https://registry-1.docker.io/v2/'+repo+'/blobs/'+desc['digest'];opener=urllib.request.build_opener(NoRedirect())
try:
 response=opener.open(urllib.request.Request(url,headers={'Authorization':'Bearer '+token}),timeout=45)
except urllib.error.HTTPError as error:
 if error.code not in(302,303,307):raise RuntimeError('Official SDK registry HTTP '+str(error.code))from None
 # Never forward registry authorization to the object download host.
 response=urllib.request.urlopen(error.headers['Location'],timeout=45)
del token
archive=root/'verified-sdk-tools-layer.tar.gz';h=hashlib.sha256();size=0;next_progress=32*1024*1024
with response,archive.open('wb')as f:
 while True:
  block=response.read(1024*1024)
  if not block:break
  size+=len(block)
  if size>desc['size']:raise RuntimeError('Official pinned layer exceeds declared size')
  h.update(block);f.write(block)
  if size>=next_progress:print('Pinned tool layer bytes',size,flush=True);next_progress+=32*1024*1024
assert size==desc['size']and h.hexdigest()==desc['digest'].split(':')[1]
kept=[]
with tarfile.open(archive,'r:gz')as tar:
 for member in tar:
  name=member.name.lstrip('./')
  if name=='opt/devkitpro/tools/bin/tex3ds'or(name.startswith('var/lib/pacman/local/tex3ds-')and name.endswith('/desc')):
   assert member.isfile()and member.size<=32*1024*1024
   dest=root/('tex3ds'if name.endswith('/tex3ds')else'package-desc.txt')
   with tar.extractfile(member)as source:raw=source.read()
   assert len(raw)==member.size;dest.write_bytes(raw);kept.append(dict(member=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
tool=root/'tex3ds';assert hashlib.sha256(tool.read_bytes()).hexdigest()==expected,'Actual tool differs from existing admitted conversion receipt'
proof=dict(official_image=repo,manifest_sha256=hashlib.sha256((r/'tools/cloud-sdk/reference/devkitarm-amd64-manifest.json').read_bytes()).hexdigest(),layer=desc,verified_layer_bytes=size,converter_sha256=expected,kept=kept,scope='Only the existing source-admitted converter and its package metadata retained locally. Full layer checksum verified; no SDK installation, root filesystem mutation, release binary or game asset created.')
(root/'proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');archive.unlink();print('Existing admitted genuine tex3ds retained:',expected,'; temporary layer bytes removed:',size,flush=True)
