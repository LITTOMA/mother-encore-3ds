"""Checked source-property bindings and exact reviewed audio limitations."""
import hashlib,re
from tools.extract_battle_entry import require,one,properties
from tools.round_presentation_recipe import fields,integer,read,ROOT

def load(path,ex):
 require(type(path)is str and path.startswith('content/')and '\\'not in path and ':'not in path and '..'not in path.split('/'),'Unreviewed animation binding path')
 require(hasattr(ex,'root'),'Animation binding extractor root missing')
 return checked(read(ex.root/path),ex)

def checked(value,ex):
 fields(value,['schema','kind','commit','sources','properties','methods','atlas','outline','interpolation','audio'],'animation bindings')
 require(type(value['schema'])is int and value['schema']==1 and value['kind']=='encore.round-animation.reviewed-bindings'and value['commit']==ex.lock['commit'],'Animation binding schema/pin rejected')
 require(type(value['sources'])is dict and value['sources'],'Missing animation binding sources')
 for source,digest in value['sources'].items():
  require(type(source)is str and source and '..'not in source.split('/')and ':'not in source and '\\'not in source and not source.startswith('/')and type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest),'Animation source identity rejected')
  require(hashlib.sha256(ex.data(source)).hexdigest()==digest,'Animation binding source mismatch')
 for key in ['properties','methods','atlas','outline','interpolation']:require(type(value[key])is dict and value[key],'Empty animation binding map')
 for path,prop in value['properties'].items():require(type(path)is str and path,'Invalid property path');integer(prop,1,18)
 for method,event in value['methods'].items():require(type(method)is str and re.fullmatch('[A-Za-z_][A-Za-z0-9_]*',method),'Invalid source method');integer(event,1,11)
 for path,property in value['atlas'].items():require(type(path)is str and path and property in ['columns','rows','texture'],'Unknown atlas mechanism')
 for path,expected in value['outline'].items():require(path in value['properties']and expected==[0.0],'Unsupported source outline')
 for source,target in value['interpolation'].items():require(source in ['1','2'],'Unsupported source interpolation');integer(target,0,8)
 require(type(value['audio'])is list and len(value['audio'])<=32,'Audio review count rejected');seen=set()
 for audio in value['audio']:
  fields(audio,['source','animation','index','properties'],'audio review');require(audio['source']in value['sources']and type(audio['animation'])is str and audio['animation'],'Audio binding source rejected');integer(audio['index'],0,128)
  identity=(audio['source'],audio['animation'],audio['index']);require(identity not in seen,'Duplicate audio review');seen.add(identity)
  fields(audio['properties'],['type','path','keys'],'audio properties');require(audio['properties']['type']=='audio'and type(audio['properties']['path'])is str and audio['properties']['path'],'Unsupported audio review type')
  fields(audio['properties']['keys'],['clips','times'],'audio keys');require(type(audio['properties']['keys']['clips'])is list and type(audio['properties']['keys']['times'])is list and len(audio['properties']['keys']['clips'])==len(audio['properties']['keys']['times']),'Audio key topology rejected')
  for clip in audio['properties']['keys']['clips']:
   fields(clip,['end_offset','start_offset','stream'],'audio clip');require(type(clip['end_offset'])in(int,float)and clip['end_offset']==0 and type(clip['start_offset'])in(int,float)and clip['start_offset']==0,'Unsupported audio clip offsets');fields(clip['stream'],['ExtResource'],'audio source reference');integer(clip['stream']['ExtResource'],1,65535)
  require(all(type(time)in(int,float)and time>=0 for time in audio['properties']['keys']['times']),'Unsupported audio time')
  text=ex.text(audio['source']);rid=int(one(r'^anims/'+re.escape(audio['animation'])+r' = SubResource\( (\d+) \)',text,'reviewed audio animation',re.M)[1]);checked_visual_source(text,rid,audio['source'],audio['animation'],value)
 return value

def checked_visual_source(text,rid,path,name,config):
 body=one(r'^\[sub_resource type="Animation" id='+str(rid)+r'\]\n(.*?)(?=^\[|\Z)',text,'reviewed animation body',re.M|re.S)[1]
 props=properties(body);omit=[]
 for review in config['audio']:
  if review['source']!=path or review['animation']!=name:continue
  prefix='tracks/'+str(review['index'])+'/'
  require({key:props.get(prefix+key)for key in review['properties']}==review['properties'],'Changed reviewed audio track')
  omit.append(prefix)
 # Only exact reviewed audio property records are removed. Other audio remains
 # visible to the animation parser and fails as an unsupported track type.
 if not omit:return text
 matches=list(re.finditer(r'^([A-Za-z_][A-Za-z0-9_/]*)\s*=\s*',body,re.M));records=[body[match.start():matches[i+1].start()if i+1<len(matches)else len(body)]for i,match in enumerate(matches)];filtered=body[:matches[0].start()]+''.join(record for match,record in zip(matches,records)if not any(match[1].startswith(prefix)for prefix in omit))
 return text.replace(body,filtered)
