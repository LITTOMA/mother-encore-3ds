from pathlib import Path
import hashlib,json,subprocess,sys
from PIL import Image
r=Path(__file__).resolve().parents[2];audit=json.loads((r/'build/full-port/shovel-next-source-audit.json').read_bytes());out=r/'build/full-port/shovel-visual-candidate-1';assert not out.exists();out.mkdir()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();base=r/'upstream/MOTHER-Encore'
for name,digest in audit['sources'].items():assert sha(base/name)==digest,name
license_file=base/'LICENSE';assert license_file.read_bytes()==(r/'docs/licenses/MOTHER-Encore-0.4.1.0-LICENSE.txt').read_bytes()
source_rects=audit['sparkles']['frames'];unique=list(dict.fromkeys(tuple(row)for row in source_rects));images=[]
with Image.open(base/'Graphics/Objects/Items/Shovel.png')as im:images.append(im.convert('RGBA'))
with Image.open(base/'Graphics/Objects/Common/Sparkles.png')as im:
 for x,y,w,h in unique:
  assert all(int(v)==v for v in(x,y,w,h));images.append(im.convert('RGBA').crop((int(x),int(y),int(x+w),int(y+h))))
assert len(images)==6 and images[0].size==(20,20)and all(im.size==(7,7)for im in images[1:])
canvas=Image.new('RGBA',(32,32),(0,0,0,0));x=y=row_height=0;frames=[]
for i,im in enumerate(images):
 if x+im.width>32:x=0;y+=row_height+1;row_height=0
 assert y+im.height<=32;canvas.paste(im,(x,y));assert canvas.crop((x,y,x+im.width,y+im.height)).tobytes()==im.tobytes();frames.append(dict(id=i,x=x,y=y,width=im.width,height=im.height));x+=im.width+1;row_height=max(row_height,im.height)
png=out/'atlas-00.png';texture=out/'atlas-00.t3x';canvas.save(png)
command=[sys.executable,str(r/'build/tooling/run_tex3ds_wsl.py'),'-f','rgba8888','-z','none','-o',str(texture),str(png)]
with(out/'conversion.log').open('wb')as f:subprocess.run(command,cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
assert texture.is_file()and texture.stat().st_size>4096
proof=dict(scope='Unpublished source visual candidate only; no runtime format, scene ownership, ready RNG, pickup, quest, GPU or hardware claim. Crops preserve original RGBA bytes; actual tex3ds invoked. No source or release assets modified.',upstream=audit['commit'],sources=audit['sources'],license_sha256=sha(license_file),permission='Pinned upstream LICENSE game-related port; original notices retained; no tool binary distributed',converter_sha256=sha(r/'build/tooling/pinned-tex3ds/tex3ds'),command=command,atlas=dict(width=32,height=32,frames=frames),sparkle_animation=dict(source=audit['sparkles'],frame_indices=[1+unique.index(tuple(v))for v in source_rects]),outputs={p.name:dict(bytes=p.stat().st_size,sha256=sha(p))for p in(png,texture)})
(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original Shovel20x20 +fiveuniqueSparkle7x7 frames/40timingframes; genuine texture',texture.stat().st_size,'bytes; candidate only')
