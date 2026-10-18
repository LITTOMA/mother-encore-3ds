"""Source AreaRoom metadata for capability5 field maps; candidate only.

This bounded admission supports a non-Magicant scene with one native playable
party member and no flying-man NPC party. Unknown exported settings fail closed.
"""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,Vec2
from tools import podunk_field as f
RECIPE=ROOT/'content/mayor-area.json'


def fields(value,keys,label):require(type(value)is dict and set(value)==set(keys.split()),'Field Area '+label+' fields')
def path(value):return type(value)is str and value and not any(c in value for c in ('\\',':'))and all(32<=ord(c)<=126 for c in value)and all(x not in ('','.','..')for x in value.split('/'))
def unique(pairs):
    out={}
    for k,v in pairs:require(k not in out,'Duplicate Field Area field');out[k]=v
    return out

def policy(file=RECIPE):
    p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique);fields(p,'schema kind commit field_recipe script map_screen inventory global_data maps','recipe')
    require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.field-area.recipe'and p['commit']==PIN,'Field Area schema/pin')
    require(all(path(p[k])for k in ('field_recipe','script','map_screen','inventory','global_data'))and type(p['maps'])is list and 1<=len(p['maps'])<=16 and all(type(x)is str and re.fullmatch('[A-Za-z0-9_]+',x)for x in p['maps'])and len(set(p['maps']))==len(p['maps']),'Field Area recipe paths/maps')
    return p

def source(file=RECIPE):
    p=policy(file);recipe=f.load_recipe(ROOT/p['field_recipe']);ex=Extractor(ROOT);doc=Document(recipe['scene']['source'],ex.text(recipe['scene']['source']));root=doc.nodes[0]
    require(doc.ext_path(root.props.get('script'),'Script')==p['script']and root.attrs.get('type')=='Node2D','Field Area root script/class')
    code=ex.text(p['script']);maps=ex.text(p['map_screen']);inventory=ex.text(p['inventory'])
    require('ret += global.currentScene.get_player_map_offset()'in maps and 'global.get_player().position / 16'in maps and '!_is_in_sub_area()'in maps,'Field Area map calculation changed')
    require('item.get_data().get("map_for") == region'in inventory,'Field Area map ownership changed')
    defaults={'_player_map_offset':Vec2(0,0),'_region_name':'','_is_sub_area':False}
    require('export var _player_map_offset := Vector2.ZERO'in code and 'export var _region_name := ""'in code and 'export var _is_sub_area := false'in code,'Field Area exports changed')
    require(set(root.props)<=set(defaults)|{'script','area_bg_color','__meta__'},'Unreviewed AreaRoom root override')
    values={k:root.props.get(k,v)for k,v in defaults.items()};offset=values['_player_map_offset'];region=values['_region_name'];require(region and region==recipe['scene']['region']and region!='Magicant','Field Area requires mapped non-Magicant region')
    require(isinstance(offset,Vec2)and type(values['_is_sub_area'])is bool,'Field Area export values')
    def table(name):
        m=re.search(r'^const '+name+r' := (\{.*?^\})',code,re.M|re.S);require(m,'Field Area source dictionary '+name);return json.loads(m[1])
    visits=table('REGION_VISIT_FLAGS');overrides=table('MAP_AREA_OVERRIDES')
    require(region in visits and 'globaldata.set_flag(flag, true)'in code and '_update_visit_flags()\n\t_update_flying_man_status()'in code,'Field Area ready order changed')
    data=ex.text(p['global_data']);require('func set_flag(flag_name: String, value: bool, emit_signal := true):'in data and 'if emit_signal: global.emit_signal("flags_updated")'in data,'Field Area flag signal policy changed')
    guard=re.search(r'globaldata.flags\["([^\"]+)"\]',code);require(guard and 'global.partyNpcs.erase(globaldata.characters.flyingman)'in code,'Field Area NPC-party admission changed')
    require('area_bg_color'not in root.props,'Explicit AreaRoom color override needs source color admission')
    color=re.search(r'^export var area_bg_color := Color\("([0-9a-fA-F]{6})"\)',code,re.M);require(color,'Field Area background default')
    by_region={}
    for identity in p['maps']:
        data=ex.yaml('Data/Items/'+identity+'.yaml');target=data.get('map_for');require(type(target)is str and target and target not in by_region,'Field Area map item identity');by_region[target]=identity
    override=overrides.get(region,'');require(region in by_region and (not override or override in by_region),'Field Area map override has no mapped item')
    metadata=dict(source='res://'+doc.source,region=region,map_item=by_region[region],map_override=override,map_override_item=by_region.get(override,''),visit_flag=visits[region],party_guard_flag=guard[1],map_offset=[f.f32(offset.x),f.f32(offset.y)],map_divisor=16,background=int(color[1],16)*256+255,sub_area=values['_is_sub_area'])
    validate(metadata);return metadata,dict(sorted(ex.sources.items()))

def validate(a):
    fields(a,'source region map_item map_override map_override_item visit_flag party_guard_flag map_offset map_divisor background sub_area','metadata')
    require(type(a['source'])is str and a['source'].startswith('res://')and path(a['source'][6:])and len(a['source'])<=256,'Field Area source')
    for k in ('region','map_item','map_override','map_override_item','visit_flag','party_guard_flag'):
        v=a[k];require(type(v)is str and len(v)<=64 and ((not v and k in ('map_override','map_override_item'))or re.fullmatch('[A-Za-z0-9_ ]+',v)),'Field Area identity '+k)
    require(bool(a['map_override'])==bool(a['map_override_item']),'Field Area map override pair')
    def number(x,lo,hi):return type(x)in(int,float)and math.isfinite(x)and lo<=x<=hi
    require(type(a['map_offset'])is list and len(a['map_offset'])==2 and all(number(x,-1e6,1e6)for x in a['map_offset'])and number(a['map_divisor'],1e-6,4096),'Field Area map position')
    require(type(a['background'])is int and 0<=a['background']<2**32 and a['background']&255==255 and type(a['sub_area'])is bool,'Field Area color/sub-area')
    return a

def encode(a,S):
    validate(a);refs=[]
    for key in ('source','region','map_item','map_override','map_override_item','visit_flag','party_guard_flag'):refs.extend(S(a[key]))
    return struct.pack('<14I3f3I',*refs,*a['map_offset'],a['map_divisor'],a['background'],int(a['sub_area']),0)

def decode(raw,pool):
    require(len(raw)==80,'Field Area record size');v=struct.unpack('<14I3f3I',raw);a={}
    for i,k in enumerate(('source','region','map_item','map_override','map_override_item','visit_flag','party_guard_flag')):
        off,length=v[i*2:i*2+2];require((length>0 or off==0)and off<=len(pool)and length<=len(pool)-off,'Field Area text reference');a[k]=pool[off:off+length].decode('utf8')
    require(v[18]in(0,1)and not v[19],'Field Area flags/reserved')
    a.update(map_offset=list(v[14:16]),map_divisor=v[16],background=v[17],sub_area=bool(v[18]));return validate(a)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);o=ap.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Field Area candidate must stay under build')
    a,sources=source();model=f.Model(f.load_recipe(ROOT/policy()['field_recipe']));atlases=f.Atlases(model);paths=[model.recipe['atlas_directory']+'/atlas-%02d.t3x'%i for i in range(len(atlases.pages))];blob=f.encode_map(model,atlases,paths,area=a);f.unpack_container(blob,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT)
    out.mkdir(parents=True,exist_ok=True);(out/'mayor.encmap').write_bytes(blob);receipt=dict(commit=PIN,metadata=a,sources=sources,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),map_sha256=hashlib.sha256(blob).hexdigest(),bytes=len(blob),scope='AreaRoom ready/map/background admission; no scene staging or hardware pass')
    (out/'source.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('AreaRoom candidate:',len(blob),'bytes; map capability5; not staged')
if __name__=='__main__':main()
