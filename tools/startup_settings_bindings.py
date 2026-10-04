"""Checked external bindings for the existing ENCSETUI compiler.

Bindings select source nodes; source topology, material inheritance and texture
identity are checked before use. This adapter introduces no gameplay defaults.
"""
import hashlib
import json
from pathlib import Path
import re
from tools.extract_battle_entry import Extractor, node, one, require, properties

ROOT=Path(__file__).resolve().parents[1]
IR=ROOT/'content/startup-settings-bindings.json'

def load_json(path):
    def unique(pairs):
        value={}
        for key,item in pairs:
            require(key not in value,'Duplicate settings binding field: '+key);value[key]=item
        return value
    return json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique)

def fields(value,expected):
    require(type(value)is dict and set(value)==set(expected),'Unknown/missing settings binding fields')

def safe(path):
    return type(path)is str and 0<len(path)<=512 and all(32<=ord(c)<=126 and c not in '\\:?#' for c in path) and all(p not in ('','.','..')for p in path.split('/'))

def refs(scene):
    return {int(i):path for path,i in re.findall(r'^\[ext_resource path="res://([^"]+)"[^\n]* id=(\d+)\]',scene,re.M)}

def node_attrs(scene,path):
    found=[]
    for block in re.findall(r'^\[node ([^\n]+)\]',scene,re.M):
        a=dict(re.findall(r'(\w+)="([^"]*)"',block));parent=a.get('parent')
        actual='.' if parent is None else a['name']if parent=='.'else parent+'/'+a['name']
        if actual==path:found.append(block)
    require(len(found)==1,'Missing/ambiguous settings node: '+path)
    return found[0]

def children(scene,parent):
    return [dict(re.findall(r'(\w+)="([^"]*)"',block))['name']for block in re.findall(r'^\[node ([^\n]+)\]',scene,re.M) if dict(re.findall(r'(\w+)="([^"]*)"',block)).get('parent')==parent]

def texture(scene,path):
    value=node(scene,path).get('texture')
    require(type(value)is dict and set(value)=={'ExtResource'}and value['ExtResource']in refs(scene),'Unsupported settings texture binding')
    return refs(scene)[value['ExtResource']]

def material(scene,path):
    value=node(scene,path)
    if value.get('use_parent_material',False):
        require(path!='.','Root material inheritance rejected')
        return material(scene,path.rsplit('/',1)[0]if '/'in path else '.')
    resource=value.get('material')
    require(type(resource)is dict and set(resource)=={'ExtResource'}and resource['ExtResource']in refs(scene),'Missing/unsupported skin material')
    return refs(scene)[resource['ExtResource']]

def inherited_label(ex,scene,path,expected_source):
    block=node_attrs(scene,path);match=re.search(r'instance=ExtResource\( (\d+) \)',block)
    require(match is not None and refs(scene).get(int(match[1]))==expected_source,'Unreviewed label inheritance')
    base=node(ex.text(expected_source),'.');base.update(node(scene,path));return base

def manifest_rows(manifest,generated_roles):
    values=manifest['resources'];recipes=manifest['recipe'].get('resources',[]);result={};generated=set()
    for entry in values:
        role=entry.get('name',entry.get('role'));path=entry.get('output',entry.get('path'))
        require(type(role)is str and role not in result and safe(path),'Invalid/duplicate manifest role')
        source=entry.get('source')
        if source is None:
            matches=[row for row in recipes if row.get('name',row.get('role'))==role]
            if not matches:
                require(role in generated_roles,'Unsupported source-less skin manifest role');generated.add(role)
            else:
                require(len(matches)==1,'Duplicate manifest source role');source=matches[0]['source']
                require(matches[0].get('output',matches[0].get('path'))==path,'Manifest recipe/output mismatch')
        result[role]=(source,path)
    require(generated==set(generated_roles),'Unknown generated skin manifest role')
    return result

def validate(ir,ex=None):
    fields(ir,('schema','kind','commit','sources','scene','script','scenario','shader','global_data','row_parent','value_parent','rows','panels','confirmation_fields','confirmation_choices','resources','skin_nodes','skin_manifests','skin_bindings'))
    require(type(ir['schema'])is int and ir['schema']==1 and ir['kind']=='encore.startup-settings-bindings','Unsupported settings binding schema')
    ex=ex or Extractor(ROOT);require(ir['commit']==ex.lock['commit'],'Settings binding pin mismatch')
    require(type(ir['sources'])is dict and ir['sources'],'Missing settings binding sources')
    for path,digest in ir['sources'].items():
        require(safe(path)and type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest),'Invalid binding source fingerprint')
        require(hashlib.sha256(ex.data(path)).hexdigest()==digest,'Settings binding source changed: '+path)
    for key in ('scene','script','scenario','shader','global_data'):
        require(ir[key]in ir['sources'],'Unreviewed binding source')
    scene=ex.text(ir['scene']);script=ex.text(ir['script']);scenario=ex.yaml(ir['scenario'])['scenario']
    require(type(ir['rows'])is list and len(ir['rows'])==4,'Missing settings rows')
    roles=[]
    for row in ir['rows']:
        fields(row,('role','label','value'));roles.append(row['role'])
        require(safe(row['label'])and (row['value']is None or safe(row['value'])),'Invalid settings row path')
        node(scene,row['label'])
        if row['value']is not None:node(scene,row['value'])
    require(roles==['speed','flavor','prompts','advance'],'Unknown/reordered/duplicate setting role')
    require(children(scene,ir['row_parent'])==[r['label'].split('/')[-1]for r in ir['rows']],'Settings row source coverage changed')
    require(children(scene,ir['value_parent'])==[r['value'].split('/')[-1]for r in ir['rows']if r['value']is not None],'Settings value source coverage changed')
    require(type(ir['panels'])is list and len(ir['panels'])==3,'Missing setting panels')
    for index,panel in enumerate(ir['panels']):
        fields(panel,('role','node','container','labels','inherited_source','value_keys','value_constant'))
        require(panel['role']==roles[index]and safe(panel['node'])and safe(panel['container']),'Unknown/reordered panel role')
        require(panel['container']==panel['node']+'/VBoxContainer','Wrong panel container')
        node(scene,panel['node'])
        require(type(panel['labels'])is list and panel['labels']and len(set(panel['labels']))==len(panel['labels']),'Missing/duplicate panel labels')
        require(children(scene,panel['container'])==panel['labels'],'Panel label source coverage changed')
        for name in panel['labels']:
            require(type(name)is str and '/'not in name,'Invalid panel label')
            p=panel['container']+'/'+name
            if panel['inherited_source']is None:require('instance='not in node_attrs(scene,p),'Unhandled panel inheritance');node(scene,p)
            else:
                require(panel['inherited_source']in ir['sources'],'Unreviewed panel inheritance');inherited_label(ex,scene,p,panel['inherited_source'])
        variable=one(r'onready var (\w+) := \$'+re.escape(panel['node'])+r'\s*$',script,'panel source variable')[1]
        require(re.search(r'\t'+str(index)+r':\s*\n\t+_show_setting_panel\('+re.escape(variable)+r'\)',script),'Setting role does not match source index')
        expression=one(r'^\s*\$'+re.escape(ir['rows'][index]['value'])+r'\.text = (.*)$',script,'setting value expression')[1]
        match=re.fullmatch(r'"([A-Z_]+)" \+ globaldata\.([A-Za-z_]+)(\[speed_index\]|\.to_upper\(\))',expression)
        require(match is not None and type(panel['value_constant'])is str and re.fullmatch('[A-Z_]+',panel['value_constant']),'Unknown setting value expression')
        if match[3]=='[speed_index]':require(match[2]==panel['value_constant'],'Wrong source value constant')
        else:require('globaldata.'+match[2]+' = globaldata.'+panel['value_constant']+'[cursor_index]'in script,'Wrong source selected-value constant')
        values=json.loads(one(r'^const '+re.escape(panel['value_constant'])+r' := (\[[^\n]+\])',ex.text(ir['global_data']),'setting value constant')[1])
        require(type(values)is list and all(type(v)is str for v in values),'Unsupported setting label values')
        expected_keys=[match[1]+(value.upper()if match[3]=='.to_upper()'else value)for value in values]
        require(panel['value_keys']==expected_keys and len(expected_keys)==len(panel['labels']),'Setting value translation bindings changed')
    require(type(ir['confirmation_fields'])is list,'Missing confirmation fields')
    naming=[step for step in scenario if step['type']=='naming']
    require(len(ir['confirmation_fields'])==len(naming)==6,'Confirmation coverage changed')
    require('find_node("Confirm%s" % i)'in script and 'get_node(_scenario[i].sprite).duplicate()'in script,'Unreviewed confirmation source mechanism')
    for index,(row,step)in enumerate(zip(ir['confirmation_fields'],naming)):
        fields(row,('target','parent','card','icon'))
        require(row['target']==step['target']and row['card'].endswith('/Confirm'+str(index))and row['card'].rsplit('/',1)[0]==row['parent'],'Confirmation identity/order mismatch')
        require(row['icon'].split('/')[-1]==step['sprite'],'Wrong confirmation icon mapping')
        for p in (row['parent'],row['card'],row['icon']):require(safe(p),'Invalid confirmation path');node(scene,p)
        texture(scene,row['icon'])
    require(type(ir['confirmation_choices'])is list and len(ir['confirmation_choices'])==2 and len(set(ir['confirmation_choices']))==2,'Missing/duplicate confirmation choices')
    choice_parent=ir['confirmation_choices'][0].rsplit('/',1)[0]
    require(all(p.rsplit('/',1)[0]==choice_parent for p in ir['confirmation_choices'])and children(scene,choice_parent)==[p.split('/')[-1]for p in ir['confirmation_choices']],'Confirmation choice source order changed')
    for p in ir['confirmation_choices']:node(scene,p)
    require(type(ir['resources'])is list and len(ir['resources'])==3,'Missing setting resource roles')
    require([r.get('role')for r in ir['resources']]==['box','card','inside'],'Unknown/reordered/duplicate setting resource role')
    for row in ir['resources']:
        fields(row,('role','node'));require(safe(row['node']),'Invalid setting resource node')
        require(material(scene,row['node'])==ir['shader'],'Setting resource shader mismatch');texture(scene,row['node'])
    require(type(ir['skin_nodes'])is list and ir['skin_nodes'],'Missing skin source nodes')
    tinted={}
    for row in ir['skin_nodes']:
        fields(row,('scene','node'));require(row['scene']in ir['sources']and (safe(row['node'])or row['node']=='.'),'Unreviewed skin source path')
        text=ex.text(row['scene']);require(material(text,row['node'])==ir['shader'],'Skin source is not MenuFlavors material')
        source=texture(text,row['node']);require(source not in tinted,'Duplicate skin texture source');tinted[source]=row
    require(type(ir['skin_manifests'])is dict and len(ir['skin_manifests'])==5,'Missing skin manifest coverage')
    expected={}
    for path,info in ir['skin_manifests'].items():
        fields(info,('sha256','generated_roles'));digest=info['sha256']
        require(type(info['generated_roles'])is list and all(type(v)is str for v in info['generated_roles'])and len(set(info['generated_roles']))==len(info['generated_roles']),'Unknown/duplicate generated manifest role')
        require(safe(path)and path.endswith('/source.json')and type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest),'Unsafe/unknown skin manifest')
        data=(ROOT/'romfs'/path).read_bytes();require(hashlib.sha256(data).hexdigest()==digest,'Skin manifest changed')
        rows=manifest_rows(json.loads(data),info['generated_roles']);expected.update({(path,role):(source,output)for role,(source,output)in rows.items()if source in tinted})
    require(type(ir['skin_bindings'])is list,'Missing skin bindings');actual={}
    for row in ir['skin_bindings']:
        fields(row,('manifest','role','source','path'))
        require(type(row['role'])is str and safe(row['path'])and row['path'].endswith('.t3x'),'Unknown/unsafe skin role/path')
        identity=(row['manifest'],row['role']);require(identity not in actual,'Duplicate skin binding');actual[identity]=(row['source'],row['path'])
    require(actual==expected,'Unknown/missing/wrong skin binding mapping')
    return ir

def load(ex=None,path=IR):return validate(load_json(path),ex)
