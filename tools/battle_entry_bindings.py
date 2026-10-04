"""Bounded offline selectors for the existing battle-entry data adapter."""
import copy,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
IR='content/battle-entry-bindings.json'
# Typed projection schemas; source values and bindings are entirely external.
TEMPLATE_FIELDS={
 'derived_layout':'method party_info_position plate_position plate_size command_icon_positions cursor_centers party_hidden_position party_shown_position party_sprite_size',
 'party_sprite':'source frame grid control_hidden_y distance_to_shown show_tween_seconds',
 'party_transition':'source_function screen_offset crouch_frame_coords jump_frame_coords squash_scale squash_segment_seconds squash_transition squash_ease jump_method_time initial_jump_wait per_member_wait jump_start_for_first_party jump_duration jump_up_seconds jump_down_seconds jump_down_delay jump_target_x jump_target_y jump_apex_offset jump_height_threshold jump_apex_formula x_transition y_up_transition y_up_ease y_down_transition y_down_ease scale_delay scale_duration scale_target scale_transition scale_ease arrival_quake_intensity arrival_hide_deferred arrival_quake_base_magnitude arrival_quake_steps center_nudge',
 'enemy_transition':'duration transition ease target tint_to tint_seconds shake_frequency shake_range per_enemy_delay_numerator shaking_cleared_after_reveal_delay',
 'menu':'source_indices actions initial_source_index hidden_source_indices wrap_around horizontal_only cursor_size cursor_offset cursor_anchor_delta cursor_tween_seconds cursor_transition cursor_ease cursor_repeat_delay_seconds cursor_scale_steps move_sound_key select_sound_key cancel_sound_key input_gate cancel_at_first_member confirm_boundary'}
def binding_digest():return hashlib.sha256((ROOT/IR).read_bytes()).hexdigest()

def require(ok,message):
    if not ok:raise ValueError(message)
def fields(row,keys):require(type(row)is dict and set(row)==set(keys),'Unknown/missing battle entry binding fields')
def safe(path):return type(path)is str and 0<len(path)<=512 and ':'not in path and '\\'not in path and all(p not in('','.','..')for p in path.split('/'))
def read(path):
    def unique(items):
        result={}
        for key,value in items:require(key not in result,'Duplicate battle entry binding key');result[key]=value
        return result
    require(Path(path).stat().st_size<=512*1024,'Battle entry binding authoring limit')
    return json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=lambda v:(_ for _ in ()).throw(ValueError('Nonfinite binding JSON')))
def function(source,name):
    result=re.findall(r'^func '+re.escape(name)+r'\([^\n]*\n.*?(?=^func |\Z)',source,re.M|re.S)
    require(len(result)==1,'Unknown/ambiguous entry source method');return result[0].rstrip()
def get(value,path):
    for token in path.split('/'):
        if type(value)is list:require(token.isdigit(),'Invalid list field index');value=value[int(token)]
        else:require(type(value)is dict and token in value,'Unknown battle binding field path');value=value[token]
    return value
def put(value,path,item):
    parts=path.split('/');target=get(value,'/'.join(parts[:-1]));target[int(parts[-1])if type(target)is list else parts[-1]]=item
def props(ex,source,path):
    from tools.extract_battle_entry import node
    return node(ex.text(source),path)
def expression(value,context,recipe,ex,depth=0):
    require(depth<=16,'Battle selector nesting limit')
    if type(value)in(int,float,bool,str):
        require(type(value)is not float or math.isfinite(value),'Nonfinite binding expression');return value
    require(type(value)is list and 0<len(value)<=32 and type(value[0])is str,'Invalid battle source expression')
    op,args=value[0],value[1:];evaluate=lambda v:expression(v,context,recipe,ex,depth+1)
    if op=='get':require(len(args)==1 and safe(args[0]),'Unsafe expression field');return get(context,args[0])
    if op=='index':
        require(len(args)==2 and type(args[1])is int,'Invalid vector index');data=evaluate(args[0]);require(type(data)is list and 0<=args[1]<len(data),'Unknown vector field');return data[args[1]]
    if op=='vector':return [evaluate(v)for v in args]
    if op=='constant':
        require(args==['Color.black'],'Unknown engine Variant constant');return [0,0,0,1]
    if op=='has':
        require(len(args)==3 and args[0]in recipe['sources']and type(args[2])is str and args[2],'Invalid source semantic fact');require(args[2]in function(ex.text(args[0]),args[1]),'Missing source semantic fact');return True
    if op=='menu_indices':
        require(len(args)==1 and type(args[0])is bool,'Invalid menu index selector');indices=[r['source_index']for r in recipe['menu']]
        return [i for i in range(6)if i not in indices]if args[0]else indices
    if op in('add','sub','mul','div'):
        require(2<=len(args)<=8 and(op=='add'or len(args)==2),'Invalid binding arithmetic arity');numbers=[evaluate(v)for v in args]
        require(all(type(v)in(int,float)and math.isfinite(v)for v in numbers),'Invalid numeric source field')
        if op=='add':return sum(numbers)
        if op=='sub':return numbers[0]-numbers[1]
        if op=='mul':return numbers[0]*numbers[1]
        require(numbers[1]!=0,'Zero source divisor');return numbers[0]/numbers[1]
    if op=='node':
        require(len(args)==3 and args[0]in recipe['sources']and(args[1]=='.'or safe(args[1]))and type(args[2])is str,'Unknown source node selector')
        p=props(ex,args[0],args[1]);key=args[2]
        # Serialized omissions use only engine schema defaults, never supplied IR fallback values.
        if key not in p:
            require(key.startswith(('margin_','anchor_'))or key=='frame','Missing source node property');return 0
        return p[key]
    if op=='regex':
        require(len(args)==5 and args[0]in recipe['sources']and(args[1]is None or type(args[1])is str)and type(args[2])is str and len(args[2])<=1024 and type(args[3])is int and args[3]>0,'Invalid source capture selector')
        text=ex.text(args[0]);text=function(text,args[1])if args[1]else text;matches=list(re.finditer(args[2],text,re.M))
        codec=args[4]
        if codec=='match_number':require(0<args[3]<=len(matches),'Missing indexed source capture');raw=matches[args[3]-1][1]
        else:require(len(matches)==1 and args[3]<=len(matches[0].groups()),'Missing/ambiguous source capture');raw=matches[0][args[3]]
        if codec in('number','match_number'):return float(raw)
        if codec=='string':return raw
        if codec=='tween':require(raw in('LINEAR','QUAD','QUART'),'Unsupported tween transition');return {'LINEAR':1,'QUAD':.5,'QUART':.25}[raw]
        require(codec=='vector','Unknown source capture codec');result=[float(n.strip())for n in raw.split(',')];require(0<len(result)<=4,'Source capture vector width');return result
    if op=='track_value':
        require(len(args)==4 and type(args[3])is int,'Invalid source track selector');clips=[a for a in context['animations']if a['name']==args[0]];require(len(clips)==1,'Unknown source animation alias')
        tracks=[t for t in clips[0]['tracks']if t['path']==args[1]];require(len(tracks)==1 and args[2]in('times','values'),'Unknown source animation field')
        rows=tracks[0]['keys'][args[2]];require(-len(rows)<=args[3]<len(rows),'Source track key index');return rows[args[3]]
    if op=='yaml':
        require(len(args)==2 and args[0]in recipe['sources']and safe(args[1]),'Unknown source YAML field');return get(ex.yaml(args[0]),args[1])
    if op in('menu_positions','menu_centers'):
        require(not args,'Invalid menu layout selector')
        menu=recipe['menu'];scene=recipe['animations'][1]['source'];parent=recipe['menu_parent'];p=props(ex,scene,parent)
        x=p['margin_left'];positions=[]
        for row in menu:
            icon=props(ex,scene,row['node']);size=[icon['margin_right']-icon.get('margin_left',0),icon['margin_bottom']]
            require(size[0]>0 and size[1]>0,'Invalid source icon dimensions');y=props(ex,scene,parent.rsplit('/',1)[0])['margin_top']
            positions.append([x,y]);x+=size[0]+p['custom_constants/separation']
        if op=='menu_centers':
            delta=get(context,'menu/cursor_anchor_delta');return [[p[0]+delta[0],p[1]+delta[1]]for p in positions]
        return positions
    raise ValueError('Unknown battle source expression operator')

def load(ex,document=None):
    recipe=read(ROOT/IR)if document is None else document
    fields(recipe,('schema','kind','commit','sources','animations','scene_layout','plate_layout','templates','bindings','menu','menu_parent','source_contracts','plate','compiler'))
    require(type(recipe['schema'])is int and recipe['schema']==1 and recipe['kind']=='encore.battle-entry-bindings'and recipe['commit']==ex.lock['commit'],'Battle entry binding schema/pin mismatch')
    require(type(recipe['sources'])is dict and 0<len(recipe['sources'])<=256,'Battle entry binding source count')
    for path,digest in recipe['sources'].items():
        require(safe(path)and type(digest)is str and re.fullmatch('[a-f0-9]{64}',digest),'Invalid entry binding source record');require(hashlib.sha256(ex.data(path)).hexdigest()==digest,'Changed entry binding source')
    require(type(recipe['animations'])is list and len(recipe['animations'])==5,'Missing entry animation bindings')
    aliases=set()
    from tools.extract_battle_entry import animation
    clips=[]
    for row in recipe['animations']:
        fields(row,('source','resource_id','alias','player_node','source_name'));require(row['source']in recipe['sources']and type(row['resource_id'])is int and row['resource_id']>0 and type(row['alias'])is str and row['alias']not in aliases,'Invalid entry animation identity')
        player=props(ex,row['source'],row['player_node']);require(player.get('anims/'+row['source_name'])=={'SubResource':row['resource_id']},'AnimationPlayer/source resource binding mismatch');aliases.add(row['alias']);clips.append(animation(ex.text(row['source']),row['resource_id'],row['source'],row['alias']))
    require(type(recipe['bindings'])is dict and 0<len(recipe['bindings'])<=256 and type(recipe['templates'])is dict,'Invalid entry template bindings')
    fields(recipe['templates'],TEMPLATE_FIELDS)
    for name,keys in TEMPLATE_FIELDS.items():fields(recipe['templates'][name],keys.split())
    fields(recipe['templates']['party_transition']['center_nudge'],'party_count center_x distance_lt offset duration transition ease random_sign'.split())
    for step in recipe['templates']['party_transition']['arrival_quake_steps']:fields(step,('from','to','duration','ease'))
    for step in recipe['templates']['menu']['cursor_scale_steps']:fields(step,('from','to','duration','ease'))
    for source,methods in recipe['source_contracts'].items():
        require(source in recipe['sources']and type(methods)is dict and methods,'Unknown entry source contract')
        for name,expected in methods.items():require(function(ex.text(source),name)==expected,'Changed entry source algorithm: '+name)
    for key,paths in [('scene_layout',recipe['scene_layout']),('plate_layout',recipe['plate_layout'])]:
        require(type(paths)is list and 0<len(paths)<=64 and len(set(paths))==len(paths),'Missing/duplicate layout node binding')
        source=recipe['animations'][1]['source']if key=='scene_layout'else recipe['plate']['source']
        for path in paths:require(path=='.'or safe(path),'Unsafe entry layout node');props(ex,source,path)
    require(type(recipe['menu'])is list and len(recipe['menu'])==3,'Missing battle menu bindings')
    source=recipe['compiler']['menu_source'];require(source in recipe['sources'],'Unknown entry menu source');text=ex.text(source)
    constants=dict(re.findall(r'^const (ACTION_[A-Z]+) := "([^"]+)"',text,re.M));order=re.findall(r'^const _actions := \[([^\n]+)\]',text,re.M);require(len(order)==1,'Unknown menu source order');order=[v.strip()for v in order[0].split(',')]
    active=re.findall(r'^var _active_actions := \[([^\n]+)\]',text,re.M);require(len(active)==1,'Unknown active menu source order');active=[v.strip()for v in active[0].split(',')]
    expected=[i for i,v in enumerate(active)if v!='""'];require([r['source_index']for r in recipe['menu']]==expected,'Menu source order/coverage mismatch')
    require(len({r['id']for r in recipe['menu']})==len(recipe['menu'])and len({r['node']for r in recipe['menu']})==len(recipe['menu']),'Duplicate entry menu binding')
    scene=ex.text(recipe['animations'][1]['source']);textures={int(i):p for p,i in re.findall(r'^\[ext_resource path="res://([^\n"]+)" type="Texture" id=(\d+)\]',scene,re.M)}
    for row in recipe['menu']:
        fields(row,('source_index','id','label_key','icon_asset','node','asset_alias','source_constant'));require(type(row['source_index'])is int and order[row['source_index']]==row['source_constant']and constants.get(row['source_constant'])==row['id'],'Menu action source constant mismatch')
        require(row['node'].startswith(recipe['menu_parent']+'/')and row['node']in recipe['scene_layout']and type(row['asset_alias'])is str and type(row['icon_asset'])is str,'Menu node/asset alias mismatch')
        texture=props(ex,recipe['animations'][1]['source'],row['node'])['texture'];require(type(texture)is dict and textures[texture['ExtResource']]in recipe['sources'],'Unknown menu icon texture source')
        if row['source_index']!=0:
            labels=re.findall(r'get_child\(0\).text = "([^"]+%s)" % action_id.to_upper\(\)',function(text,'_update_name_box'))
            require(len(labels)==1 and row['label_key']==labels[0]%row['id'].upper(),'Menu action source label mapping mismatch')
    require(recipe['templates']['menu']['actions']==[{k:r[k]for k in ('source_index','id','label_key','icon_asset')}for r in recipe['menu']],'Missing/mismatched projected source menu bindings')
    def numeric_fields(value,path=''):
        if type(value)in(int,float,bool):return [path]
        if type(value)is list:return [p for i,v in enumerate(value)for p in numeric_fields(v,path+'/'+str(i))]
        if type(value)is dict:return [p for k,v in value.items()for p in numeric_fields(v,path+'/'+k)]
        return []
    targets=set(recipe['bindings'])|{'menu/actions'}
    require(all(any(path[1:]==target or path[1:].startswith(target+'/')for target in targets)for path in numeric_fields(recipe['templates'])),'Missing numerical source binding coverage')
    def dependencies(selector):
        if type(selector)is not list:return []
        if selector and selector[0]=='get':return [selector[1]]
        return [p for v in selector[1:]for p in dependencies(v)]
    seen=set();visiting=set()
    def check_dependency(target):
        require(target not in visiting,'Cyclic source field binding')
        if target in seen:return
        visiting.add(target)
        for reference in dependencies(recipe['bindings'][target]):
            parents=[p for p in recipe['bindings']if reference==p or reference.startswith(p+'/')]
            require(len(parents)==1,'Unbound/ambiguous source field dependency');check_dependency(parents[0])
        visiting.remove(target);seen.add(target)
    for target in recipe['bindings']:check_dependency(target)
    context=dict(copy.deepcopy(recipe['templates']),animations=clips)
    require(context['party_sprite']['source']==recipe['animations'][4]['source'],'Unknown party portrait source binding')
    for target,selector in recipe['bindings'].items():
        require(safe(target),'Unsafe entry template target');expected=expression(selector,context,recipe,ex);actual=get(context,target)
        def equal(a,b):
            if type(a)is list and type(b)is list:return len(a)==len(b)and all(equal(x,y)for x,y in zip(a,b))
            if type(a)in(int,float)and type(b)in(int,float):return math.isfinite(a)and math.isfinite(b)and abs(a-b)<=1e-12
            return type(a)is type(b)and a==b
        require(equal(actual,expected),'Entry template/source selector mismatch: '+target)
    fields(recipe['plate'],('source','background','content','counter','name','background_asset','frame_asset','counter_asset','digits_asset','name_asset','label_assets','stats'))
    fields(recipe['compiler'],('menu_source','plate_depths','menu_depths','background_margins','font_manifest'))
    for key,count in [('plate_depths',5),('menu_depths',4),('background_margins',4)]:require(type(recipe['compiler'][key])is list and len(recipe['compiler'][key])==count and all(type(v)is int and 0<=v<=4096 for v in recipe['compiler'][key]),'Invalid native layout policy vector')
    require(recipe['plate']['source']in recipe['sources']and len(recipe['plate']['stats'])==2,'Missing entry plate source/stat binding')
    for row in recipe['plate']['stats']:
        fields(row,('stat','label','counter','role','digits'));require(row['stat']in('hp','pp')and row['role']==(7 if row['stat']=='hp'else 8)and len(row['digits'])==3,'Unsupported entry plate stat schema')
        require(row['label']in recipe['plate_layout']and row['counter']in recipe['plate_layout'],'Unknown plate stat source node')
        positions=[]
        for i,digit in enumerate(row['digits']):
            fields(digit,('node','power','binding','label'));require(type(digit['power'])is int and type(digit['binding'])is int and digit['power']==2-i and digit['binding']==i and digit['node']in recipe['plate_layout']and digit['label']==Path(digit['node']).name.lower().split('_')[0]+'_'+Path(digit['node']).name.split('_')[-1],'Invalid entry decimal slot binding');props(ex,recipe['plate']['source'],digit['node'])
            positions.append(props(ex,recipe['plate']['source'],digit['node'])['position'])
        require(len({d['node']for d in row['digits']})==3 and positions==sorted(positions),'Duplicate/reordered source decimal node')
    require([r['stat']for r in recipe['plate']['stats']]==['hp','pp'],'Entry plate stat coverage/order mismatch')
    return recipe,context

def verify_ir(ir,ex,recipe=None):
    recipe,context=load(ex,recipe)
    for key in ('derived_layout','party_sprite','party_transition','enemy_transition'):require(ir['presentation'][key]==context[key],'Stale entry binding projection: '+key)
    require(ir['menu']==context['menu'],'Stale entry menu projection');require(ir['animations']==context['animations'],'Stale entry animation projection')
    reference=ir.get('entry_bindings');require(reference==dict(path=IR,sha256=binding_digest()),'Missing/stale independent entry binding identity')
    return recipe

def compiler(ir,assets,ex):
    recipe=verify_ir(ir,ex);scene=recipe['animations'][1]['source'];textures={int(i):p for p,i in re.findall(r'^\[ext_resource path="res://([^\n"]+)" type="Texture" id=(\d+)\]',ex.text(scene),re.M)}
    for row in recipe['menu']:
        source=textures[props(ex,scene,row['node'])['texture']['ExtResource']];resources=[r for r in assets['resources']if r['name']==row['asset_alias']]
        require(len(resources)==1 and resources[0]['source']==source and ir['presentation']['assets'][row['icon_asset']]['source']==source,'Menu compiled resource/source alias mismatch')
    plate=recipe['plate'];text=ex.text(plate['source']);textures={int(i):p for p,i in re.findall(r'^\[ext_resource path="res://([^\n"]+)" type="Texture" id=(\d+)\]',text,re.M)}
    pairs=[(plate['frame_asset'],'.'),(plate['background_asset'],plate['background']),(plate['counter_asset'],plate['stats'][0]['counter']),(plate['digits_asset'],plate['stats'][0]['digits'][0]['node'])]+list(zip(plate['label_assets'],[r['label']for r in plate['stats']]))
    for alias,node in pairs:
        p=props(ex,plate['source'],node);source=textures[p['texture']['ExtResource']];rows=[r for r in assets['resources']if r['name']==alias]
        require(len(rows)==1 and rows[0]['source']==source,'Compiled plate resource/source alias mismatch')
    require(safe(recipe['compiler']['font_manifest']),'Unsafe font receipt path');font=read(ex.root/recipe['compiler']['font_manifest'])['recipe']['font']
    require(font['name']==plate['name_asset']and font['source']in ir['sources'],'Unknown compiled name font alias')
    require(len([r for r in assets['resources']if r['name']==plate['name_asset']and r.get('output')==font['output']and r.get('kind')=='texture'and r.get('frames')==1])==1,'Compiled font manifest resource mismatch')
    return recipe

def fixtures(output,project=None,recipe=None):
    """Actual original entry, changed adapter margin; no fixture gameplay rules."""
    import tempfile
    from tools.extract_battle_entry import Extractor
    from tools import native_battle
    global ROOT
    project=ROOT if project is None else Path(project).resolve();output=Path(output).resolve();require(output.is_relative_to(project/'build'),'Fixture outputs must stay in build')
    baseline=read(project/IR)if recipe is None else recipe;output.mkdir(parents=True,exist_ok=True);original=ROOT
    try:
        for label,delta in [('baseline',0),('changed',1)]:
            recipe=copy.deepcopy(baseline);recipe['compiler']['background_margins'][0]+=delta
            with tempfile.TemporaryDirectory(dir=project/'build')as temporary:
                ROOT=Path(temporary);target=ROOT/IR;target.parent.mkdir();target.write_text(json.dumps(recipe,indent=2),encoding='utf8')
                entry=Extractor(project).build();assets=read(project/entry['presentation']['asset_receipt_path']);native_battle.verify_sources(entry)
                tables=native_battle.lower(entry,assets,(project/'romfs/data/opening.encroom').read_bytes());blob=native_battle.encode(tables,entry['commit']);native_battle.parse_sections(blob)
                (output/str(label+'.encbattle')).write_bytes(blob)
        require((output/'baseline.encbattle').read_bytes()==(project/'romfs/data/opening.encbattle').read_bytes(),'Entry fixture baseline drift')
        require((output/'baseline.encbattle').read_bytes()!=(output/'changed.encbattle').read_bytes(),'Entry adapter data fixture did not change binary')
    finally:ROOT=original
    print('Original-entry compiler fixtures; only plate BG left margin differs by one')

if __name__=='__main__':
    import argparse,sys
    sys.path.insert(0,str(ROOT));sys.modules['tools.battle_entry_bindings']=sys.modules[__name__]
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['verify','fixtures']);parser.add_argument('--out',type=Path);args=parser.parse_args()
    if args.action=='fixtures':require(args.out is not None,'Fixture output required');fixtures(args.out)
    else:
        from tools.extract_battle_entry import Extractor
        load(Extractor(ROOT));print('Checked independent battle entry bindings and actual original source selectors')
