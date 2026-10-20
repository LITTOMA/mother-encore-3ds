"""Offline source scene-ready replay, retaining samples previously discarded.

This is the four already-admitted source scripts' fixed ready subset. It is
not a script VM, a runtime RNG stream or a new viewport-event policy.
"""
import math,re,struct
from tools.extract_battle_entry import require

def operations(ex,order,expected_scripts):
 from tools.podunk_field import godot_string_hash
 nodes={n.path:n for n in order};require(len(nodes)==len(order)and'.'in nodes,'Ready RNG unique scene root')
 children={n.path:[]for n in order}
 for node in order:
  if node.path=='.':continue
  parent=node.path.rsplit('/',1)[0]if'/'in node.path else'.';require(parent in nodes,'Ready RNG missing parent');children[parent].append(node.path)
 rand_pat=re.compile(r'\b(?:randi|randf|rand_range|randomize|seed)\s*\(');func_pat=re.compile(r'^func ([A-Za-z0-9_]+)\(.*?(?=^func |\Z)',re.S|re.M);draws={}
 for node in order:
  script=node.script or''
  if script in draws or not script.endswith('.gd'):continue
  funcs={m.group(1):m.group(0)for m in func_pat.finditer(ex.text(script))};ready=funcs.get('_ready','');called=re.findall(r'\b([A-Za-z_][A-Za-z0-9_]*)\(',ready)
  draws[script]=bool(rand_pat.search(ready))or any(rand_pat.search(funcs.get(name,''))for name in called)
 require({s for s,hit in draws.items()if hit}==set(expected_scripts),'Unreviewed field _ready RNG')
 out=[];seen=set()
 def visit(path,depth=0):
  require(depth<=128 and path not in seen,'Ready RNG scene cycle/depth/duplicate');seen.add(path)
  for child in children[path]:visit(child,depth+1)
  node=nodes[path];script=node.script
  def add(kind,**extra):out.append(dict(kind=kind,path=path,**extra))
  if script=='Scripts/misc/grass spawner.gd':
   add('Seed',seed=godot_string_hash(node.name));add('Randi');add('Randi')
  elif script=='Scripts/misc/birds.gd':add('Randi');add('Randf');add('Randi');add('Randi')
  elif script=='Scripts/misc/butterfly.gd':add('Randi')
  elif script=='Scripts/misc/sparkles.gd':
   require(re.search(r'func _ready\(\):\s+frame\s*=\s*int\(rand_range\(0, 47\)\)\s*\Z',ex.text(script))is not None,'Sparkles ready assignment changed');add('RandRange',low=0,high=47)
  elif draws.get(script,False):raise ValueError('Unmapped ready RNG source script')
 visit('.');require(len(seen)==len(nodes),'Ready RNG disconnected source nodes');return out

def evaluate(ops):
 from tools.podunk_field import SourceRandom
 rng=SourceRandom(0);seeded=False;rows=[];samples={};raw_draws=0
 f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
 for index,op in enumerate(ops):
  require(type(op)is dict and set(op)==({'kind','path','seed'}if op.get('kind')=='Seed'else{'kind','path','low','high'}if op.get('kind')=='RandRange'else{'kind','path'}),'Unknown ready RNG operation fields')
  require(type(op['path'])is str and 0<len(op['path'])<=512 and '\x00'not in op['path'],'Ready RNG owner path rejected')
  kind=op['kind'];value=0
  if kind=='Seed':
   require(type(op['seed'])is int and 0<=op['seed']<=0xffffffff,'Ready RNG source hash range');rng.seed(op['seed']);seeded=True
  elif kind=='Randi':value=rng.randi();raw_draws+=1
  elif kind=='Randf':value=f32(f32(rng.randi())/f32(0xffffffff));raw_draws+=1
  elif kind=='RandRange':
   require(type(op['low'])in(int,float)and type(op['high'])in(int,float)and math.isfinite(op['low'])and math.isfinite(op['high'])and 0<=op['low']<op['high']<=65536,'Ready RNG source interval')
   exponent=rng.randi();raw_draws+=1;fraction=0.
   if exponent:
    high=rng.randi();low=rng.randi();raw_draws+=2;significand=(high<<32)|low|0x8000000000000001;fraction=math.ldexp(float(significand),-64-(32-exponent.bit_length()))
   scaled=fraction*(op['high']-op['low']);value=scaled+op['low']
   require(op['path']not in samples,'Ready RNG duplicate sampled owner');samples[op['path']]=dict(value=value,seeded=seeded,operation=index,state=rng.state)
  else:raise ValueError('Unsupported ready RNG operation')
  rows.append(dict(operation=index,kind=kind,value=value,state=rng.state,seeded=seeded,raw_draws=raw_draws))
 return dict(state=rng.state,rows=rows,samples=samples,raw_draws=raw_draws)

def replay(ex,order,expected_scripts):
 ops=operations(ex,order,expected_scripts);return ops,evaluate(ops)
