#!/usr/bin/env python3
"""Extract the pinned lamp battle-entry slice as external source-derived JSON.

Offline adapter only. It does not grant broad compatibility to BattleSystem,
run combat, read C++ constants, or modify the upstream checkout. The caller's
binary compiler is responsible for validating the deliberately bounded schema.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
PIN = '7d9246600fffe518408f5830d4848635019005a3'
BATTLE = 'Nodes/Ui/Battle/Battle.tscn'
SYSTEM = 'Scripts/UI/Battle/BattleSystem.gd'
TRANSITION = 'Nodes/Ui/Battle/Battle Transition.tscn'
ENEMY_SPRITE = 'Nodes/Ui/Battle/EnemySprite.tscn'
PARTY_SPRITE = 'Nodes/Ui/Battle/BattleSpriteNinten.tscn'
PARTY_PLATE = 'Nodes/Ui/Battle/PartyInfoPlate.tscn'


def require(ok, message):
    if not ok: raise ValueError(message)


def one(pattern, text, label, flags=re.M):
    result = list(re.finditer(pattern, text, flags))
    require(len(result) == 1, 'Missing/ambiguous source selector: ' + label)
    return result[0]


def variant(raw):
    """Bounded textual Variant reader for the inspected entry resources only."""
    raw = raw.strip()
    raw = re.sub(r'NodePath\(\s*("(?:[^"\\]|\\.)*")\s*\)', r'\1', raw)
    raw = re.sub(r'(?:Vector2|Color|PoolRealArray|PoolIntArray|PoolStringArray)\(\s*([^()]*)\s*\)', r'[\1]', raw)
    raw = re.sub(r'(ExtResource|SubResource)\(\s*(\d+)\s*\)', lambda m: json.dumps({m[1]: int(m[2])}), raw)
    try: return json.loads(raw, parse_constant=lambda value: (_ for _ in ()).throw(ValueError("Nonfinite Variant: " + value)))
    except json.JSONDecodeError as error: raise ValueError('Unreviewed Variant: ' + raw[:180]) from error


def properties(body):
    matches = list(re.finditer(r'^([A-Za-z_][A-Za-z0-9_/]*)\s*=\s*', body, re.M))
    result = {}
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(body)
        require(match[1] not in result, 'Duplicate property: ' + match[1])
        result[match[1]] = variant(body[match.end():end])
    return result


def node(text, path):
    found = []
    for match in re.finditer(r'^\[node ([^\n]+)\]\n(.*?)(?=^\[|\Z)', text, re.M | re.S):
        attrs = dict(re.findall(r'(\w+)="([^"]*)"', match[1]))
        name, parent = attrs.get('name'), attrs.get('parent')
        actual = '.' if parent is None else name if parent == '.' else parent + '/' + name
        if actual == path: found.append(properties(match[2]))
    require(len(found) == 1, 'Missing/ambiguous node: ' + path)
    return found[0]


def animation(text, resource_id, source, name):
    match = one(r'^\[sub_resource type="Animation" id=' + str(resource_id) + r'\]\n(.*?)(?=^\[|\Z)', text, source + ':' + name, re.M | re.S)
    props = properties(match[1])
    tracks = {}
    for key, value in props.items():
        if key.startswith('tracks/'):
            _, index, field = key.split('/')
            tracks.setdefault(int(index), {})[field] = value
    require(set(tracks) == set(range(len(tracks))), 'Noncontiguous animation tracks')
    for track in tracks.values():
        require(track['type'] in ('value', 'method'), 'Unreviewed animation track')
        require(track['enabled'] and not track['imported'], 'Unexpected disabled/imported track')
        keys = track['keys']
        require(len(keys['times']) == len(keys['transitions']) == len(keys['values']), 'Animation key mismatch')
    return dict(name=name, source=source, source_resource_id=resource_id,
                length=props.get('length', 1.0), loop=props.get('loop', False),
                step=props.get('step', 0.1), tracks=[tracks[i] for i in sorted(tracks)])


class Extractor:
    def __init__(self, root=ROOT):
        self.root = Path(root)
        self.upstream = self.root / 'upstream/MOTHER-Encore'
        self.inventory = json.loads((self.root / 'compatibility/upstream-inventory.json').read_text())
        self.lock = json.loads((self.root / 'upstream.lock').read_text())
        require(self.lock['commit'] == self.inventory['commit'] == PIN, 'Unreviewed source pin')
        head = subprocess.check_output(['git', '-C', str(self.upstream), 'rev-parse', 'HEAD'], text=True).strip()
        require(head == PIN, 'Upstream HEAD mismatch')
        require(not subprocess.check_output(['git', '-C', str(self.upstream), 'status', '--porcelain'], text=True).strip(), 'Upstream must stay pristine')
        self.sources = {}

    def data(self, path):
        require(path in self.inventory['files'], 'Uninventoried source: ' + path)
        raw = (self.upstream / path).read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        require(digest == self.inventory['files'][path]['sha256'], 'Changed pinned source: ' + path)
        self.sources[path] = digest
        return raw

    def text(self, path): return self.data(path).decode('utf-8')
    def yaml(self, path): return yaml.safe_load(self.text(path))
    def png_size(self, path):
        raw = self.data(path)
        require(raw[:8] == b'\x89PNG\r\n\x1a\n', 'Expected source PNG')
        return list(struct.unpack('>II', raw[16:24]))

    def build(self):
        from tools.battle_entry_bindings import load as load_bindings,IR as BINDINGS_IR,binding_digest
        recipe,projected=load_bindings(self)
        battle = self.text(BATTLE)
        system = self.text(SYSTEM)
        ui = self.text('Scripts/global/uiManager.gd')
        pm = self.text('Scripts/global/PartyMember.gd')
        cursor = self.text('Scripts/UI/cursor.gd')
        new_game = self.yaml('Data/save_new_game.yaml')
        overrides = self.yaml('Data/save_overrides.yaml')
        enemy = self.yaml('Data/Battlers/lamp.yaml')
        cap = self.yaml('Data/Items/BaseballCap.yaml')
        basic = self.yaml('Data/BattleSkills/attack.yaml')
        strike = self.yaml('Data/BattleSkills/strike.yaml')
        split = self.yaml('Data/BattleSkills/splitShot.yaml')
        action_menu = self.text('Nodes/Ui/Battle/ActionMenuBox.gd')
        for path in ['Scripts/global/Character.gd', 'Scripts/global/Enemy.gd', 'Scripts/global/global.gd',
                     'Scripts/global/globalData.gd', 'Scripts/global/Inventory.gd', 'Scripts/global/Item.gd',
                     'Scripts/UI/Battle/BattleParticipant.gd', 'Scripts/UI/Battle/BattleMenuBox.gd',
                     'Scripts/UI/Battle/BattleSpriteParty.gd', 'Scripts/UI/Battle/EnemySpritesManager.gd',
                     'Scripts/UI/Battle/EnemySprite.gd', 'Scripts/UI/Battle/PartyInfoPlate.gd',
                     'Scripts/UI/Battle/BattleItemPool.gd', 'Scripts/UI/Battle/OnScreenEnemy.gd',
                     'Scripts/UI/Battle/EnemySkill.gd', 'Scripts/UI/BattleTransition.gd',
                     'Scripts/UI/DialogueBox.gd', 'Scripts/Main/actor.gd', 'Scripts/global/audioManager.gd',
                     'addons/distortionator_integration/scene_importer.gd',
                     'addons/distortionator_integration/default_shader.tres', 'Shaders/Transition.shader',
                     'Shaders/MenuFlavors.tres', 'Scripts/UI/colorRectFlavor.gd',
                     'Fonts/EBMain.tres', 'Fonts/EBMain_la.tres', 'Fonts/EBMain.ttf']:
            self.data(path)
        ninten = dict(new_game['ninten'])
        ninten['name'] = overrides['ninten']['name']
        ninten['affinity_multipliers'] = overrides['ninten']['affinity_multipliers']
        for skill in overrides['ninten']['learnedSkills']:
            if skill not in ninten['learnedSkills']: ninten['learnedSkills'].append(skill)
        stat_body = one(r'const PLAYER_STAT_TARGET_TABLE := \{\s*NINTEN: \{(.*?)\n\t\},', pm, 'Ninten stat table', re.S)[1]
        targets = {}
        for name, nums in re.findall(r'\b([A-Z]+):\s*(\[[^\]]+\])', stat_body): targets[name.lower()] = json.loads(nums)
        require(set(targets) == {'maxhp','maxpp','offense','defense','speed','iq','guts'}, 'Unknown stat table')
        level = ninten['level']
        require(level == 1 and ninten['exp'] == 0, 'Initial-level algorithm scope changed')
        base = {key: int(values[level // 10] + (values[level // 10 + 1]-values[level // 10])*(level % 10)/10.0) for key, values in targets.items()}
        effective = {key: value + cap['boost'].get(key, 0) for key, value in base.items()}
        effective.update(hp=min(ninten['hp'],effective['maxhp']), pp=min(ninten['pp'],effective['maxpp']))
        require(ninten['inventory'] == [{'item_name':'BaseballCap','equipped':True}], 'Unreviewed starting equipment')
        require(strike['required_weapon'] == 'bat' and split['required_weapon'] == 'slingshot', 'Changed skill requirements')
        require(enemy['boss'] is False and enemy['music'] == '' and enemy['bg'] == 'lamp' and 'battlescript' not in enemy, 'Entry requires additional behavior')
        # All values below are source adapter data, not C++ program constants.
        transitions=projected['animations']
        require([clip['length'] for clip in transitions[:3]] == [1.3,1.9,0.65], 'Changed entry duration')
        method_track = [track for track in transitions[1]['tracks'] if track['type']=='method']
        require(len(method_track)==1 and method_track[0]['keys']['times']==[0,0.15,1.2,1.25,1.35], 'Changed entry event schedule')
        mask = node(self.text(TRANSITION),'Sprite')
        all_layout = {}
        for path in recipe['scene_layout']: all_layout[path] = node(battle,path)
        plate = self.text(PARTY_PLATE)
        plate_nodes = {path:node(plate,path)for path in recipe['plate_layout']}
        translations={}
        for path in ['Translations/TranslatedText/battletext - sheet.csv','Translations/TranslatedText/battleskills - sheet.csv','Translations/TranslatedText/battlers - sheet.csv']:
            for row in csv.reader(io.StringIO(self.text(path))):
                if row and row[0] in ['ATTACK_NAME','BATTLE_ACTION_ITEMS','BATTLE_ACTION_DEFEND','LAMP_NAME','LAMP_DESC','LAMP_ART']:
                    translations[row[0]]=row[1]
        # The BBG file is ConfigFile data; enumerate all layer fields and reject extra sections.
        bg_text=self.text('Graphics/Battle BGS/lamp.bbg')
        bg_layers=[]
        for match in re.finditer(r'^\[Layer (\d+)\]\s*\n(.*?)(?=^\[|\Z)',bg_text,re.M|re.S):
            bg_layers.append({'index':int(match[1]),'properties':properties(match[2])})
        require(len(bg_layers)==2 and [x['index'] for x in bg_layers]==[0,1],'Unknown BBG layer count')
        require(all(layer['properties'].get('shader')=='[DEFAULT]' and layer['properties'].get('texture_stretch')=='STRETCH_TILE' and layer['properties'].get('barrel') is True for layer in bg_layers),'Unknown BBG shader/profile')
        flavor_raw=one(r'var menuFlavors := \[\s*(\[[^\n]+?\]),\s*# Plain',ui,'Plain menu palette',re.S)[1]
        flavor=json.loads(flavor_raw)
        enemy_size=self.png_size('Graphics/Battle Sprites/lamp.png')
        viewport_text=self.text('project.godot')
        viewport=[int(one(r'^window/size/'+name+r'=(\d+)$',viewport_text,'viewport '+name)[1]) for name in ['width','height']]
        neutral_alpha=int(one(r'var transparency := (\d+)',ui,'neutral tint alpha')[1])
        encounter='Audio/Music/'+one(r'"encounter": "([^"]+)"',system,'encounter audio')[1]
        self.data(encounter);self.data(encounter+'.import')
        self.data('Audio/Music/Poltergeist.ogg');self.data('Audio/Music/Poltergeist.ogg.import')
        assets={}
        for name,path,grid in [('transition','Graphics/VFX/battle_transition.png',[mask['hframes'],mask['vframes']]),
                               ('enemy','Graphics/Battle Sprites/lamp.png',[1,1]),
                               ('party','Graphics/Character Sprites/Ninten/battle.png',[10,18]),
                               ('background','Graphics/Battle BGS/lamp.png',[1,1]),
                               ('digits','Graphics/UI/Battle/hp_pp_numbers.png',[8,10])]:
            size=self.png_size(path)
            self.data(path+'.import')
            assets[name]={'source':path,'texture_size':size,'grid':grid,'frame_size':[size[0]//grid[0],size[1]//grid[1]]}
        for key,path in [('bash_icon','Graphics/UI/Battle/bashIcon.png'),('items_icon','Graphics/UI/Battle/itemIcon.png'),('defend_icon','Graphics/UI/Battle/defendIcon.png'),
                         ('box','Graphics/UI/Overworld/flavours/defaultbox.png'),('plate_frame','Graphics/UI/Battle/info_plate_frame.png'),
                         ('plate_bg','Graphics/UI/Battle/info_plate_bg_battle.png'),('counter_bg','Graphics/UI/Battle/info_plate_counter_bg.png'),
                         ('hp_label','Graphics/UI/Battle/label_hp.png'),('pp_label','Graphics/UI/Battle/label_pp.png')]:
            assets[key]={'source':path,'texture_size':self.png_size(path)}
            self.data(path+'.import')
        result={
            'schema':1,'kind':'encore.native-battle-entry.source-ir','commit':PIN,'game_version':self.lock['game_version'],
            'scope':{'battle_id':'lamp','completion':'first command menu; no selected action execution',
                     'whole_battle_approved':False,'combat_implemented':False,'gpu_reference_verified':False},
            'viewport':{'width':viewport[0],'height':viewport[1]},
            'entry':{'advantage':0,'can_run':False,'win_flag':'poltergeist','encountered_key':'lamp','first_encounter_flag':'lamp_fought',
                     'show_intro_outro':enemy.get('show_intro_outro',False),'post_battle_cutscenes':{},
                     'time_domain':'source animation key times; native deferred callbacks and timer completion may occur on later frames','scene_duration':transitions[1]['length'],'overlay_duration':transitions[0]['length'],
                     'action_reveal_time':1.2,'action_reveal_duration':transitions[2]['length'],'enemy_reveal_time':1.25,'enemy_transition_cleanup_time':1.35,
                     'command_menu_activation_time':1.9,'party_fully_shown_time':2.02,
                     'neutral_color_rgba8':[0,0,255,neutral_alpha],'mask_grid':[mask['hframes'],mask['vframes']],
                     'await_camera_stopped_shaking':True,'overlay_add_deferred_after_idle_frame':True,
                     'backbuffer':{'mode':'COPY_MODE_RECT','z_index':-128,'live_each_frame':True,'captured_canvas':'battle background layer -1 before canvas 0 world'},
                     'area_backdrop_z_index':-100,'overlay_z_index':2,'battle_canvas_layer':2,'background_initial_layer':-1,'background_final_layer':1,
                     'blend_mode':'blend_disabled','mask_white':'replace with neutral_color','mask_other_opaque':'copy SCREEN_TEXTURE','mask_transparent':'write source RGBA with blending disabled'},
            'party':{'id':'ninten','initial_save_data':ninten,'nickname_default_translation_key':'NAME_NINTEN0','stat_targets':targets,
                     'base_stats':base,'effective_stats':effective,'equipment':[{'id':'BaseballCap','equipped':True,'boosts':cap['boost'],'slot':cap['slot']}],
                     'level':level,'exp':ninten['exp'],'learned_skills':ninten['learnedSkills'],'usable_skills':[],
                     'unusable_skills':[{'id':'strike','required_weapon':strike['required_weapon']},{'id':'splitShot','required_weapon':split['required_weapon']}],
                     'basic_skill_id':'attack','basic_skill':basic,'statuses':ninten['status'],'sp_meter_visible':False,'encore_enabled':False},
            'enemy':{'id':'lamp','data':enemy,'defaults':{'show_intro_outro':False,'musicintro':'','passive_skills':[],'affinity_multipliers':{},'items':[]},
                     'homonym_index':-1,'pool_exp':enemy['exp'],'pool_cash':enemy['cash'],'item_drop':None,
                     'sprite_size':enemy_size,'sprite_center':[viewport[0]/2,147/2],'sprite_position':[viewport[0]/2-enemy_size[0]/2,147/2-enemy_size[1]/2],
                     'initial_hidden':True,'initial_appear_scale':[0.3,0.2],'initial_flash_modifier':1.0,'final_scale':[1,1]},
            'animations':transitions,
            'presentation':{'asset_recipe_path':'content/battle-assets.json','asset_receipt_path':'content/asset-receipts/graphics/battle/lamp/source.json','assets':assets,'scene_nodes':all_layout,'plate_nodes':plate_nodes,'plain_palette_hex':flavor,'translations_en':translations,
                            **{key:projected[key]for key in ('derived_layout','party_sprite','party_transition','enemy_transition')}},
            'menu':projected['menu'],
            'audio':{'encounter_audio_id':1001,'encounter_source':encounter,'encounter_loop':False,
                     'encounter_trigger':'BattleSystem._ready','overworld_battle_music':True,'pause_overworld_music':False,
                     'battle_music_source':None,'continue_overworld_source':'Audio/Music/Poltergeist.ogg','overworld_loop_offset_seconds':6.382},
            'background':{'source':'Graphics/Battle BGS/lamp.bbg','shader_source':'addons/distortionator_integration/default_shader.tres',
                          'layers':bg_layers,'texture_size':assets['background']['texture_size'],'native_layer_size':[504,180],
                          'viewport_clip_size':viewport,'uv_domain':'TextureRect extent 504x180, clipped by320x180 viewport; STRETCH_TILE',
                          'shader_clock':'Godot TIME is engine-relative, not restarted by battle entry',
                          'gpu_reference_verified':False},
            'selectors':{'entry':'uiManager.start_battle; DialogueBox._end_dialogue queued start; BattleSystem.init_battle_params/_ready/_battle_start',
                         'stats':'save_new_game plus save_overrides; PartyMember.init_from_dict/_get_stat_for_level/get_base_stat; Character.set_stat',
                         'menu':'ActionMenuBox.set_actions_for_user/_reset_actions/_move; BattleSystem._set_active_member/_add_actions_to_menu; cursor.gd',
                         'layout':'Battle.tscn selected nodes; PartyInfoPlate.tscn; BattleParticipant.add_info_plate; EnemySpritesManager._get_new_position',
                         'background':'lamp.bbg all fields; scene_importer.gd; default_shader.tres'},
            'sources':self.sources,
        }
        result['entry_bindings']={'path':BINDINGS_IR,'sha256':binding_digest()}
        return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--out',type=Path,default=ROOT/'content/native-battle.json')
    args=parser.parse_args()
    try:
        result=Extractor(args.root).build()
        args.out.parent.mkdir(parents=True,exist_ok=True)
        args.out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print('Extracted pinned lamp entry IR; combat and full-source-scene GPU equivalence remain unapproved.')
        return 0
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        print('BATTLE ENTRY EXTRACTION ERROR: '+str(error),file=sys.stderr)
        return 1


if __name__=='__main__': raise SystemExit(main())
