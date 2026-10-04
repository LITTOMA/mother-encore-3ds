#!/usr/bin/env python3
"""Compile only source locale-remapped title options; base Continue art is shared.

No native title background, primary title layer, or English option is copied.
Source remaps and highlight values come from the pinned project and title script.
"""
from __future__ import annotations
import argparse,hashlib,io,json,os,re,struct,subprocess,sys,zlib
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
SOURCE_ROOT=ROOT if (ROOT/'upstream/MOTHER-Encore').exists() else ROOT.parents[1]/'encore-native'
RECIPE=ROOT/'content/native-title-locales.json'
PACK=ROOT/'romfs/data/opening.enctitlelocale'
LOCALES=('zh_Hans_CN',)  # Explicit first native rendering checkpoint.

def require(ok,message):
    if not ok:raise ValueError(message)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def write(p,value):
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(json.dumps(value,ensure_ascii=False,indent=2,sort_keys=True)+'\n')

def extract():
    sys.path.insert(0,str(SOURCE_ROOT))
    from tools.extract_battle_entry import Extractor,one,variant
    ex=Extractor(SOURCE_ROOT)
    project=ex.text('project.godot')
    block=one(r'^translation_remaps=(\{.*?^\})',project,'source translation remaps',re.M|re.S)[1]
    remaps=variant(block)
    fallback=one(r'^const LANGUAGE_DEFAULT := "([^"]+)"',ex.text('Scripts/global/global.gd'),'source default locale')[1]
    script=ex.text('Scripts/UI/Title screen.gd')
    flash=float(one(r'flash_modifier", (0\.\d+)\)',script,'source title highlight')[1])
    base_path=SOURCE_ROOT/'content/native-continue.json';base=read(base_path)
    require(base['commit']==ex.lock['commit'] and len(base['title_options'])==4,'Unexpected base title source/count')
    resources=[]
    for locale in LOCALES:
        for option in base['title_options']:
            normal=base['resources'][option['resource']]
            selected=base['resources'][option['selected_resource']]
            require(normal['source']==selected['source'] and not normal['crop'] and not normal['tint'] and not normal['flash'] and selected['flash']==flash and not selected['crop'] and not selected['tint'],'Unreviewed source title option transform')
            source_key='res://'+normal['source']
            require(source_key in remaps,'Missing title source remap')
            translated=[entry.rsplit(':',1)[0] for entry in remaps[source_key] if entry.rsplit(':',1)[-1]==locale]
            require(len(translated)==1 and translated[0].startswith('res://'),'Missing/ambiguous locale title remap')
            source=translated[0][6:];size=ex.png_size(source);ex.data(source+'.import')
            require(all(0<v<=1024 for v in size),'Title remap dimensions exceed renderer')
            for resource in (normal,selected):
                resources.append(dict(locale=locale,base_path=resource['path'],path='graphics/ui/title/'+locale+'/'+Path(resource['path']).name,source=source,width=size[0],height=size[1],flash=resource['flash']))
    require(len(resources)==8 and len({r['path'] for r in resources})==8,'Title remap coverage changed')
    base_outputs={r['path']:sha(SOURCE_ROOT/'romfs'/r['path']) for r in base['resources']}
    return ex,dict(schema=1,commit=ex.lock['commit'],fallback=fallback,sources=ex.sources,
                   scope='Only Simplified Chinese source title option texture remaps, normal and selected; unchanged shared English and primary title layers.',
                   dependencies={'content/native-continue.json':sha(base_path)},
                   base_readonly_outputs=base_outputs,resources=resources)

def encode(recipe):
    payload=bytearray()
    def integer(v):payload.extend(struct.pack('<I',v))
    def string(s):
        raw=s.encode('ascii');integer(len(raw));payload.extend(raw)
    string(recipe['fallback']);integer(len(recipe['resources']))
    for r in recipe['resources']:
        for key in ('locale','base_path','path'):string(r[key])
        integer(r['width']);integer(r['height'])
    return struct.pack('<8s4I',b'ENCTLCL1',1,24+len(payload),zlib.crc32(payload),1)+payload

def verify_recipe(recipe):
    _,source=extract()
    require(set(recipe)==set(source)|{'outputs'},'Unknown title locale recipe fields')
    for key,value in source.items():require(recipe[key]==value,'Title locale source drift: '+key)
    require(set(recipe['outputs'])=={r['path'] for r in source['resources']},'Title locale output coverage drift')
    for path,out in recipe['outputs'].items():
        p=ROOT/'romfs'/path
        require(p.stat().st_size==out['bytes'] and sha(p)==out['sha256'],'Title locale texture drift: '+path)

def assets(tex3ds):
    ex,recipe=extract();build=ROOT/'build/title-locale-assets';build.mkdir(parents=True,exist_ok=True)
    recipe['outputs']={}
    for r in recipe['resources']:
        img=Image.open(io.BytesIO(ex.data(r['source']))).convert('RGBA')
        # Exactly the continue_assets.py selected-title transform, including
        # Python round and unchanged alpha. Never translate with a system font.
        if r['flash']:img.putdata([tuple(round(v+(255-v)*r['flash']) for v in px[:3])+(px[3],) for px in img.getdata()])
        require(list(img.size)==[r['width'],r['height']],'Title remap geometry changed')
        png=build/(r['locale']+'-'+Path(r['path']).stem+'.png');img.save(png)
        out=ROOT/'romfs'/r['path'];out.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(out),str(png)],check=True)
        recipe['outputs'][r['path']]={'sha256':sha(out),'bytes':out.stat().st_size}
    verify_recipe(recipe);write(RECIPE,recipe);PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(recipe))
    print('Compiled 8 source title option textures and checked external metadata; shared base art unchanged')

def stage_files(source_root):
    recipe=read(RECIPE);verify_recipe(recipe);root=Path(source_root).resolve();files={}
    for path in ['data/opening.enctitlelocale',*recipe['outputs']]:
        rel=Path(path);p=(root/rel).resolve();require(p.is_relative_to(root),'Title locale stage path escape')
        data=p.read_bytes();require(data==encode(recipe) if path=='data/opening.enctitlelocale' else hashlib.sha256(data).hexdigest()==recipe['outputs'][path]['sha256'],'Stale staged title locale bytes')
        files[rel]=data
    return files

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('command',choices=['assets','compile','verify'])
    ap.add_argument('--source-root',type=Path,default=SOURCE_ROOT)
    ap.add_argument('--tex3ds',type=Path,default=Path(os.environ.get('DEVKITPRO','/opt/devkitpro'))/'tools/bin/tex3ds')
    args=ap.parse_args();SOURCE_ROOT=args.source_root.resolve()
    if args.command=='assets':assets(args.tex3ds)
    else:
        recipe=read(RECIPE);verify_recipe(recipe)
        if args.command=='compile':PACK.write_bytes(encode(recipe))
        else:require(PACK.read_bytes()==encode(recipe),'Title locale binary drift');print('Verified title locale metadata/textures and unchanged English/primary title assets')
