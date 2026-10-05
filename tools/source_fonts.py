#!/usr/bin/env python3
"""Source-pinned EBMain remaps; native Godot advances, lossless LA8 pages.
Does not modify upstream, source ASCII atlas, or locale translations.
"""
import argparse, collections, hashlib, json, os, re, shutil, struct, subprocess, sys, zlib
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, features, __version__ as pillow_version
from fontTools import __version__ as fonttools_version
from fontTools.ttLib import TTFont
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
PAGE=256
HEADER=struct.Struct('<8s6I')

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,v): Path(p).write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def scalar_text(text):
    # Source CSV contains literal C escapes; actual importer unescapes them.
    text=re.sub(r'\\(?:n|r|t|b|f|v)', '', text)
    # RichTextLabel BBCode tags are layout/control, never glyph requirements.
    text=re.sub(r'\[/?(?:color|wave|shake|rainbow|tornado|fade|center|right|fill|b|i|u|s|url|img|font|code|table|cell|indent|list|lb|rb)(?:=[^\]]*)?\]', '', text)
    return {ord(c) for c in text if ord(c)>=32 and ord(c)!=127}

def resource(root,path):
    text=(root/path).read_text()
    ext={i:f for f,i in re.findall(r'\[ext_resource path="res://([^"]+)"[^\n]*id=(\d+)\]',text)}
    chain=[ext[i] for i in re.findall(r'(?:font_data|fallback/\d+) = ExtResource\( (\d+) \)',text)]
    return chain

GODOT='''extends SceneTree
func _init():
 var version=Engine.get_version_info()
 if version.major!=3 or version.minor!=6 or version.patch!=2 or version.hash!="3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8":
  quit(7)
  return
 var input=File.new()
 if input.open("res://request.json",File.READ)!=OK:
  quit(2)
  return
 var request=JSON.parse(input.get_as_text()).result
 var faces=[]
 for spec in request:
  var font=load("res://"+spec.source)
  var advances=[]
  for cp in spec.codepoints:
   advances.append(font.get_char_size(int(cp)).x)
  faces.append({"source":spec.source,"size":font.size,"ascent":font.get_ascent(),"descent":font.get_descent(),"height":font.get_height(),"advances":advances})
 var output=File.new()
 if output.open("res://metrics.json",File.WRITE)!=OK:
  quit(3)
  return
 output.store_string(JSON.print({"version":version,"faces":faces}))
 output.close()
 quit()
'''

def compile_fonts(args):
    root=Path(args.source).resolve();cat=Path(args.catalog).resolve();out=Path(args.output).resolve();build=Path(args.build).resolve()
    review=json.loads(Path(args.review).read_text());catalog=json.loads(cat.read_text())
    if review['schema']!=1 or review['commit']!=catalog['commit']: raise ValueError('Unreviewed source commit')
    head=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    if head!=review['commit']: raise ValueError('Source checkout differs from reviewed commit')
    for p,h in review['sources'].items():
        if sha(root/p)!=h: raise ValueError('Source changed: '+p)
    out.mkdir(parents=True,exist_ok=True);build.mkdir(parents=True,exist_ok=True)
    project=build/'godot-project';(project/'Fonts').mkdir(parents=True,exist_ok=True)
    groups=collections.defaultdict(set);priority=collections.defaultdict(set)
    names=''.join(l['name'] for l in catalog['locales'])
    bindings={x.get('key') for x in catalog.get('house_bindings',[]) if isinstance(x,dict)}
    # Cover ASCII in remapped faces and language names in all faces. Latin ASCII
    # is measured but never substituted for the existing shipped atlas.
    for locale in catalog['locales']:
        p=locale['font'];groups[p].update(range(32,127));groups[p].update(scalar_text(names));priority[p].update(groups[p])
        for row in catalog['records']:
            text=row['values'].get(locale['code'],'') or row['values'].get('en','')
            cps=scalar_text(text);groups[p].update(cps)
            if row['key'] in bindings: priority[p].update(cps)
    # PSI level symbols are literal source labels outside the CSV catalogue.
    # Number labels retain the existing BottleRocket renderer and ASCII atlas.
    from tools.field_psi import font_bindings,load as psi_load
    psi=psi_load();extra=font_bindings()
    if psi['source_font'] not in review['sources']:raise ValueError('Unreviewed PSI text font')
    number=extra.pop(psi['number_font'])
    if any(any(ord(c)>126 or ord(c)<32 for c in text)for text in number):raise ValueError('PSI number label requires unadmitted glyph')
    cps=set().union(*(scalar_text(text)for texts in extra.values()for text in texts))
    for p in groups:groups[p].update(cps)
    requests=[];cmaps={};credits={};chains={}
    for path,cps in groups.items():
        if path not in review['sources']: raise ValueError('Font role is not reviewed: '+path)
        chains[path]=resource(root,path)
        shutil.copyfile(root/path,project/path)
        for p in chains[path]:
            if p not in review['sources']: raise ValueError('Fallback not reviewed: '+p)
            shutil.copyfile(root/p,project/p)
            if p not in cmaps:
                ft=TTFont(root/p);cmaps[p]=ft.getBestCmap()
                credits[p]={str(i):sorted({n.toUnicode() for n in ft['name'].names if n.nameID==i}) for i in (0,8,9,13,14)}
        requests.append({'source':path,'codepoints':sorted(cps)})
    dump(project/'request.json',requests);(project/'project.godot').write_text('config_version=4\n');(project/'metrics.gd').write_text(GODOT)
    result=subprocess.run([args.godot,'--path',str(project),'-s','metrics.gd'],text=True,capture_output=True,check=True,timeout=90,env=dict(os.environ,XDG_DATA_HOME=str(build/'userdata')))
    (build/'godot.log').write_text(result.stdout+result.stderr);metrics=json.loads((project/'metrics.json').read_text())
    pages=[];faces=[];all_glyphs=[];missing=[];bitmaps={}
    for request,metric in zip(requests,metrics['faces']):
        path=request['source'];gid=len(all_glyphs);pid=len(pages);glyphs=[]
        native=dict(zip(request['codepoints'],metric['advances']))
        order=sorted(request['codepoints'],key=lambda cp:(cp not in priority[path],cp))
        image=None;x=y=rowh=0;localpage=-1
        def flush():
            if image is None:return
            stem='ebmain-%d-%03d'%(len(faces),localpage);png=build/(stem+'.png');target=out/(stem+'.t3x')
            image.save(png)
            subprocess.run([args.tex3ds,'-f','la8','-z','none','-o',str(target),str(png)],check=True,capture_output=True)
            raw=target.read_bytes();pages.append({'path':target.name,'width':PAGE,'height':PAGE,'texture_bytes':PAGE*PAGE*2,'file_bytes':len(raw),'crc32':zlib.crc32(raw),'sha256':sha(target)})
        for cp in order:
            face=next((p for p in chains[path] if cp in cmaps[p]),None)
            if face is None:
                missing.append({'source':path,'codepoint':cp,'unicode':'U+%04X'%cp,'native_advance':native[cp]});continue
            key=(face,metric['size'])
            if key not in bitmaps:bitmaps[key]=ImageFont.truetype(str(root/face),metric['size'])
            font=bitmaps[key];character=chr(cp);advance=float(font.getlength(character))
            if abs(advance-native[cp])>.001:raise ValueError('Advance differs from native Godot: %s U+%04X Pillow=%s Godot=%s'%(path,cp,advance,native[cp]))
            bbox=font.getbbox(character,anchor='ls');w=bbox[2]-bbox[0];h=bbox[3]-bbox[1]
            if w>PAGE-2 or h>PAGE-2 or w<0 or h<0:raise ValueError('Unbounded source glyph')
            if image is None:
                image=Image.new('RGBA',(PAGE,PAGE),(255,255,255,0));localpage+=1;x=y=rowh=1
            if x+w+1>PAGE:x=1;y+=rowh+1;rowh=1
            if y+h+1>PAGE:
                flush();image=Image.new('RGBA',(PAGE,PAGE),(255,255,255,0));localpage+=1;x=y=rowh=1
            # Baseline anchor matches Godot's font-ascent origin, including fallback.
            if w and h:ImageDraw.Draw(image).text((x-bbox[0],y-bbox[1]),character,font=font,anchor='ls',fill=(255,255,255,255))
            glyphs.append({'codepoint':cp,'page':pid+localpage,'u':x,'v':y,'width':w,'height':h,'advance':native[cp],'offset_x':bbox[0],'offset_y':metric['ascent']+bbox[1],'source_face':face})
            x+=w+2;rowh=max(rowh,h)
        flush();glyphs.sort(key=lambda g:g['codepoint']);all_glyphs.extend(glyphs)
        faces.append({'source':path,'legacy_ascii':path==review['legacy_ascii_resource'],'first_glyph':gid,'glyph_count':len(glyphs),'first_page':pid,'page_count':len(pages)-pid,'ascent':metric['ascent'],'descent':metric['descent'],'height':metric['height'],'texture_bytes':sum(p['texture_bytes'] for p in pages[pid:]),'fallback_chain':chains[path]})
        if faces[-1]['texture_bytes']>2*1024*1024:raise ValueError('Selected face exceeds admitted 2 MiB budget')
    payload=bytearray()
    def string(value):
        raw=value.encode('ascii');payload.extend(struct.pack('<I',len(raw)));payload.extend(raw)
    for f in faces:
        string(f['source']);payload.extend(struct.pack('<5I3f',int(f['legacy_ascii']),f['first_glyph'],f['glyph_count'],f['first_page'],f['page_count'],f['ascent'],f['descent'],f['height']))
    for p in pages:
        string(p['path']);payload.extend(struct.pack('<5I',p['width'],p['height'],p['texture_bytes'],p['file_bytes'],p['crc32']))
    for g in all_glyphs:payload.extend(struct.pack('<6I3f',*(g[k] for k in ('codepoint','page','u','v','width','height','advance','offset_x','offset_y'))))
    binary=HEADER.pack(b'ENCFONT\0',1,len(payload),zlib.crc32(payload),len(faces),len(pages),len(all_glyphs))+payload
    target=out/'source-fonts.encfont';target.write_bytes(binary)
    receipt={'schema':1,'field_psi_sha256':sha(ROOT/'content/native-field-psi.json'),'review':review,'catalog_sha256':sha(cat),'generator_sha256':sha(__file__),'godot_sha256':sha(args.godot),'tex3ds_sha256':sha(args.tex3ds),'pillow_version':pillow_version,'freetype_version':features.version_module('freetype2'),'fonttools_version':fonttools_version,'metrics':metrics,'faces':faces,'pages':pages,'glyphs':all_glyphs,'missing_source_glyphs':missing,'font_embedded_notices':credits,'binary':{'path':target.name,'sha256':sha(target),'bytes':len(binary)},'limits':'EBMain role only; no new fonts or invented glyph replacements. Native Godot 3.6.2 advances checked for every included glyph; baseline from native ascent. Glyph pixels are Pillow/FreeType rasters; no rendered Godot/GPU pixel or hardware comparison claimed. Missing source glyphs are not encoded and reject at lookup. All assets remain game-related derivatives, not MIT-relicensed.'}
    receipt_path=ROOT/'content/asset-receipts/fonts/source.json'
    receipt_path.parent.mkdir(parents=True,exist_ok=True)
    dump(receipt_path,receipt)
    print(json.dumps({'faces':[{k:f[k] for k in ('source','glyph_count','page_count','texture_bytes')} for f in faces],'pages':len(pages),'missing':missing,'binary_bytes':len(binary)}))

def stage_files(root):
    """Verify reviewed inputs and supplied RomFS bytes; return fonts/* mapping.

    ROOT supplies the reviewed receipt outside RomFS. `root` is a RomFS tree,
    allowing the normal packager to verify a separate staging tree as well.
    Only the font pack and texture pages are included; redistribution notices
    are supplied by release_notices under licenses/.
    This path never invokes Godot/tex3ds or changes any source/output file.
    """
    root=Path(root)
    review=json.loads((ROOT/'content/source-fonts-review.json').read_text())
    if review.get('schema')!=1: raise ValueError('Unknown source font review schema')
    upstream=ROOT/'upstream/MOTHER-Encore'
    head=subprocess.check_output(['git','-C',str(upstream),'rev-parse','HEAD'],text=True).strip()
    if head!=review['commit']: raise ValueError('Source font upstream commit changed')
    for name,digest in review['sources'].items():
        rel=Path(name)
        if rel.is_absolute() or '..' in rel.parts or '\\' in name:
            raise ValueError('Unsafe reviewed font source path')
        if sha(upstream/rel)!=digest: raise ValueError('Source changed: '+name)
    manifest=json.loads((ROOT/'content/asset-receipts/fonts/source.json').read_bytes())
    if manifest.get('schema')!=1 or manifest.get('review')!=review:
        raise ValueError('Source font manifest review changed')
    if manifest.get('field_psi_sha256')!=sha(ROOT/'content/native-field-psi.json'):raise ValueError('Source font PSI labels changed; regenerate assets')
    if manifest.get('generator_sha256')!=sha(__file__):
        raise ValueError('Source font generator changed; regenerate assets')
    if manifest.get('catalog_sha256')!=sha(ROOT/'content/native-localization.json'):
        raise ValueError('Source font locale catalogue changed; regenerate assets')
    files={}
    def output(name,expected_hash,expected_bytes,expected_crc=None):
        if not isinstance(name,str) or not name or Path(name).name!=name or name in ('.','..') or ':' in name or '\\' in name:
            raise ValueError('Unsafe staged source font output path')
        path=Path('fonts')/name
        if path in files: raise ValueError('Duplicate staged source font output')
        raw=(root/path).read_bytes()
        if len(raw)!=expected_bytes or hashlib.sha256(raw).hexdigest()!=expected_hash:
            raise ValueError('Staged source font size/hash mismatch: '+name)
        if expected_crc is not None and zlib.crc32(raw)!=expected_crc:
            raise ValueError('Staged source font page CRC mismatch: '+name)
        files[path]=raw
        return raw
    binary=manifest['binary']
    if binary['path']!='source-fonts.encfont': raise ValueError('Unrecognized source font pack identity')
    raw=output(binary['path'],binary['sha256'],binary['bytes'])
    if len(raw)<HEADER.size: raise ValueError('Truncated staged source font pack')
    magic,version,length,crc,nf,np,ng=HEADER.unpack_from(raw)
    if (magic!=b'ENCFONT\0' or version!=1 or length!=len(raw)-HEADER.size or crc!=zlib.crc32(raw[HEADER.size:]) or nf!=len(manifest['faces']) or np!=len(manifest['pages']) or ng!=len(manifest['glyphs'])):
        raise ValueError('Staged source font header/count/CRC mismatch')
    for page in manifest['pages']:
        output(page['path'],page['sha256'],page['file_bytes'],page['crc32'])
    return files

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--catalog',required=True);p.add_argument('--godot',required=True);p.add_argument('--tex3ds',required=True);p.add_argument('--review',default=str(ROOT/'content/source-fonts-review.json'));p.add_argument('--output',default=str(ROOT/'romfs/fonts'));p.add_argument('--build',default=str(ROOT/'build/multilingual-fonts'));compile_fonts(p.parse_args())
