"""Checked, offline UI binding recipes. No source-script execution or fallbacks."""
from __future__ import annotations
import hashlib,json,math,re
from pathlib import Path
from tools.extract_battle_entry import Extractor,node,one,properties,require,variant
ROOT=Path(__file__).resolve().parents[1]

def unique(pairs):
 result={}
 for key,value in pairs:
  require(key not in result,'Duplicate UI binding key: '+key);result[key]=value
 return result

def safe(path):
 require(isinstance(path,str) and path and '\\' not in path and ':' not in path,'Invalid UI binding path')
 p=Path(path);require(not p.is_absolute() and all(v not in ('','..','.') for v in path.split('/')),'Unsafe UI binding path');return path

def scene_block(text,kind,ident):
 return properties(one(r'^\[sub_resource type="'+re.escape(kind)+r'" id='+str(ident)+r'\]\n(.*?)(?=^\[|\Z)',text,'UI resource',re.M|re.S)[1])

def value(ex,selector):
 require(isinstance(selector,list) and selector,'Invalid UI source selector');op=selector[0]
 if op=='node':
  require(len(selector)==3,'UI node selector shape');return node(ex.text(safe(selector[1])),selector[2])
 if op=='node_path':
  require(len(selector)==3,'UI node path selector shape');node(ex.text(safe(selector[1])),selector[2]);return selector[2]
 if op=='node_name':
  require(len(selector)==3,'UI node name selector shape');node(ex.text(safe(selector[1])),selector[2]);return selector[2].split('/')[-1]
 if op=='source_path':
  require(len(selector)==2,'UI source path selector shape');ex.data(safe(selector[1]));return selector[1]
 if op=='resource':
  require(len(selector)==4 and type(selector[3]) is int,'UI resource selector shape');return scene_block(ex.text(safe(selector[1])),selector[2],selector[3])
 if op=='regex':
  require(len(selector)==5 and selector[4] in ('number','json','string','vector','string_list'),'UI regex selector shape')
  raw=one(selector[2],ex.text(safe(selector[1])),'UI binding',re.M|re.S)[selector[3]]
  if selector[4]=='string':return raw
  if selector[4]=='number':return float(raw)
  if selector[4]=='vector':return [float(v.strip()) for v in raw.split(',')]
  if selector[4]=='string_list':
   require(re.fullmatch(r"\[\s*(['\"][A-Za-z_0-9]+['\"]\s*,?\s*)*\]",raw),'Unknown UI source string list');return re.findall(r"['\"]([A-Za-z_0-9]+)['\"]",raw)
  return json.loads(raw,object_pairs_hook=unique)
 if op=='field':
  require(len(selector)==3,'UI field selector shape');return value(ex,selector[1])[selector[2]]
 if op=='index':
  require(len(selector)==3 and type(selector[2]) is int,'UI index selector shape');return value(ex,selector[1])[selector[2]]
 if op=='texture':
  require(len(selector)==4,'UI texture selector shape');text=ex.text(safe(selector[1]));ref=node(text,selector[2])[selector[3]]
  require(isinstance(ref,dict) and set(ref)=={'ExtResource'},'UI texture must be an external source resource')
  return one(r'^\[ext_resource path="res://([^"\n]+)" type="Texture" id='+str(ref['ExtResource'])+r'\]$',text,'UI texture')[1]
 if op=='font':
  require(len(selector)==3,'UI font selector shape');text=ex.text(safe(selector[1]));props=properties(text.split('[resource]\n',1)[1]);return props[selector[2]]
 if op=='atlas_grid':
  require(len(selector)==4 and type(selector[3]) is int,'UI atlas selector shape');text=ex.text(safe(selector[2]));body=one(r'^\[sub_resource type="AtlasTexture" id='+str(selector[3])+r'\]\n(.*?)(?=^\[|\Z)',text,'UI atlas',re.M|re.S)[1]
  region=[float(v.strip()) for v in one(r'^region = Rect2\(\s*([^)]+)\)',body,'UI atlas region')[1].split(',')];size=ex.png_size(safe(selector[1]));require(len(region)==4 and all(v>0 for v in region[2:]),'Invalid UI atlas geometry');grid=[size[i]/region[i+2] for i in range(2)];require(all(v==int(v) for v in grid),'UI atlas cells do not divide source texture');return [int(v) for v in grid]
 if op=='palette':
  require(len(selector)==4,'UI palette selector shape');p=scene_block(ex.text(safe(selector[1])),'ShaderMaterial',selector[2]);return [''.join(f'{round(v*255):02x}' for v in p[key][:3]) for key in selector[3]]
 if op=='members':
  require(len(selector)==4,'UI member-set selector shape');raw=one(selector[3],ex.text(safe(selector[1])),'UI party members')[1];symbols=re.findall(r'PartyMember\.([A-Z_0-9]+)',raw)
  require(symbols and re.fullmatch(r'\[\s*(PartyMember\.[A-Z_0-9]+\s*,?\s*)+\]',raw),'Unknown source member set')
  source=ex.text(safe(selector[2]));return sorted(one(r'^const '+symbol+r'\s*:?=\s*"([^"\n]+)"',source,'UI member identity')[1] for symbol in symbols)
 if op=='regex_equal':
  require(len(selector)==3,'UI repeated numeric selector shape');rows=re.findall(selector[2],ex.text(safe(selector[1])),re.M);require(rows and len(set(rows))==1,'Ambiguous UI repeated tuning');return float(rows[0])
 if op in ('prompt_masks','prompt_category'):
  require(len(selector)==6,'UI prompt type selector shape');script=ex.text(safe(selector[1]));types=json.loads('['+one(selector[2],script,'UI prompt types')[1]+']');require(len(types)==2 and len(set(types))==2,'Unknown UI prompt categories')
  if op=='prompt_masks':
   choices=value(ex,selector[3]);all_=one(selector[4],script,'UI combined prompt choice')[1];require(all_ in choices and all(t in choices for t in types),'Unknown UI prompt choices');return [3 if c==all_ else 1 if c==types[0] else 2 if c==types[1] else 0 for c in choices]
  default=one(selector[4],script,'UI default prompt type')[1];p=node(ex.text(safe(selector[3])),selector[5]);type_=p['type'] if 'type' in p else default;require(type_ in types,'Unknown UI prompt type');return types.index(type_)+1
 raise ValueError('Unknown UI source selector: '+str(op))

def shape(value_,spec):
 if isinstance(spec,dict):
  require(isinstance(value_,dict) and set(value_)==set(spec),'Unknown/missing UI binding fields')
  for k,s in spec.items():shape(value_[k],s)
 elif isinstance(spec,list):
  require(isinstance(value_,list) and len(value_)==len(spec),'UI binding list scope/order changed')
  for v,s in zip(value_,spec):shape(v,s)
 else:
  require(type(value_) is spec,'UI binding type mismatch')
  if spec in (float,int):require(math.isfinite(value_),'Nonfinite UI binding')

def load(name,spec,project=ROOT,recipe=None,expected_checks=None,expected_contracts=None):
 project=Path(project);file=Path(recipe) if recipe else project/'content'/name
 d=json.loads(file.read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=lambda v:(_ for _ in ()).throw(ValueError('Nonfinite UI binding')))
 require(set(d)=={'schema','commit','sources','bindings','checks','contracts'},'Unknown UI binding schema keys');require(type(d['schema'])is int and d['schema']==1,'UI binding schema version')
 ex=Extractor(project);require(d['commit']==ex.lock['commit'],'UI binding source pin mismatch');shape(d['bindings'],spec)
 b=d['bindings']
 for key in ('resource_root','font_path','outline_path'):
  if key in b:safe(b[key].rstrip('/') if key.endswith('_root') else b[key])
 for key in ('viewport','viewports','atlas'):
  if key in b:require(all(type(v) is int and 1<=v<=1024 for v in b[key]),'UI dimension out of bounds')
 if 'font_recipe' in b:
  f=b['font_recipe'];require(1<=f['size']<=128 and 0<=f['first']<=f['last']<=65535 and f['last']-f['first']<=255 and all(1<=v<=128 for v in f['cell']) and 1<=f['columns']<=256 and 0<=f['outline']<=8,'UI font atlas bounds')
  require(b['atlas'][0]==f['columns']*f['cell'][0] and b['atlas'][1]>=((f['last']-f['first']+f['columns'])//f['columns'])*f['cell'][1],'UI font atlas coverage')
 for key in ('background_color','color'):
  if key in b:require(0<=b[key]<=0xffffffff,'UI color out of bounds')
 require(isinstance(d['sources'],dict) and d['sources'],'Missing UI source coverage')
 for path,digest in d['sources'].items():require(hashlib.sha256(ex.data(safe(path))).hexdigest()==digest,'UI source fingerprint mismatch')
 for key,path in d['bindings'].items():
  if key.startswith('source_'):require(safe(path) in d['sources'],'Unreviewed UI source alias')
 require(isinstance(d['checks'],list) and d['checks'] and len(d['checks'])<=256,'Invalid UI semantic checks')
 require(expected_checks is None or len(d['checks'])==expected_checks,'UI check coverage changed')
 seen=set()
 for row in d['checks']:
  require(set(row)=={'binding','selector'},'Unknown UI check fields');key=tuple(row['binding']);identity=(key,json.dumps(row['selector'],sort_keys=True));require(key and identity not in seen,'Duplicate UI binding check');seen.add(identity)
  current=d['bindings']
  for part in key:
   require(isinstance(part,(str,int)) and not isinstance(part,bool),'UI binding reference type');current=current[part]
  require((sorted(current) if row['selector'][0]=='members' else current)==value(ex,row['selector']),'UI binding/source semantic mismatch: '+str(key))
 require(isinstance(d['contracts'],list) and d['contracts'] and len(d['contracts'])<=256,'Missing UI semantic contracts')
 require(expected_contracts is None or len(d['contracts'])==expected_contracts,'UI contract coverage changed')
 seen=set()
 for row in d['contracts']:
  require(set(row)=={'selector','expected'},'Unknown UI contract fields');key=json.dumps(row['selector'],sort_keys=True);require(key not in seen,'Duplicate UI source contract');seen.add(key)
  require(value(ex,row['selector'])==row['expected'],'Changed reviewed UI source semantics')
 require(set(ex.sources)<=set(d['sources']),'Incomplete UI source coverage')
 return d['bindings']

def spec(value_):
 """Only used by offline recipe-authoring/tests; consumers declare a fixed schema."""
 if isinstance(value_,dict):return {k:spec(v) for k,v in value_.items()}
 if isinstance(value_,list):return [spec(v) for v in value_]
 return type(value_)
