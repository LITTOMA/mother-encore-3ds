#!/usr/bin/env python3
"""Pinned opening cutscene art and original DynamicFont resources, offline only."""
from __future__ import annotations
import argparse, ctypes, ctypes.util, csv, hashlib, io, json, math, os, re, shutil, struct, subprocess, sys, zlib
from pathlib import Path, PurePosixPath
from PIL import Image, ImageDraw, ImageFont, features, __version__ as pillow_version
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.upstream import write_json
HEADER=struct.Struct("<8s6I")
PAGE=256
RECIPE=ROOT/'content/introduction-assets.json'
ART_RECEIPT=Path('content/asset-receipts/graphics/cutscenes/introduction/source.json')
FONT_RECEIPT=Path('content/asset-receipts/fonts/introduction/source.json')


def require(ok,message):
    if not ok:raise ValueError(message)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def exact(obj,fields,label):require(type(obj)is dict and set(obj)==set(fields),'Unknown/missing '+label+' fields')
def unique(pairs):
    out={}
    for k,v in pairs:require(k not in out,'Duplicate JSON field');out[k]=v
    return out
def read(path):return json.loads(Path(path).read_bytes().decode('utf-8'),object_pairs_hook=unique)
def safe(root,path):
    require(type(path)is str and path and path.isascii()and not any(c in path for c in ':\\')and all(p not in ('','.','..')for p in path.split('/')),'Unsafe resource path')
    p=PurePosixPath(path);require(not p.is_absolute(),'Absolute resource path')
    target=(Path(root)/path).resolve();require(Path(root).resolve()in target.parents,'Resource escapes root');return target

def font_definition(root,path):
    """The admitted original DynamicFont property and data forms only."""
    text=safe(root,path).read_text(encoding='utf-8');external={i:{'path':p,'antialiased':True,'hinting':2}for p,i in re.findall(r'\[ext_resource path="res://([^"]+)" type="DynamicFontData" id=(\d+)\]',text)}
    internal={}
    for ident,body in re.findall(r'\[sub_resource type="DynamicFontData" id=(\d+)\]\n(.*?)(?=^\[|\Z)',text,re.M|re.S):
        properties=dict(re.findall(r'^([a-z_]+) = (.+)$',body,re.M));require(set(properties)<= {'font_path','antialiased'}and 'font_path'in properties,'Unknown DynamicFontData content')
        p=json.loads(properties['font_path']);require(p.startswith('res://'),'Invalid original font path');aa=properties.get('antialiased','true');require(aa in ('true','false'),'Unknown antialias mode');internal[ident]={'path':p[6:],'antialiased':aa=='true','hinting':2}
    sections=re.findall(r'^\[resource\]\n(.*?)(?=^\[|\Z)',text,re.M|re.S);require(len(sections)==1,'Missing original DynamicFont body')
    props=dict(re.findall(r'^([^\n=]+) = (.+)$',sections[0],re.M))
    supported={'size','extra_spacing_top','extra_spacing_bottom','extra_spacing_char','extra_spacing_space','outline_size','outline_color','font_data'}
    require('font_data'in props and all(k in supported or re.fullmatch(r'fallback/[0-9]+',k)for k in props),'Unknown DynamicFont property')
    chain=[];fallback=sorted((int(k.split('/')[1]),k)for k in props if k.startswith('fallback/'));require([i for i,_ in fallback]==list(range(len(fallback))),'Noncontiguous fallback order')
    for key in ['font_data']+[k for _,k in fallback]:
        m=re.fullmatch(r'(ExtResource|SubResource)\( (\d+) \)',props[key]);require(m is not None,'Unknown font-data reference');table=external if m[1]=='ExtResource'else internal;require(m[2]in table,'Unbound font data');record=table[m[2]].copy();safe(root,record['path']);chain.append(record)
    values={k:props[k]for k in props if not k.startswith('fallback/')and k!='font_data'}
    for k,v in values.items():
        if k=='outline_color':require(re.fullmatch(r'Color\( 0, 0, 0, 1 \)',v)is not None,'Only original black font outline supported')
        else:require(re.fullmatch(r'-?[0-9]+',v)is not None,'Noninteger DynamicFont property')
    return {'properties':values,'fallback_chain':chain}

def node_texture(root,scene,path,property_name="texture"):
    text=safe(root,scene).read_text(encoding='utf-8');ext={i:p for p,i in re.findall(r'\[ext_resource path="res://([^"]+)"[^\n]*id=(\d+)\]',text)}
    found=[]
    for m in re.finditer(r'^\[node name="([^"]+)"([^\n]*)\]\n(.*?)(?=^\[node|^\[connection|\Z)',text,re.M|re.S):
        parent=re.search(r'parent="([^"]+)"',m[2]);full=(parent[1]+'/'if parent and parent[1]!='.'else'')+m[1]
        if full==path:found.append(m[3])
    require(len(found)==1,'Missing/duplicate source texture node '+path);m=re.search(r'^'+re.escape(property_name)+r' = ExtResource\( (\d+) \)',found[0],re.M);require(m and m[1]in ext,'Unknown source texture reference');return ext[m[1]],found[0]

def translations(upstream,recipe):
    records={}
    for path in recipe['translation_sources']:
        rows=csv.reader(io.StringIO(safe(upstream,path).read_text(encoding='utf-8-sig')));header=next(rows);require(header[0]=='key','Unknown source translation schema')
        for row in rows:
            if not row:continue
            require(len(row)==len(header),'Malformed source translation row')
            if row[0]in recipe['text_keys']:
                require(row[0]not in records,'Duplicate cutscene translation key');records[row[0]]={locale:(row[header.index(code)]or row[header.index('en')]).replace('\\n','\n').replace('\\r','\r').replace('\\t','\t')for locale,code in recipe['locales'].items()}
    require(set(records)==set(recipe['text_keys']),'Missing cutscene source text');return records

def validate(recipe,project=ROOT,recipe_path=RECIPE):
    project=Path(project);upstream=project/'upstream/MOTHER-Encore';lock=read(project/'upstream.lock')
    exact(recipe,('schema','kind','commit','licence_review','sources','resources','font_catalog','fonts','locales','translation_sources','text_keys'),'introduction asset recipe')
    require(type(recipe['schema'])is int and recipe['schema']==1 and recipe['kind']=='encore.introduction-assets'and recipe['commit']==lock['commit']and type(recipe['licence_review'])is str and recipe['licence_review'],'Unreviewed introduction asset schema/permission')
    require(subprocess.check_output(['git','-C',str(upstream),'rev-parse','HEAD'],text=True).strip()==recipe['commit'],'Upstream checkout differs')
    require(type(recipe['sources'])is dict and recipe['sources'],'Missing asset source review')
    for path,pin in recipe['sources'].items():require(type(pin)is str and re.fullmatch('[0-9a-f]{64}',pin)and sha(safe(upstream,path))==pin,'Changed reviewed source '+path)
    require(recipe['locales']=={'en':'en','zh_Hans_CN':'zh_CN'},'Unsupported introduction locale slice')
    require(type(recipe['resources'])is list and len(recipe['resources'])==11,'Incomplete introduction image coverage')
    ids=set();roles=set();outputs=set()
    for row in recipe['resources']:
        exact(row,('id','role','scene','node','source','size','source_grid','output','output_grid','trim'),'image binding')
        require(type(row['id'])is int and row['id']>0 and row['id']not in ids and type(row['role'])is str and row['role']not in roles,'Duplicate/invalid image identity');ids.add(row['id']);roles.add(row['role'])
        require(row['scene']in recipe['sources']and row['source']in recipe['sources']and row['source']+'.import'in recipe['sources'],'Unreviewed image binding')
        actual,body=node_texture(upstream,row['scene'],row['node']);require(actual==row['source'],'Source scene/image role differs')
        for key in ('size','source_grid','output_grid'):require(type(row[key])is list and len(row[key])==2 and all(type(v)is int and v>0 for v in row[key]),'Bad image size/grid')
        require(type(row['trim'])is list and len(row['trim'])==4 and all(type(v)is int and v>=0 for v in row['trim'])and row['trim'][2]>row['trim'][0]and row['trim'][3]>row['trim'][1],'Bad image trim')
        require(row['output'].startswith('graphics/cutscenes/introduction/')and row['output'].endswith('.t3x')and row['output']not in outputs,'Unsafe/duplicate image deployment');safe(project/'romfs',row['output']);outputs.add(row['output'])
        with Image.open(safe(upstream,row['source']))as im:require(list(im.size)==row['size'],'Source image dimensions changed');image=im.convert('RGBA')
        w,h=row['size'];cols,rows=row['source_grid'];require(w%cols==0 and h%rows==0,'Invalid source grid');fw,fh=w//cols,h//rows
        if cols*rows>1:
            m=re.search(r'^hframes = ([0-9]+)$',body,re.M);require(m and int(m[1])==cols and rows==1,'Unreviewed source cloud grid')
        else:require(row['trim']==[0,0,fw,fh]and row['output_grid']==[1,1],'Unsupported still-image trim')
        l,t,r,b=row['trim'];require(r<=fw and b<=fh and row['output_grid'][0]*row['output_grid'][1]>=cols*rows,'Trim/grid exceeds source')
        require((r-l)*row['output_grid'][0]<=1024 and (b-t)*row['output_grid'][1]<=1024,'Atlas exceeds 3DS texture bounds')
        for i in range(cols*rows):
            frame=image.crop(((i%cols)*fw,(i//cols)*fh,(i%cols+1)*fw,(i//cols+1)*fh));outside=frame.getchannel('A');ImageDraw.Draw(outside).rectangle((l,t,r-1,b-1),fill=0);require(outside.getbbox()is None,'Trim discards visible original pixels')
    require(type(recipe['fonts'])is list and len(recipe['fonts'])==6,'Incomplete original font roles')
    font_sources=set();font_roles=set()
    for font in recipe['fonts']:
        exact(font,('role','locale','source','scene','node','definition','text_keys','extra_characters'),'font binding');require(font['locale']in recipe['locales']and type(font['role'])is str and (font['role'],font['locale'])not in font_roles ,'Duplicate font role');font_roles.add((font['role'],font['locale']));font_sources.add(font['source'])
        require(font['source']in recipe['sources']and font_definition(upstream,font['source'])==font['definition'],'Original font properties/fallback differs')
        for data in font['definition']['fallback_chain']:require(data['path']in recipe['sources'],'Unreviewed original font data')
        require(type(font['text_keys'])is list and font['text_keys']and len(set(font['text_keys']))==len(font['text_keys'])and all(k in recipe['text_keys']for k in font['text_keys'])and type(font['extra_characters'])is str,'Unknown font glyph coverage')
    for font in recipe['fonts']:
        require(font['scene']in recipe['sources'],'Unreviewed font scene')
        original=next(f for f in recipe['fonts']if f['role']==font['role']and f['locale']=='en')
        actual,_=node_texture(upstream,font['scene'],font['node'],'custom_fonts/font');require(actual==original['source'],'Source font/node role differs')
    require(font_roles=={(role,locale)for role in ('old','now','hint')for locale in recipe['locales']},'Missing source font role')
    require(recipe['font_catalog']=='fonts/introduction/intro.encfont','Unknown font catalog identity')
    for p in recipe['translation_sources']:require(p in recipe['sources'],'Unreviewed translations')
    project_text=safe(upstream,'project.godot').read_text(encoding='utf-8')
    for role in ('old','now','hint'):
        a=next(f for f in recipe['fonts']if f['role']==role and f['locale']=='en');b=next(f for f in recipe['fonts']if f['role']==role and f['locale']=='zh_Hans_CN')
        if a['source']!=b['source']:require('"res://'+a['source']+'": PoolStringArray('in project_text and 'res://'+b['source']+':zh_Hans_CN'in project_text,'Original locale font remap changed')
    font_specs(recipe)
    return translations(upstream,recipe)

def font_specs(recipe):
    """A source resource has one checked face, including all locale uses."""
    result=[];seen={}
    for spec in recipe['fonts']:
        if spec['source']in seen:require(spec['definition']==seen[spec['source']]['definition'],'Conflicting source font definitions')
        else:seen[spec['source']]=spec;result.append(spec)
    return result


def art_records(recipe):
    result=[]
    for r in recipe['resources']:
        sc,sr=r['source_grid'];w,h=r['size'];fw,fh=w//sc,h//sr;l,t,right,bottom=r['trim'];c,rows=r['output_grid']
        result.append(dict(id=r['id'],role=r['role'],source=r['source'],path=r['output'],width=(right-l)*c,height=(bottom-t)*rows,columns=c,rows=rows,frame_count=sc*sr,source_width=fw,source_height=fh,trim_x=l,trim_y=t))
    return result

def compile_art(recipe,tex3ds,project=ROOT,recipe_path=RECIPE,build=None):
    project=Path(project);validate(recipe,project,recipe_path);build=Path(build or project/'build/introduction-assets');build.mkdir(parents=True,exist_ok=True);records=art_records(recipe)
    for source,record in zip(recipe['resources'],records):
        image=Image.open(safe(project/'upstream/MOTHER-Encore',source['source'])).convert('RGBA');atlas=Image.new('RGBA',(record['width'],record['height']),(0,0,0,0));fw,fh=record['source_width'],record['source_height'];sc=source['source_grid'][0];l,t,r,b=source['trim'];cw,ch=r-l,b-t
        for i in range(record['frame_count']):atlas.paste(image.crop(((i%sc)*fw+l,(i//sc)*fh+t,(i%sc)*fw+r,(i//sc)*fh+b)),((i%record['columns'])*cw,(i//record['columns'])*ch))
        png=build/(str(record['id'])+'.png');atlas.save(png);target=safe(project/'romfs',record['path']);target.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(png)],check=True,capture_output=True)
        raw=target.read_bytes();record.update(bytes=len(raw),sha256=sha(target),crc32=zlib.crc32(raw))
    receipt=dict(schema=1,kind='encore.introduction-asset-receipt',commit=recipe['commit'],recipe_sha256=sha(recipe_path),generator_sha256=sha(__file__),tex3ds_sha256=sha(tex3ds),sources=recipe['sources'],resources=records,font_catalog=recipe['font_catalog'])
    write_json(project/ART_RECEIPT,receipt);return receipt


class FreeTypeMetrics:
    """Opaque public FreeType API; preserve original NORMAL/MONO hinting."""
    def __init__(self):
        name=ctypes.util.find_library('freetype');require(name,'Existing FreeType shared library required')
        self.api=ctypes.CDLL(name);self.faces={};self.library=ctypes.c_void_p()
        signatures={'FT_Init_FreeType':[ctypes.POINTER(ctypes.c_void_p)],'FT_New_Face':[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_long,ctypes.POINTER(ctypes.c_void_p)],'FT_Set_Pixel_Sizes':[ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint],'FT_Get_Char_Index':[ctypes.c_void_p,ctypes.c_ulong],'FT_Get_Advance':[ctypes.c_void_p,ctypes.c_uint,ctypes.c_int32,ctypes.POINTER(ctypes.c_long)],'FT_Done_Face':[ctypes.c_void_p],'FT_Done_FreeType':[ctypes.c_void_p],'FT_Library_Version':[ctypes.c_void_p,ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int)]}
        for method,args in signatures.items():getattr(self.api,method).argtypes=args
        self.api.FT_Get_Char_Index.restype=ctypes.c_uint
        require(self.api.FT_Init_FreeType(ctypes.byref(self.library))==0,'FreeType initialization failed')
        major=ctypes.c_int();minor=ctypes.c_int();patch=ctypes.c_int();self.api.FT_Library_Version(self.library,ctypes.byref(major),ctypes.byref(minor),ctypes.byref(patch));self.version='%d.%d.%d'%(major.value,minor.value,patch.value)
    def advance(self,path,size,codepoint,data):
        require(data['hinting']==2,'Unreviewed original FreeType hinting')
        key=(str(path),size)
        if key not in self.faces:
            face=ctypes.c_void_p();require(self.api.FT_New_Face(self.library,os.fsencode(path),0,ctypes.byref(face))==0,'Original FreeType face failed');self.faces[key]=face;require(self.api.FT_Set_Pixel_Sizes(face,0,size)==0,'Original FreeType size failed')
        face=self.faces[key];index=self.api.FT_Get_Char_Index(face,codepoint);require(index>0,'Original FreeType scalar missing')
        value=ctypes.c_long();flags=0 if data['antialiased']else 2<<16
        require(self.api.FT_Get_Advance(face,index,flags,ctypes.byref(value))==0,'Original hinted advance failed')
        return value.value/65536.
    def close(self):
        if self.library:
            for face in self.faces.values():self.api.FT_Done_Face(face)
            self.faces.clear();self.api.FT_Done_FreeType(self.library);self.library=ctypes.c_void_p()
    def __del__(self):
        if hasattr(self,'faces'):self.close()


def introduction_metrics_script(base):
    old='  faces.append({"source":spec.source,"size":font.size,"ascent":font.get_ascent(),"descent":font.get_descent(),"height":font.get_height(),"advances":advances})'
    new='  var data=[font.font_data]\n  for i in range(font.get_fallback_count()):\n   data.append(font.get_fallback(i))\n  var settings=[]\n  for d in data:\n   settings.append({"path":d.font_path.trim_prefix("res://"),"antialiased":d.antialiased,"hinting":d.hinting})\n  var pairs=[]\n  var widths=[]\n  for text in spec.texts:\n   widths.append(font.get_string_size(text).x)\n   for i in range(text.length()):\n    var cp=text.ord_at(i)\n    var following=text.ord_at(i+1) if i+1<text.length() else 0\n    pairs.append({"codepoint":cp,"next":following,"advance":font.get_char_size(cp,following).x})\n  faces.append({"source":spec.source,"size":font.size,"ascent":font.get_ascent(),"descent":font.get_descent(),"height":font.get_height(),"advances":advances,"pairs":pairs,"texts":spec.texts,"widths":widths,"char_spacing":font.extra_spacing_char,"space_spacing":font.extra_spacing_space,"data_settings":settings})'
    require(base.count(old)==1,'Native metrics helper changed; review Introduction probe integration')
    return base.replace(old,new)


def check_native_layout(spec,request,metric,glyphs):
    """Every original line and adjacent scalar must fit the bounded renderer."""
    spacing=int(spec['definition']['properties'].get('extra_spacing_char','0'))
    spaces=int(spec['definition']['properties'].get('extra_spacing_space','0'))
    require(metric['data_settings']==spec['definition']['fallback_chain'],'Native DynamicFontData settings changed')
    require(metric['char_spacing']==spacing and metric['space_spacing']==spaces and metric['texts']==request['texts'],'Native source spacing/text changed')
    advances={g['codepoint']:g['advance']for g in glyphs}
    expected_pairs=[(ord(c),ord(text[i+1])if i+1<len(text)else 0)for text in request['texts']for i,c in enumerate(text)]
    require(type(metric['pairs'])is list and len(metric['pairs'])==len(expected_pairs),'Incomplete native pair metrics')
    for pair,(cp,following)in zip(metric['pairs'],expected_pairs):
        exact(pair,('codepoint','next','advance'),'native font pair');require(type(pair['codepoint'])is int and type(pair['next'])is int and (pair['codepoint'],pair['next'])==(cp,following)and type(pair['advance'])in(int,float)and math.isfinite(pair['advance']),'Native pair identity/metric changed')
        expected=advances[pair['codepoint']]+(spacing if pair['next']and pair['codepoint']!=32 else 0)
        require(abs(pair['advance']-expected)<=.001,'Original pair kerning unsupported; explicit native mapping required '+spec['source'])
    require(len(metric['widths'])==len(request['texts']),'Incomplete native string metrics')
    for text,width in zip(request['texts'],metric['widths']):
        require(type(width)in(int,float)and math.isfinite(width),'Invalid native string width')
        expected=sum(advances[ord(c)]+(spacing if i+1<len(text)and c!=' 'else 0)for i,c in enumerate(text))
        require(abs(expected-width)<=.001,'Original line width differs from bounded native renderer '+spec['source'])


def compile_fonts(recipe,godot,tex3ds,project=ROOT,recipe_path=RECIPE,build=None):
    from tools.source_fonts import GODOT
    from fontTools import __version__ as fonttools_version
    from fontTools.ttLib import TTFont
    project=Path(project);texts=validate(recipe,project,recipe_path);upstream=project/'upstream/MOTHER-Encore';build=Path(build or project/'build/introduction-fonts');probe=build/'godot-project';(probe/'Fonts').mkdir(parents=True,exist_ok=True)
    requests=[];required={};cmaps={};credits={}
    for f in font_specs(recipe):
        cps={ord(c)for same in recipe['fonts']if same['source']==f['source']for key in same['text_keys']for c in texts[key][same['locale']]if ord(c)>=32}|{ord(c)for same in recipe['fonts']if same['source']==f['source']for c in same['extra_characters']};required[f['source']]=cps;requests.append(dict(source=f['source'],codepoints=sorted(cps),texts=sorted({line for same in recipe['fonts']if same['source']==f['source']for key in same['text_keys']for line in texts[key][same['locale']].split('\n')})))
        shutil.copyfile(safe(upstream,f['source']),probe/f['source'])
        for d in f['definition']['fallback_chain']:
            path=d['path'];shutil.copyfile(safe(upstream,path),probe/path)
            if path not in cmaps:
                ft=TTFont(safe(upstream,path));cmaps[path]=ft.getBestCmap();credits[path]={str(i):sorted({n.toUnicode()for n in ft['name'].names if n.nameID==i})for i in (0,8,9,13,14)}
    write_json(probe/'request.json',requests);(probe/'project.godot').write_text('config_version=4\n',encoding='utf-8');(probe/'metrics.gd').write_text(introduction_metrics_script(GODOT),encoding='utf-8')
    run=subprocess.run([str(godot),'--path',str(probe),'-s','metrics.gd'],check=True,capture_output=True,text=True,timeout=90,env=dict(os.environ,XDG_DATA_HOME=str(build/'userdata')));(build/'godot.log').write_text(run.stdout+run.stderr,encoding='utf-8');metrics=read(probe/'metrics.json');require(len(metrics['faces'])==len(requests),'Incomplete native font metrics')
    pages=[];faces=[];glyphs=[];cache={};hinted=FreeTypeMetrics();metrics['freetype_metrics_version']=hinted.version;out=project/'romfs/fonts/introduction';out.mkdir(parents=True,exist_ok=True)
    for index,(spec,request,metric)in enumerate(zip(font_specs(recipe),requests,metrics['faces'])):
        require(metric['source']==spec['source']and len(metric['advances'])==len(request['codepoints']),'Native font identity/metrics mismatch');props=spec['definition']['properties'];spacing=int(props.get('extra_spacing_char','0'));spaces=int(props.get('extra_spacing_space','0'));stroke=int(props.get('outline_size','0'));require(0<=stroke<=4,'Unsupported outline width')
        first_glyph,first_page=len(glyphs),len(pages);local=[];image=None;x=y=rowh=0;page=-1
        def flush():
            if image is None:return
            name='face-%d-%03d.t3x'%(index,page);png=build/(name+'.png');target=out/name;image.save(png);subprocess.run([str(tex3ds),'-f','la8','-z','none','-o',str(target),str(png)],check=True,capture_output=True);raw=target.read_bytes();pages.append(dict(path=name,width=PAGE,height=PAGE,texture_bytes=PAGE*PAGE*2,file_bytes=len(raw),crc32=zlib.crc32(raw),sha256=sha(target)))
        for cp,native in zip(request['codepoints'],metric['advances']):
            data=next((d for d in spec['definition']['fallback_chain']if cp in cmaps[d['path']]),None);require(data is not None,'Original fallback has no required glyph U+%04X'%cp)
            key=(data['path'],metric['size']);font=cache.get(key)
            if font is None:font=cache[key]=ImageFont.truetype(str(safe(upstream,data['path'])),metric['size'])
            advance=hinted.advance(safe(upstream,data['path']),metric['size'],cp,data)+(spacing+spaces if cp==32 else 0);require(abs(advance-native)<=.001,'Original advance mismatch %s U+%04X FreeTypeHinted=%s Godot=%s'%(spec['source'],cp,advance,native))
            box=font.getbbox(chr(cp),anchor='ls',stroke_width=stroke);w,h=box[2]-box[0],box[3]-box[1];require(0<=w<PAGE-2 and 0<=h<PAGE-2,'Unbounded source glyph')
            if image is None:image=Image.new('RGBA',(PAGE,PAGE),(255,255,255,0));page+=1;x=y=rowh=1
            if x+w+1>PAGE:x=1;y+=rowh+1;rowh=1
            if y+h+1>PAGE:flush();image=Image.new('RGBA',(PAGE,PAGE),(255,255,255,0));page+=1;x=y=rowh=1
            if w and h:
                bitmap=Image.new('RGBA',(w,h),(255,255,255,0));ImageDraw.Draw(bitmap).text((-box[0],-box[1]),chr(cp),font=font,anchor='ls',fill=(255,255,255,255),stroke_width=stroke,stroke_fill=(0,0,0,255))
                if not data['antialiased']:bitmap.putalpha(bitmap.getchannel('A').point(lambda v:255 if v>=128 else 0))
                image.paste(bitmap,(x,y))
            local.append(dict(codepoint=cp,page=first_page+page,u=x,v=y,width=w,height=h,advance=native,offset_x=box[0],offset_y=metric['ascent']+box[1],source_face=data['path']));x+=w+2;rowh=max(rowh,h)
        check_native_layout(spec,request,metric,local);flush();glyphs.extend(local);face=dict(source=spec['source'],legacy_ascii=False,first_glyph=first_glyph,glyph_count=len(local),first_page=first_page,page_count=len(pages)-first_page,ascent=metric['ascent'],descent=metric['descent'],height=metric['height'],definition=spec['definition']);require(face['page_count']*PAGE*PAGE*2<=2*1024*1024,'Font resident budget exceeded');faces.append(face)
    hinted.close();blob=encode_font(faces,pages,glyphs);target=safe(project/'romfs',recipe['font_catalog']);target.write_bytes(blob)
    receipt=dict(schema=1,kind='encore.introduction-font-receipt',commit=recipe['commit'],recipe_sha256=sha(recipe_path),generator_sha256=sha(__file__),metrics_generator_sha256=sha(ROOT/'tools/source_fonts.py'),godot_sha256=sha(godot),tex3ds_sha256=sha(tex3ds),pillow_version=pillow_version,freetype_version=features.version_module('freetype2'),fonttools_version=fonttools_version,sources=recipe['sources'],metrics=metrics,faces=faces,pages=pages,glyphs=glyphs,font_embedded_notices=credits,binary=dict(path=recipe['font_catalog'],bytes=len(blob),sha256=sha(target)),limits='Original DynamicFont fallback and spacing; every used scalar matched to official Godot 3.6.2 hinted advance/ascent/height; opaque FreeType FT_Get_Advance NORMAL/MONO source settings, spacing and all original text pairs/line widths checked. Pillow/FreeType glyph rasters, no GPU/hardware pixel equivalence claim; original Nintendo-source rights remain unconfirmed. No system or replacement glyphs.')
    write_json(project/FONT_RECEIPT,receipt);return receipt


def encode_font(faces,pages,glyphs):
    require(type(faces)is list and type(pages)is list and type(glyphs)is list and 1<=len(faces)<=16 and 1<=len(pages)<=128 and 1<=len(glyphs)<=65536,'Font record count out of bounds')
    payload=bytearray();next_glyph=next_page=0
    def string(value):
        require(type(value)is str and value.isascii()and value,'Invalid font source/path');raw=value.encode('ascii');payload.extend(struct.pack('<I',len(raw)));payload.extend(raw)
    for f in faces:
        exact(f,('source','legacy_ascii','first_glyph','glyph_count','first_page','page_count','ascent','descent','height','definition'),'font face')
        require(type(f['first_glyph'])is int and type(f['first_page'])is int,'Invalid face range identity')
        require(type(f['legacy_ascii'])is bool and not f['legacy_ascii']and f['first_glyph']==next_glyph and f['first_page']==next_page and type(f['glyph_count'])is int and f['glyph_count']>0 and type(f['page_count'])is int and f['page_count']>0,'Noncontiguous font face ranges')
        require(all(type(f[k])in(int,float)and math.isfinite(f[k])and 0<=f[k]<=128 for k in ('ascent','descent','height'))and f['height']>0,'Invalid native font metric')
        next_glyph+=f['glyph_count'];next_page+=f['page_count'];require(next_glyph<=len(glyphs)and next_page<=len(pages),'Font face range exceeds table')
        string(f['source']);payload.extend(struct.pack('<5I3f',int(f['legacy_ascii']),f['first_glyph'],f['glyph_count'],f['first_page'],f['page_count'],f['ascent'],f['descent'],f['height']))
    require(next_glyph==len(glyphs)and next_page==len(pages),'Unbound font records')
    for p in pages:
        exact(p,('path','width','height','texture_bytes','file_bytes','crc32','sha256'),'font page')
        require(p['width']==PAGE and p['height']==PAGE and p['texture_bytes']==PAGE*PAGE*2 and type(p['file_bytes'])is int and p['texture_bytes']<=p['file_bytes']<=p['texture_bytes']+4096 and type(p['crc32'])is int and 0<=p['crc32']<=0xffffffff,'Invalid font page structure')
        string(p['path']);payload.extend(struct.pack('<5I',p['width'],p['height'],p['texture_bytes'],p['file_bytes'],p['crc32']))
    for f in faces:
        previous=0
        for g in glyphs[f['first_glyph']:f['first_glyph']+f['glyph_count']]:
            exact(g,('codepoint','page','u','v','width','height','advance','offset_x','offset_y','source_face'),'font glyph')
            require(type(g['source_face'])is str and g['source_face'],'Missing glyph source identity')
            require(all(type(g[k])is int and g[k]>=0 for k in ('codepoint','page','u','v','width','height'))and 32<=g['codepoint']<=0x10ffff and not 0xd800<=g['codepoint']<=0xdfff and g['codepoint']>previous,'Invalid glyph scalar/order')
            previous=g['codepoint'];require(f['first_page']<=g['page']<f['first_page']+f['page_count']and g['u']+g['width']<=PAGE and g['v']+g['height']<=PAGE,'Glyph page/atlas bounds mismatch')
            require(all(type(g[k])in(int,float)and math.isfinite(g[k])for k in ('advance','offset_x','offset_y'))and 0<=g['advance']<=128 and abs(g['offset_x'])<=128 and abs(g['offset_y'])<=128,'Invalid glyph metric')
            payload.extend(struct.pack('<6I3f',*(g[k]for k in ('codepoint','page','u','v','width','height','advance','offset_x','offset_y'))))
    return HEADER.pack(b'ENCFONT\0',1,len(payload),zlib.crc32(payload),len(faces),len(pages),len(glyphs))+payload


def stage_files(root,project=ROOT,recipe_path=RECIPE):
    root,project=Path(root),Path(project);recipe=read(recipe_path);validate(recipe,project,recipe_path);art=read(project/ART_RECEIPT);font=read(project/FONT_RECEIPT)
    exact(art,('schema','kind','commit','recipe_sha256','generator_sha256','tex3ds_sha256','sources','resources','font_catalog'),'art receipt')
    exact(font,('schema','kind','commit','recipe_sha256','generator_sha256','metrics_generator_sha256','godot_sha256','tex3ds_sha256','pillow_version','freetype_version','fonttools_version','sources','metrics','faces','pages','glyphs','font_embedded_notices','binary','limits'),'font receipt')
    for receipt,kind in ((art,'encore.introduction-asset-receipt'),(font,'encore.introduction-font-receipt')):
        require(type(receipt['schema'])is int and receipt['schema']==1 and receipt['kind']==kind and receipt['commit']==recipe['commit']and receipt['recipe_sha256']==sha(recipe_path)and receipt['generator_sha256']==sha(__file__)and receipt['sources']==recipe['sources'],'Stale/unknown introduction receipt')
    require(font['metrics_generator_sha256']==sha(ROOT/'tools/source_fonts.py'),'Font metrics generator changed');require(art['font_catalog']==recipe['font_catalog'],'Font catalog binding changed');files={}
    def checked(path,size,pin,crc=None):
        require(type(size)is int and size>0 and type(pin)is str and re.fullmatch('[0-9a-f]{64}',pin),'Malformed output fingerprint');p=safe(root,path);require(Path(path)not in files,'Duplicate staged output');raw=p.read_bytes();require(len(raw)==size and hashlib.sha256(raw).hexdigest()==pin,'Output size/hash mismatch '+path);require(crc is None or type(crc)is int and zlib.crc32(raw)==crc,'Output CRC mismatch '+path);files[Path(path)]=raw;return raw
    require(type(art['resources'])is list and len(art['resources'])==len(recipe['resources']),'Image receipt coverage mismatch')
    for expected,actual in zip(art_records(recipe),art['resources']):
        exact(actual,tuple(expected)+('bytes','sha256','crc32'),'compiled image');require(all(type(actual[k])is type(v)and actual[k]==v for k,v in expected.items()),'Image identity/geometry changed');checked(actual['path'],actual['bytes'],actual['sha256'],actual['crc32'])
    exact(font['binary'],('path','bytes','sha256'),'font binary');require(font['binary']['path']==recipe['font_catalog'],'Font binary identity changed');binary=checked(font['binary']['path'],font['binary']['bytes'],font['binary']['sha256']);require(len(binary)>=HEADER.size,'Truncated font header');magic,version,length,crc,nf,np,ng=HEADER.unpack_from(binary);require(magic==b'ENCFONT\0'and version==1 and length==len(binary)-HEADER.size and crc==zlib.crc32(binary[HEADER.size:])and (nf,np,ng)==(len(font['faces']),len(font['pages']),len(font['glyphs'])),'Font header/version/length/CRC/count changed')
    require(encode_font(font['faces'],font['pages'],font['glyphs'])==binary,'Font binary does not match checked receipt metadata')
    texts=translations(project/'upstream/MOTHER-Encore',recipe)
    for spec,face in zip(font_specs(recipe),font['faces']):
        expected={ord(c)for same in recipe['fonts']if same['source']==spec['source']for key in same['text_keys']for c in texts[key][same['locale']]if ord(c)>=32}|{ord(c)for same in recipe['fonts']if same['source']==spec['source']for c in same['extra_characters']}
        rows=font['glyphs'][face['first_glyph']:face['first_glyph']+face['glyph_count']]
        require({g['codepoint']for g in rows}==expected,'Font required glyph coverage changed')
        require(all(g['source_face']in {d['path']for d in spec['definition']['fallback_chain']}for g in rows),'Glyph fallback source changed')
        metrics=font['metrics'];require(type(metrics)is dict and set(metrics)=={'version','faces','freetype_metrics_version'}and len(metrics['faces'])==len(font['faces']),'Incomplete official font metrics')
        version=metrics['version'];require(version['major']==3 and version['minor']==6 and version['patch']==2 and version['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8','Unknown official font probe version')
        metric=metrics['faces'][font['faces'].index(face)]
        exact(metric,('source','size','ascent','descent','height','advances','pairs','texts','widths','char_spacing','space_spacing','data_settings'),'native font metric')
        require(metric['source']==spec['source']and metric['size']==int(spec['definition']['properties'].get('size','16'))and all(metric[k]==face[k]for k in ('ascent','descent','height'))and metric['advances']==[g['advance']for g in rows],'Font native metric binding changed')
        expected_lines=sorted({line for same in recipe['fonts']if same['source']==spec['source']for key in same['text_keys']for line in texts[key][same['locale']].split('\n')})
        check_native_layout(spec,dict(texts=expected_lines),metric,rows)
    require([f['source']for f in font['faces']]==[f['source']for f in font_specs(recipe)]and all(not f['legacy_ascii']for f in font['faces']),'Font source identity changed')
    for spec,face in zip(font_specs(recipe),font['faces']):require(face['definition']==spec['definition'],'Font definition changed')
    for p in font['pages']:
        exact(p,('path','width','height','texture_bytes','file_bytes','crc32','sha256'),'font page');require(Path(p['path']).name==p['path']and p['width']==PAGE and p['height']==PAGE and p['texture_bytes']==PAGE*PAGE*2,'Font page layout changed');checked('fonts/introduction/'+p['path'],p['file_bytes'],p['sha256'],p['crc32'])
    return files


def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('assets','fonts','compile','verify'));p.add_argument('--tex3ds',type=Path);p.add_argument('--godot',type=Path);a=p.parse_args();recipe=read(RECIPE)
    if a.action in ('assets','compile'):require(a.tex3ds and a.tex3ds.is_file(),'Need real tex3ds');compile_art(recipe,a.tex3ds)
    if a.action in ('fonts','compile'):require(a.tex3ds and a.tex3ds.is_file()and a.godot and a.godot.is_file(),'Need official Godot3.6.2 and real tex3ds');compile_fonts(recipe,a.godot,a.tex3ds)
    if a.action=='verify':print('Checked introduction assets:',len(stage_files(ROOT/'romfs')))
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError)as e:print('INTRODUCTION ASSETS ERROR:',e,file=sys.stderr);raise SystemExit(1)
