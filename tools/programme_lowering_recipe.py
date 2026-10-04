"""Checked declarative programme lowering, limited to references and emissions.

This does not execute scripts, evaluate expressions or interpret arbitrary YAML
at runtime. Ordered emissions, saved identities and game-specific bindings are
external reviewed data; these functions implement the bounded recipe schema.
"""
import copy,csv,hashlib,io,json,math,re
from contextvars import ContextVar
from functools import wraps
from inspect import signature
from pathlib import Path
from tools.extract_battle_entry import Extractor,require,one

ROOT=Path(__file__).resolve().parents[1]
PATH=Path('content/programme-lowering-recipe.json')
KINDS=set('BeginCutscene BindActor ActorPersistent StartWait MusicFadeOut SetTalker CallObjectDeferred OverworldBattleMusic PlaySound MoveActor TurnActor ShakeActor JumpActor AnimateActor EmoteActor ShakeCamera ChangeCamera MoveCamera QueueBattle StopInteraction RestoreActor ReleaseBattleActor CutsceneEnded DialogueDone RequestBattle YieldIdle AwaitTimer SetActorDirection TeleportActor MoveActorPath ReturnCamera SetFlag ShowDialogue AwaitDialogue PlayMusicImmediate HideDialogue'.split())
COMMAND_FIELDS=set('kind phrase actor vector value duration text detail binding clip flag resource dialogue_id flags path dialogue_key source_label source_object source_method target_actor'.split())

def fields(value,names,label):require(type(value)is dict and set(value)==set(names),'Unknown/missing programme fields: '+label)
def safe(value):
    require(type(value)is str and value and not value.startswith('/')and ':'not in value and '\\'not in value and all(p not in ('','.','..')for p in value.split('/')),'Unsafe programme source path')
    return value
def read(root=ROOT):
    def unique(pairs):
        value={}
        for key,item in pairs:require(key not in value,'Duplicate programme field');value[key]=item
        return value
    def invalid(value):raise ValueError('Nonfinite programme number')
    return json.loads((Path(root)/PATH).read_text(encoding='utf-8'),object_pairs_hook=unique,parse_constant=invalid)
def equal(a,b):
    if type(b)is bool:return type(a)is bool and a==b
    if type(b)in(int,float):return type(a)in(int,float)and math.isfinite(a)and a==b
    if type(b)is dict:return type(a)is dict and set(a)==set(b)and all(equal(a[k],v)for k,v in b.items())
    if type(b)is list:return type(a)is list and len(a)==len(b)and all(equal(x,y)for x,y in zip(a,b))
    return type(a)is type(b)and a==b
def at(value,path):
    require(type(path)is list and path and len(path)<=32,'Invalid programme reference')
    for key in path:
        require(type(key)in(str,int),'Invalid programme reference key')
        if type(value)is dict:require(type(key)is str and key in value,'Missing programme source reference')
        elif type(value)is list:require(type(key)is int and 0<=key<len(value),'Missing programme array reference')
        else:raise ValueError('Programme reference traverses scalar')
        value=value[key]
    return value
def document(value,path,doc):
    require(path in value['documents'],'Unreviewed programme document')
    expected=value['documents'][path]
    require(equal(doc,expected)and list(doc)==list(expected),'Unreviewed programme source document/order')
    for label,phrase in expected.items():
        if type(phrase)is not dict:continue
        for field in ['actors','options']:
            if field in phrase:require(list(doc[label][field])==list(phrase[field]),'Unreviewed programme '+field+' binding order')
    return doc

def resolve(item,doc,value,arguments=None,translations=None):
    if type(item)is dict:
        tags=[k for k in item if k.startswith('$')]
        if tags:
            require(len(item)==1,'Mixed programme reference/literal fields');tag=tags[0];data=item[tag]
            if tag=='$source':return copy.deepcopy(at(doc,data))
            if tag=='$fact':require(data in value['facts'],'Unknown programme fact');return copy.deepcopy(value['facts'][data]['value'])
            if tag=='$identity':require(data in value['identities'],'Unknown programme identity');return copy.deepcopy(value['identities'][data]['value'])
            if tag=='$argument':
                require(data=='end_duration'and arguments is not None and data in arguments,'Unknown programme argument')
                number=arguments[data];require(type(number)in(int,float)and math.isfinite(number)and number>0,'Programme camera return duration');return number
            if tag=='$translation':require(translations is not None and data in translations,'Unknown programme translation');return translations[data]
            if tag=='$concat':
                require(type(data)is list and 1<=len(data)<=16,'Invalid programme concatenation')
                parts=[resolve(p,doc,value,arguments,translations)for p in data]
                require(all(type(p)is str for p in parts),'Non-string programme concatenation');return ''.join(parts)
            raise ValueError('Unknown programme reference operation')
        return {k:resolve(v,doc,value,arguments,translations)for k,v in item.items()}
    if type(item)is list:return [resolve(v,doc,value,arguments,translations)for v in item]
    require(type(item)in(str,int,float,bool,type(None)),'Invalid programme literal')
    if type(item)in(int,float):require(math.isfinite(item),'Nonfinite programme literal')
    return item

def checked(value,root=ROOT):
    fields(value,['schema','kind','commit','sources','documents','facts','identities','programmes','normal_metadata','normal_source','phone_sources'],'root')
    ex=Extractor(root)
    require(type(value['schema'])is int and value['schema']==1 and value['kind']=='encore.programme-lowering.reviewed-recipe'and value['commit']==ex.lock['commit'],'Programme schema/pin rejected')
    require(type(value['sources'])is dict and value['sources'],'Missing programme provenance')
    for source,digest in value['sources'].items():
        safe(source);require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest),'Invalid programme fingerprint')
        require(hashlib.sha256(ex.data(source)).hexdigest()==digest,'Programme source fingerprint mismatch')
    require(type(value['documents'])is dict and value['documents'],'Missing programme source review')
    for source,review in value['documents'].items():
        require(source in value['sources'],'Programme source review lacks provenance');document(value,source,ex.yaml(source))
    require(type(value['facts'])is dict and value['facts'],'Missing programme expression facts')
    for name,fact in value['facts'].items():
        fields(fact,['source','pattern','type','value'],'fact');require(fact['source']in value['sources']and fact['type']in ['number','string','direction'],'Unknown programme fact mechanism')
        require(type(fact['pattern'])is str and len(fact['pattern'])<=4096,'Invalid programme fact selector')
        try:raw=one(fact['pattern'],ex.text(fact['source']),'programme source expression',re.M)['value']
        except (re.error,IndexError)as error:raise ValueError('Invalid programme fact selector')from error
        if fact['type']=='number':require(re.fullmatch('[0-9]+(?:\.[0-9]+)?',raw)and type(fact['value'])in(int,float)and math.isfinite(fact['value'])and float(raw)==fact['value'],'Programme source expression value mismatch')
        elif fact['type']=='string':require(type(fact['value'])is str and raw==fact['value'],'Programme source string expression mismatch')
        else:require(raw in ['RIGHT','ZERO']and fact['value']==([1,0]if raw=='RIGHT'else[0,0]),'Unsupported mathematical direction constant')
    require(type(value['identities'])is dict,'Invalid programme identity map')
    for name,identity in value['identities'].items():
        fields(identity,['source','path','value','category'],'identity');require(identity['source']in value['sources'],'Unreviewed programme identity source')
        source=at(value['documents'][identity['source']]if identity['source']in value['documents']else ex.yaml(identity['source']),identity['path'])
        require(identity['category']in ['actor','text','binding','token'],'Unknown programme identity category')
        if identity['category']=='text':require(type(source)is str and source and type(identity['value'])is int and 1<=identity['value']<=65535,'Invalid programme text identity')
        elif identity['category']=='actor':
            alias=identity['path'][-1];require(type(alias)is str and type(source)is str,'Invalid programme actor source')
            expected=ex.yaml('Data/save_new_game.yaml')['party'][0].title()if alias=='leader'else alias.title()
            require(identity['value']==expected,'Programme actor source alias mismatch')
        elif identity['category']=='token':require(type(source)is dict and identity['path'][-1]in ex.yaml('Data/save_new_game.yaml')['party']and identity['value']=='['+identity['path'][-1].title()+']','Programme player token source mismatch')
        else:require(type(source)is str and source and type(identity['value'])in(str,int)and bool(identity['value']),'Invalid programme binding identity')
    require(type(value['phone_sources'])is list and len(set(value['phone_sources']))==len(value['phone_sources'])and all(p in value['documents']for p in value['phone_sources'])and value['normal_source']in value['phone_sources'],'Invalid phone programme scope')
    require(type(value['programmes'])is dict and value['programmes'],'Missing programme emissions')
    for name,programme in value['programmes'].items():
        require(type(programme)is dict and set(programme)in ({'source','commands'},{'source','commands','result'},{'source','commands','boundary'}),'Unknown programme fields')
        require(programme['source']in value['documents']and type(programme['commands'])is list and 1<=len(programme['commands'])<=4096,'Invalid programme command scope')
        doc=value['documents'][programme['source']]
        for emission in programme['commands']:
            fields(emission,['op','template'],'emission');require(emission['op']=='emit','Unknown programme operation')
            template=emission['template'];require(type(template)is dict and {'kind','phrase','actor'}<=set(template)<=COMMAND_FIELDS and template['kind']in KINDS,'Unknown programme command/schema')
            require(type(template['phrase'])is int and 0<=template['phrase']<len(doc),'Invalid programme source phase')
            command=resolve(template,doc,value,{'end_duration':1})
            require(command['actor']=='None'or type(template['actor'])is dict and '$identity'in template['actor']and value['identities'][template['actor']['$identity']]['category']=='actor','Unreviewed literal programme actor')
            if 'duration'in template:require(type(template['duration'])is dict or template['duration']==0,'Unreviewed literal programme timing')
        if 'boundary'in programme:
            fields(programme['boundary'],['prefix_count','suffix_count'],'boundaries')
            require(all(type(n)is int and 0<=n<len(programme['commands'])for n in programme['boundary'].values())and sum(programme['boundary'].values())<len(programme['commands']),'Invalid programme boundaries')
        if 'result'in programme:
            result=resolve(programme['result'],doc,value,{'end_duration':1})
            fields(result,['identity','source_path','source_labels','actor_bindings','clears_phone_location_on_finish'],'programme result')
            require(result['source_path']==programme['source']and result['identity']==programme['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml')and result['source_labels']==list(doc),'Programme result source identity mismatch')
            require(result['actor_bindings']==[dict(actor=k.title(),source=v)for k,v in doc['0'].get('actors',{}).items()],'Programme result actor binding mismatch')
    normal=value['normal_metadata'];fields(normal,['identity','source_path','execution_status','entry','nodes','clears_phone_location_on_finish','singleton_specialization'],'normal graph')
    doc=value['documents'][value['normal_source']]
    require(normal['source_path']==value['normal_source']and normal['identity']==value['normal_source'].removeprefix('Data/Dialogue/').removesuffix('.yaml')and normal['entry']==next(iter(doc))and type(normal['nodes'])is list and [n['label']for n in normal['nodes']]==list(doc),'Normal graph source identity/order mismatch')
    fields(normal['singleton_specialization'],['required_party','skipped_conditions','policy'],'singleton metadata')
    require(normal['singleton_specialization']['required_party']==ex.yaml('Data/save_new_game.yaml')['party'],'Normal singleton source mismatch')
    allowed=set('label text_key fallback terminal textless_immediate branch evaluate_branch choices cancel_target initial_selection show_choices text_replacement_effects reviewed_noop entry can_input suspend_until hidden_text_does_not_finish'.split())
    for row in normal['nodes']:
        require({'label','text_key','fallback','terminal','textless_immediate'}<=set(row)<=allowed,'Unknown normal node fields')
        phrase=doc[row['label']]
        require(row['text_key']==(value['normal_source']+'::'+row['label']if phrase.get('text')else None)and row['fallback']==phrase.get('goto'),'Normal source text/control-flow mismatch')
        if 'branch'in row:
            condition=phrase['if']
            if 'leader'in condition:expected=dict(kind='LeaderEquals',value=condition['leader'],target=condition['goto'])
            else:
                flag,flagvalue=next(iter(condition['flags'].items()));expected=dict(kind='FlagEquals',flag=flag,value=flagvalue,target=condition['goto'])
            require(equal(row['branch'],expected),'Normal branch source mismatch')
        if 'choices'in row:
            require([(c['translation_key'],c['target'])for c in row['choices']]==[(k,v)for k,v in phrase['options'].items()if k!='cancel']and row['cancel_target']==phrase['options']['cancel'],'Normal choice source mismatch')
            for choice in row['choices']:fields(choice,['translation_key','text_en','target'],'normal choice')
        if 'reviewed_noop'in row:
            fields(row['reviewed_noop'],['source_command','flag','reason','flags_updated','source'],'normal no-op')
            require(row['reviewed_noop']['source_command']=='unsetflags'and row['reviewed_noop']['flag']==phrase['unsetflags'],'Normal no-op source mismatch')
    return value

_operation = ContextVar('programme_source_operation', default=None)

def operation(function):
    """Reuse an admitted recipe only inside one bounded, read-only conversion.

    Admission is lazy so earlier receipt failures retain their original gate.
    Every operation re-admits real source bytes before returning its result;
    neither successful nor failed operations leave a persistent source cache.
    """
    parameters = signature(function)
    @wraps(function)
    def run(*args, **kwargs):
        bound = parameters.bind(*args, **kwargs)
        root = Path(bound.arguments.get('root', parameters.parameters['root'].default)).resolve()
        active = _operation.get()
        if active is not None and active['root'] == root:
            return function(*args, **kwargs)
        state = {'root': root, 'value': None}
        token = _operation.set(state)
        try:
            result = function(*args, **kwargs)
            if state['value'] is not None:
                final = checked(read(root), root)
                require(final == state['value'], 'Programme recipe changed during conversion')
            return result
        finally:
            _operation.reset(token)
    return run

def load(root=ROOT):
    value = read(root)
    active = _operation.get()
    if active is None or active['root'] != Path(root).resolve():
        return checked(value, root)
    if active['value'] is None:
        active['value'] = copy.deepcopy(checked(value, root))
    else:
        require(value == active['value'], 'Programme recipe changed during conversion')
    # Callers cannot mutate the admitted snapshot used by another nested call.
    return copy.deepcopy(active['value'])
def execute(name,doc,end_duration=None,root=ROOT):
    value=load(root);require(name in value['programmes'],'Unknown programme dispatch')
    row=value['programmes'][name];document(value,row['source'],doc)
    args={'end_duration':end_duration}if end_duration is not None else None
    return [resolve(e['template'],doc,value,args)for e in row['commands']]
def lamp_phrase(phrase,index,root=ROOT):
    value=load(root);row=value['programmes']['lamp'];doc=copy.deepcopy(value['documents'][row['source']])
    require(type(index)is int and 0<=index<len(doc),'Invalid lamp phrase index')
    label=list(doc)[index];doc[label]=phrase;document(value,row['source'],doc)
    boundary=row['boundary'];end=len(row['commands'])-boundary['suffix_count']
    return [resolve(e['template'],doc,value)for e in row['commands'][boundary['prefix_count']:end]if e['template']['phrase']==index]
def for_source(path,doc,end_duration,root=ROOT):
    value=load(root);rows=[(name,row)for name,row in value['programmes'].items()if row['source']==path and 'result'in row]
    require(len(rows)==1,'Unreviewed linear phone programme');name,row=rows[0];document(value,path,doc)
    result=resolve(row['result'],doc,value,{'end_duration':end_duration})
    result['commands']=[resolve(e['template'],doc,value,{'end_duration':end_duration})for e in row['commands']]
    # Original JSON field order is stable for byte-level extraction checks.
    return dict(identity=result['identity'],source_path=result['source_path'],source_labels=result['source_labels'],actor_bindings=result['actor_bindings'],commands=result['commands'],clears_phone_location_on_finish=result['clears_phone_location_on_finish'])
def phone_documents(root=ROOT):
    value=load(root);return {p:copy.deepcopy(value['documents'][p])for p in value['phone_sources']}
def normal_metadata(doc,translations,root=ROOT):
    value=load(root);document(value,value['normal_source'],doc)
    # Translation values supplied by the frontend must match actual CSV source
    # rows, so a changed caller dictionary cannot create unreviewed content.
    actual={};ex=Extractor(root)
    for source in value['sources']:
        if source.endswith('.csv'):
            for row in csv.DictReader(io.StringIO(ex.text(source))):actual[row['key']]=row['en']
    for row in value['normal_metadata']['nodes']:
        for choice in row.get('choices',[]):require(choice['translation_key']in translations and translations[choice['translation_key']]==actual[choice['translation_key']],'Programme source translation mismatch')
    return resolve(value['normal_metadata'],doc,value,translations=translations)
def verify_room(ir,root=ROOT):
    """Ordinary reviewed compilation gate, without compiler recursion.

    Provenance changes when adapters/data are reviewed. Actual game identities,
    commands and values must match a fresh source extraction before packing.
    """
    from tools.extract_native_content import Extractor as RoomExtractor
    expected,_=RoomExtractor(root).run()
    require(ir['strings']==expected['strings']and ir['sections']==expected['sections'],'Stale programme recipe/Room content')
