#!/usr/bin/env python3
"""Checked global Ready cursor and explicit native 3DS preferences adaptation.

This resource grants no Ready by itself. The actual source owner must execute
all six steps on the same ObjectDB. Imported encrypted PC settings are pending.
"""
from pathlib import Path
import argparse, hashlib, json, re, struct, sys, zlib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor
from tools.podunk_scene import PIN,read,write,sha,require
from tools.field_global_constructor import load as constructor_load,IR as CTOR_IR
from tools.player_initialization import load as player_load,IR as PLAYER_IR
from tools.global_load import load as cold_load,IR as LOAD_IR
from tools.native_input import encode as input_encode,IR as INPUT_IR
FAMILY=0x454e005d
IR=ROOT/'content/native-global-ready.json'
REVIEW=ROOT/'reports/global-ready/source-review.json'
PACK=ROOT/'romfs/data/global.encready'
SOURCE='Scripts/global/global.gd'

def method(source,name):
    start=re.search(r'^func '+re.escape(name)+r'\([^\n]*\):\n',source,re.M)
    require(start,'Source Ready method absent '+name)
    rest=source[start.end():]
    return rest.split('\nfunc ',1)[0]

def derive():
    ex=Extractor(ROOT);source=ex.text(SOURCE)
    ctor=constructor_load();player=player_load();cold=cold_load()
    steps=[x.strip()for x in method(source,'_ready').splitlines()if x.strip()]
    require(steps==ctor['ready_steps'] and len(steps)==6,'Source Ready order changed')
    require(steps[0]=='_set_localized_default_inputs()' and steps[1]=='_load_settings()'
            and steps[3]=='add_child(scene_transition)' and steps[4]=='_init_player()'
            and steps[5]=='_load_default_save()','Unknown Ready opcode')
    resize=re.fullmatch(r'partySpace\.resize\((\d+)\)',steps[2]);require(resize,'Unknown partySpace resize')
    settings=method(source,'_load_settings')
    prefix=[x.strip()for x in settings.splitlines()if x.strip()][:4]
    require(prefix==['var save_file := File.new()', 'if not save_file.file_exists("user://settings.save"):', '_set_language_default()', 'return'],'Source settings absent-file branch changed')
    require('open_encrypted_with_pass("user://settings.save", File.READ,"ENCORE")'in settings
            and 'set_master_volume(master_volume)'in settings and 'set_sfx_volume(sfx_volume)'in settings,
            'Source settings encrypted branch changed')
    actions=json.loads(re.search(r'var actions := (\[[^\n]+\])',method(source,'_set_localized_default_inputs'))[1])
    require(actions==['ui_accept','ui_cancel','ui_select','ui_focus_prev','ui_focus_next'],'Source keyboard action scope changed')
    constants={x['name']:x['value']for x in ctor['constants']}
    require(constants['LANGUAGES_DISABLED']==[],'Disabled languages need separate source policy')
    require(player['owner']==cold['owner']==SOURCE and player['sources'][SOURCE]==cold['sources'][SOURCE]==ex.sources[SOURCE], 'Source Ready consumers do not share exact owner')
    raw=input_encode(read(INPUT_IR));path=ROOT/'romfs/data/native.encinput'
    require(path.read_bytes()==raw,'Native checked input pack stale')
    methods=['_ready','_set_localized_default_inputs','_load_settings','_set_language_default','_init_player','_load_default_save']
    return dict(schema=1,format=1,capability=1,rules=1,family=FAMILY,commit=PIN,
      owner=SOURCE,scene_id=ctor['scene_id'],source_sha256=ex.sources[SOURCE],sources=ex.sources,
      dependencies=dict(constructor_ir_sha256=sha(CTOR_IR),player_ir_sha256=sha(PLAYER_IR),load_ir_sha256=sha(LOAD_IR),input_ir_sha256=sha(INPUT_IR)),
      methods={name:dict(body_sha256=hashlib.sha256(method(source,name).encode()).hexdigest(),body=method(source,name))for name in methods},
      settings_file=dict(id=int.from_bytes(hashlib.sha256((SOURCE+':_load_settings.File').encode()).digest()[:4],'little'),name='_load_settings.File',native='File'),
      policy=dict(party_space=int(resize[1]),source_settings_path='user://settings.save',native_settings_path='sdmc:/3ds/encore-native/settings.save',locale_preference_path='sdmc:/3ds/encore-native/language.encprefs',slot_preference_path='sdmc:/3ds/encore-native/settings.encprefs',language_default=constants['LANGUAGE_DEFAULT'],languages=constants['LANGUAGES'],source_save_slot_member='save_file',input_path='data/native.encinput',input_sha256=hashlib.sha256(raw).hexdigest(),input_bytes=len(raw),input_crc32=zlib.crc32(raw),steps=[dict(kind=i+1,source=s)for i,s in enumerate(steps)],encrypted_settings_capability=0),
      adaptation=dict(localized_inputs='Physical 3DS HID uses the existing checked NativeInputAdapter; no InputEventKey exists to remap. Configure the actual borrowed adapter from the exact native resource.',locale='Read the actual native persisted locale and apply the existing GPU-safe select_native_locale service, then verify the SAME LocaleSelection owner. Missing native preference retains an already source-supported/native-ready selection; otherwise source LANGUAGE_DEFAULT.',slot='Read actual native slot metadata into the borrowed UI slot owner only; no game snapshot/globalData save-file assignment is inferred.',settings='An existing mapped source settings.save is rejected: encrypted JSON/audio bus/window/InputMap/rumble/button-style complete branch is not migrated. Native two preferences plus SessionSettings are not that branch.'),scene_ready_admitted=False)

def extract():
    d=derive();write(IR,d)
    write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],dependencies=d['dependencies'],scope='Actual six-step global Ready executor, native Node owner, exact missing-encrypted-settings branch with separately persisted 3DS locale/slot adaptation',encrypted_settings_admitted=False,scene_ready_admitted=False))

def load():
    d=read(IR);require(d==derive(),'Global Ready source/dependency/adaptation changed')
    r=read(REVIEW);require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==sha(IR) and r['sources']==d['sources'] and r['dependencies']==d['dependencies'] and r['encrypted_settings_admitted']is False and r['scene_ready_admitted']is False,'Global Ready review changed')
    return d

def encode(d):
    b=bytearray(128)
    def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
    def t(s):raw=s.encode();u(len(raw));b.extend(raw)
    for k in ('constructor_ir_sha256','player_ir_sha256','load_ir_sha256'):b.extend(bytes.fromhex(d['dependencies'][k]))
    t(d['owner']);f=d['settings_file'];u(f['id']);t(f['name']);t(f['native']);u(len(d['sources']))
    for name,h in d['sources'].items():t(name);b.extend(bytes.fromhex(h))
    p=d['policy'];u(p['party_space'],p['encrypted_settings_capability'])
    for k in ('source_settings_path','native_settings_path','locale_preference_path','slot_preference_path','language_default','source_save_slot_member','input_path'):t(p[k])
    b.extend(bytes.fromhex(p['input_sha256']));u(p['input_bytes'],p['input_crc32'])
    u(len(p['languages']));[t(s)for s in p['languages']]
    u(len(p['steps']))
    for row in p['steps']:u(row['kind']);t(row['source'])
    struct.pack_into('<8s8I',b,0,b'ENCGRED1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id'])
    b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR))
    return bytes(b)

def stage_files(source):
    raw=encode(load());path=Path('data/global.encready');require((Path(source)/path).read_bytes()==raw,'Staged global Ready differs');return {path:raw}

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
    if a.action=='extract':extract();return
    raw=encode(load())
    if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
    else:require(PACK.read_bytes()==raw,'Global Ready pack stale')
    print('Source global Ready cursor:',len(raw),'bytes; existing encrypted settings remain rejected')
if __name__=='__main__':
    try:main()
    except(ValueError,KeyError,TypeError,OSError,struct.error)as error:sys.exit('GLOBAL READY ERROR: '+str(error))
