#!/usr/bin/env python3
"""Extract SDK payload from verified layers of the official devkitPro image.
No Docker daemon, root filesystem writes, or image entrypoint execution.
"""
import concurrent.futures,hashlib,json,pathlib,tarfile,urllib.request
root=pathlib.Path(__file__).resolve().parent
repo='devkitpro/devkitarm'
with urllib.request.urlopen('https://auth.docker.io/token?service=registry.docker.io&scope=repository:'+repo+':pull',timeout=60) as r: token=json.load(r)['token']
headers={'Authorization':'Bearer '+token}
man=json.load(open(root/'reference/devkitarm-manifest.json'))
if 'manifests' in man: man=json.load(open(root/'reference/devkitarm-amd64-manifest.json'))
def fetch(desc):
 digest=desc['digest']; dest=root/'downloads'/('layer-'+digest.split(':')[1]+'.tar.gz')
 if not dest.exists():
  req=urllib.request.Request('https://registry-1.docker.io/v2/'+repo+'/blobs/'+digest,headers=headers)
  with urllib.request.urlopen(req,timeout=180) as r, dest.open('wb') as f:
   while data:=r.read(1024*1024):f.write(data)
 h=hashlib.file_digest(dest.open('rb'),'sha256').hexdigest()
 assert h==digest.split(':')[1], 'SHA256 mismatch'
 print('Verified',dest.name,dest.stat().st_size,flush=True)
 return dest
# Skip base OS layer; retain SDK, package metadata and license payloads only.
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as ex: paths=list(ex.map(fetch,man['layers'][1:]))
for path in paths:
 with tarfile.open(path) as tar:
  members=[m for m in tar if m.name.lstrip('./').startswith(('opt/devkitpro/','opt/devkitpro','var/lib/pacman/local/','usr/local/share/licenses/'))]
  tar.extractall(root/'sdk-image',members=members,filter='data')
  print('Extracted',len(members),'entries from',path.name,flush=True)
(root/'reference/sdk-origin.txt').write_text('Official image: docker.io/devkitpro/devkitarm\nManifest SHA256: '+hashlib.sha256((root/'reference/devkitarm-manifest.json').read_bytes()).hexdigest()+'\n')
