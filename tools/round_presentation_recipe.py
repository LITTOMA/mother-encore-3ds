"""Checked declarative presentation data; no GDScript execution or eval.

References select reviewed numeric source facts. The only derived operations
are bounded arithmetic and coordinate conversion, not an executable language.
Native presentation policies are identified explicitly and remain external data.
"""
import hashlib,json,math,re
from pathlib import Path
from tools.extract_battle_entry import node,one,require,animation
from tools.native_round import PARAMETERS,SLOTS
ROOT=Path(__file__).resolve().parents[1]
RECIPE=ROOT/'content/round-presentation-recipe.json'
LIMIT=1024*1024

def fields(value,names,label):require(type(value)is dict and set(value)==set(names),'Unknown/missing '+label+' fields')
def number(value):require(type(value)in(int,float)and math.isfinite(value)and abs(value)<=1000000,'Invalid recipe number');return value
def integer(value,low,high):require(type(value)is int and low<=value<=high,'Invalid recipe schema integer');return value
def duplicate_free(pairs):
 result={}
 for key,value in pairs:require(key not in result,'Duplicate recipe key');result[key]=value
 return result

def read(path=RECIPE):
 raw=Path(path).read_bytes();require(0<len(raw)<=LIMIT,'Recipe size rejected')
 return json.loads(raw.decode('utf-8'),object_pairs_hook=duplicate_free)

def checked(recipe,ex,resources):
 fields(recipe,['schema','kind','commit','scope','sources','facts','actions','parameters','context','animation_reviews','animation_bindings','skill_media'],'recipe')
 require(type(recipe['schema'])is int and recipe['schema']==1 and recipe['kind']=='encore.round-presentation.reviewed-recipe'and recipe['commit']==ex.lock['commit']and type(recipe['scope'])is str and recipe['scope'],'Recipe schema/pin rejected')
 sources=recipe['sources'];require(type(sources)is dict and 0<len(sources)<=128,'Recipe source inventory rejected')
 for path,digest in sources.items():
  require(type(path)is str and path and not path.startswith(('/',"\\"))and ':'not in path and '..'not in Path(path).parts and type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest),'Recipe source identity rejected')
  require(hashlib.sha256(ex.data(path)).hexdigest()==digest,'Recipe source mismatch: '+path)
 require(type(resources)is list and 0<len(resources)<=256,'Recipe resources rejected');by_name={}
 for i,resource in enumerate(resources):
  require(resource['name']not in by_name,'Duplicate recipe resource name');by_name[resource['name']]=i
 def resource_index(name):
  if name is None:return 0xffffffff
  require(type(name)is str and name in by_name,'Unknown recipe resource');return by_name[name]
 values={}
 def resolve(value):
  if type(value)is list:return [resolve(x)for x in value]
  if type(value)is dict:
   fields(value,['fact','index'],'fact reference');require(type(value['fact'])is str and value['fact']in values,'Unknown/forward recipe fact');index=integer(value['index'],0,len(values[value['fact']])-1);return values[value['fact']][index]
  return number(value)
 def vector(value,size):
  value=resolve(value);require(type(value)is list and len(value)==size and all(type(x)in(int,float)for x in value),'Recipe vector shape rejected');return value
 def source_animation(source,name,nodepath):
  require(source in sources and type(name)is str and name and type(nodepath)is str and nodepath,'Unreviewed animation selector');text=ex.text(source);props=node(text,nodepath);require('anims/'+name in props,'Missing reviewed source animation');return animation(text,props['anims/'+name]['SubResource'],source,name)
 facts=recipe['facts'];require(type(facts)is list and 0<len(facts)<=256,'Recipe fact count rejected')
 for fact in facts:
  require(type(fact)is dict and type(fact.get('id'))is str and fact['id']and fact['id']not in values,'Recipe fact identity rejected');kind=fact.get('kind')
  if kind=='numbers':
   fields(fact,['id','kind','source','function','pattern','expected'],'numeric source fact');require(fact['source']in sources and type(fact['pattern'])is str and 0<len(fact['pattern'])<=4096,'Numeric source selector rejected');text=ex.text(fact['source'])
   if fact['function']is not None:
    require(type(fact['function'])is str and re.fullmatch('[A-Za-z_][A-Za-z0-9_]*',fact['function']),'Source function selector rejected')
    text=one(r'^func '+fact['function']+r'\([^\n]*\)[^\n]*\n(.*?)(?=^func |\Z)',text,fact['id'],re.M|re.S)[1]
   match=one(fact['pattern'],text,fact['id'],re.M);actual=[float(x)for x in match.groups()]
  elif kind=='node':
   fields(fact,['id','kind','source','node','properties','expected'],'node source fact');require(fact['source']in sources and type(fact['node'])is str and type(fact['properties'])is list,'Node selector rejected');props=node(ex.text(fact['source']),fact['node']);actual=[]
   for selector in fact['properties']:
    require(type(selector)is dict and set(selector)in[{'property','index'},{'property','index','default'}],'Unknown node property selector');require(type(selector['property'])is str,'Invalid node property selector');require('default'not in selector or(selector['default']==0 and type(selector['default'])is int and selector['property']in ['margin_left','margin_top','margin_right','margin_bottom']),'Unknown Control schema default');require(selector['property']in props or'default'in selector,'Missing node source property');value=props.get(selector['property'],selector.get('default'))
    if selector['index']is not None:require(type(value)is list,'Indexed node source property rejected');value=value[integer(selector['index'],0,len(value)-1)]
    actual.append(value)
  elif kind=='animation_numbers':
   fields(fact,['id','kind','source','animation','nodepath','track','field','expected'],'animation numeric fact');a=source_animation(fact['source'],fact['animation'],fact['nodepath']);require(fact['field']in ['length','times','values','transitions'],'Unknown animation numeric field')
   if fact['field']=='length':require(fact['track']is None,'Animation length selector rejected');selected=a['length']
   else:
    require(type(fact['track'])is str,'Animation track selector rejected');tracks=[t for t in a['tracks']if t['path']==fact['track']];require(len(tracks)==1,'Unknown/ambiguous animation numeric track');selected=tracks[0]['keys'][fact['field']]
   def flatten(value):
    if type(value)is list:return [element for child in value for element in flatten(child)]
    return [int(value)if type(value)is bool else number(value)]
   actual=flatten(selected)
  elif kind=='resource_geometry':
   fields(fact,['id','kind','resource','property','expected'],'resource geometry fact');require(fact['resource']is not None and fact['property']in ['width','height','columns','rows'],'Unknown resource geometry selector');resource=resources[resource_index(fact['resource'])];actual=[integer(resource[fact['property']],1,65535)]
  elif kind=='resource_cell':
   fields(fact,['id','kind','resource','expected'],'resource-cell fact');require(fact['resource']is not None,'Missing resource-cell binding');resource=resources[resource_index(fact['resource'])];require(all(type(resource.get(key))is int and resource[key]>0 for key in ['width','height','columns','rows']),'Invalid resource-cell geometry');actual=[resource['width']/resource['columns'],resource['height']/resource['rows']]
  elif kind=='derive':
   fields(fact,['id','kind','operation','inputs','expected'],'derived source fact');inputs=resolve(fact['inputs']);require(type(inputs)is list and 0<len(inputs)<=16 and all(type(x)in(int,float)for x in inputs),'Derived operands rejected');operation=fact['operation']
   if operation=='sum':actual=[sum(inputs)]
   elif operation=='product':require(len(inputs)==2,'Product arity rejected');actual=[inputs[0]*inputs[1]]
   elif operation=='difference':require(len(inputs)==2,'Difference arity rejected');actual=[inputs[0]-inputs[1]]
   elif operation=='ratio':require(len(inputs)==2 and inputs[1]!=0,'Ratio arity/divisor rejected');actual=[inputs[0]/inputs[1]]
   elif operation=='negate':require(len(inputs)==1,'Negation arity rejected');actual=[-inputs[0]]
   elif operation=='degrees':require(len(inputs)==1,'Degrees arity rejected');actual=[math.degrees(inputs[0])]
   else:raise ValueError('Unknown derived source operation')
  elif kind=='native_policy':
   fields(fact,['id','kind','sources','reason','expected'],'native policy');require(type(fact['sources'])is list and fact['sources']and all(path in sources for path in fact['sources'])and type(fact['reason'])is str and fact['reason'],'Unreviewed native policy');actual=fact['expected']
  else:raise ValueError('Unknown recipe fact kind')
  require(type(fact['expected'])is list and 0<len(fact['expected'])<=32 and len(actual)==len(fact['expected']),'Source fact shape mismatch')
  for observed,expected in zip(actual,fact['expected']):require(math.isclose(number(observed),number(expected),rel_tol=0,abs_tol=1e-12),'Recipe source expression mismatch: '+fact['id'])
  # Retain the reviewed decimal representation after checking source arithmetic;
  # repeated float additions must not rewrite established animation key bytes.
  values[fact['id']]=list(fact['expected'])
 actions=recipe['actions'];require(type(actions)is list and 0<len(actions)<=512,'Recipe action count rejected');lowered=[];media=set();slots=set()
 for action in actions:
  require(type(action)is dict,'Recipe action rejected');op=action.get('op');a=dict(action)
  if op in ('animation','media'):
   common=['op','id','name','role','resource','rect','flags','anchor'];names=common+(['source','nodepath']if op=='animation'else['duration','color'])
   fields(a,names,'media action');require(type(a['id'])is str and a['id']and a['id']not in media and type(a['name'])is str and a['name'],'Duplicate/invalid media identity');media.add(a['id']);integer(a['role'],1,12);integer(a['flags'],0,7);a['resource']=resource_index(a['resource']);a['rect']=vector(a['rect'],4);a['anchor']=vector(a['anchor'],2)
   if op=='animation':require(a['source']in sources and type(a['nodepath'])is str and a['nodepath'],'Unknown animation source binding')
   else:a['duration']=resolve(a['duration']);require(0<=a['duration']<=120,'Media duration rejected');a['color']=vector(a['color'],4)
  elif op=='bind':
   fields(a,['op','slot','media'],'binding action');require(a['slot']in SLOTS and a['slot']not in slots,'Unknown/duplicate presentation slot');slots.add(a['slot'])
  elif op=='track':
   fields(a,['op','media','property','times','values','update','interpolation','mode','eases'],'track action');integer(a['property'],1,18);integer(a['update'],0,1);integer(a['interpolation'],0,8);integer(a['mode'],0,2);a['times']=resolve(a['times']);a['values']=resolve(a['values']);require(type(a['times'])is list and 0<len(a['times'])<=128 and type(a['values'])is list and len(a['values'])==len(a['times'])and all(type(t)in(int,float)and 0<=t<=120 for t in a['times'])and a['times']==sorted(a['times']),'Track topology rejected')
   for value in a['values']:require(type(value)in(int,float)or(type(value)is list and 0<len(value)<=4 and all(type(x)in(int,float)for x in value)),'Track value shape rejected')
   if a['eases']is not None:a['eases']=vector(a['eases'],len(a['times']))
  elif op=='source_event':
   fields(a,['op','media','time','kind','source','animation','nodepath','track','method'],'source event action');integer(a['kind'],1,11);a['time']=resolve(a['time']);require(0<=a['time']<=120,'Source event time rejected');source=source_animation(a['source'],a['animation'],a['nodepath']);tracks=[t for t in source['tracks']if t['type']=='method'and t['path']==a['track']];require(len(tracks)==1,'Source event track rejected');require(type(a['method'])is str and len([(time,value)for time,value in zip(tracks[0]['keys']['times'],tracks[0]['keys']['values'])if time==a['time']and value=={'args':[],'method':a['method']}])==1,'Source callback expression mismatch')
  elif op=='event':fields(a,['op','media','time','kind'],'event action');integer(a['kind'],1,9);a['time']=resolve(a['time']);require(0<=a['time']<=120,'Event time rejected')
  elif op=='update':
   fields(a,['op','media','values'],'media update');require(type(a['values'])is dict and a['values']and set(a['values'])<=set(['duration','anchor']),'Unsupported media update');a['values']={key:(vector(value,2)if key=='anchor'else resolve(value))for key,value in a['values'].items()};require('duration'not in a['values']or 0<=a['values']['duration']<=120,'Updated duration rejected')
  else:raise ValueError('Unknown presentation recipe action')
  if 'media'in a:require(type(a['media'])is str and a['media']in media,'Unknown/forward media reference')
  lowered.append(a)
 parameters=recipe['parameters'];require(type(parameters)is dict and set(parameters)==set(PARAMETERS[:PARAMETERS.index('ReturnCamera')]),'Unknown presentation parameter');parameters={key:vector(value,4)for key,value in parameters.items()}
 context={};fields(recipe['context'],['party','battle','pr','box','dr','bash','effect'],'presentation context')
 for key,value in recipe['context'].items():
  require(type(key)is str and type(value)is dict and len(value)==1,'Context shape rejected');tag=next(iter(value));value=value[tag]
  if tag=='source':require(value in sources,'Unknown context source');context[key]=value
  elif tag=='resource':context[key]=resource_index(value)
  elif tag=='media':require(value in media,'Unknown context media');context[key]=value
  elif tag=='facts':require(type(value)is list,'Context fact vector rejected');context[key]=resolve(value)
  else:raise ValueError('Unknown presentation context tag')
 require(slots==set(SLOTS),'Missing presentation binding')
 reviews=recipe['animation_reviews'];require(type(reviews)is list and len(reviews)<=32,'Animation review count rejected');identities=set()
 for review in reviews:
  fields(review,['source','animation','nodepath','tracks'],'animation review');identity=(review['source'],review['animation'],review['nodepath']);require(identity not in identities,'Duplicate animation review');identities.add(identity);source=source_animation(*identity);actual=[dict(path=t['path'],type=t['type'],interpolation=t['interp'],keys=t['keys'])for t in source['tracks']];require(actual==review['tracks'],'Changed reviewed animation structure')
 from tools.round_animation_bindings import load
 context['animation_bindings']=load(recipe['animation_bindings'],ex)
 skills=recipe['skill_media'];require(type(skills)is dict and 0<len(skills)<=256,'Skill media count rejected');context['skill_media']={}
 for name,binding in skills.items():
  require(type(name)is str and name,'Skill binding identity rejected');fields(binding,['user_media','hit_media'],'skill media');require(all(value is None or(type(value)is str and value in media)for value in binding.values()),'Unknown skill media reference');context['skill_media'][name]=binding
 return lowered,parameters,context

def apply(p,recipe=None):
 actions,parameters,context=checked(read()if recipe is None else recipe,p.ex,p.resources);media={};p.animation_config=context.pop('animation_bindings');skills=context.pop('skill_media')
 for a in actions:
  op=a['op']
  if op=='animation':media[a['id']]=p.anim(a['source'],a['name'],a['role'],a['resource'],a['rect'],a['nodepath'],a['flags'],a['anchor'])
  elif op=='media':media[a['id']]=p.add(a['name'],a['role'],a['resource'],a['duration'],a['rect'],a['flags'],a['color'],a['anchor'])
  elif op=='bind':p.bind(a['slot'],media[a['media']])
  elif op=='track':p.track(media[a['media']],a['property'],a['times'],a['values'],a['update'],a['interpolation'],a['mode'],a['eases'])
  elif op in ('event','source_event'):p.event(media[a['media']],a['time'],a['kind'])
  elif op=='update':media[a['media']].update(a['values'])
 p.parameters=parameters
 context['skill_media']={name:{key:0xffffffff if value is None else media[value]['id']-1 for key,value in binding.items()}for name,binding in skills.items()}
 return {key:media[value]if type(value)is str and value in media else value for key,value in context.items()}
