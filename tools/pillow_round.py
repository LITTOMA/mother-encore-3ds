#!/usr/bin/env python3
"""Source-bound Pillow actions, ordinary defeat and optional Minnie continuation.

The existing round mechanisms execute the original tackle/float weighted AI.
Schema5 carries a conditional level2 capability; its presence never means a
promotion is automatic, and incoming HP, stats and experience remain live.
"""
import argparse, copy, csv, io, re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT))
import native_round as native
from extract_battle_entry import Extractor, require
from doll_round import digest, read, write, conditional_growth_policy, return_music_policy
from pillow_entry_asset import POST_WIN

IR = ROOT / 'content/pillow-round.json'
PACK = ROOT / 'romfs/data/pillow-entry.encround'
REPORT = ROOT / 'reports/pillow-battle'

def build():
    ex = Extractor(ROOT); base = read(ROOT / 'content/native-round.json'); native.verify_sources(base)
    doll = read(ROOT / 'content/doll-round.json'); native.verify_sources(doll)
    entry = read(ROOT / 'content/pillow-entry.json'); lamp = read(ROOT / 'content/native-battle.json')
    ir = copy.deepcopy(base)
    for reviewed in [base, doll]:
        for path, sha in reviewed['sources'].items(): require(digest(ex.upstream / path) == sha, 'Changed inherited source ' + path); ex.data(path)
    enemy = ex.yaml('Data/Battlers/pillow.yaml'); scene = ex.yaml('Data/Dialogue/Podunk/cutscenes/pillow_attack.yaml')
    require(scene['6']['startbattle'] == {'battlers': [{'pillow': 'pillow'}], 'wincutscene': POST_WIN}, 'Pillow encounter source changed')
    require(enemy['boss'] is False and [s['skill'] for s in enemy['skills']] == ['tackle', 'float'] and not enemy.get('items'), 'Pillow AI/reward path changed')
    require(entry['binding']['stable_id'] == 3 and entry['enemy']['data'] == enemy, 'Pillow entry mismatch')
    trans = {}
    for name in ['battletext', 'battlers', 'battleskills', 'menus']:
        for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/' + name + ' - sheet.csv'))): trans[row['key']] = row['en']
    def text(key, role, values=None, literal=None):
        raw = trans[key] if literal is None else literal; value = raw
        for k, v in (values or {}).items(): value = value.replace('{' + k + '}', str(v))
        require(not re.search(r'\{[^}]+\}', value), 'Unhandled Pillow text ' + value)
        ir['texts'].append(dict(id=len(ir['texts']) + 1, role=role, key=key, source_text=raw, text=value)); return len(ir['texts']) - 1
    def inherit_text(index):
        if not index: return 0
        record = doll['texts'][index]; return text(record['key'], record['role'], literal=record['source_text'], values={}) if record['source_text'] == record['text'] else append_text(record)
    def append_text(record):
        ir['texts'].append(dict(record, id=len(ir['texts']) + 1)); return len(ir['texts']) - 1
    name = lamp['party']['initial_save_data']['name']; ename = trans[enemy['name']]
    articles = trans[enemy['article']].split(','); party_articles = trans['ARTICLES_NINTEN'].split(',')
    contexts = [dict(name=name, n0=party_articles[0], n4=party_articles[4], target=ename, t0=articles[0], t1=articles[1]),
                dict(name=ename, n0=articles[0], n4=articles[4], target=name, t0=party_articles[0], t1=party_articles[1])]
    for i, skill in enumerate(ir['skills']):
        source = ex.yaml(skill['source'])
        if source['dialog']: skill['dialog'] = text(source['dialog'], 3, contexts[0 if i in [0, 3] else 1])
    binding = ir['binding']
    for field, context in [('enemy_outro', contexts[1]), ('mortal_damage', contexts[1]), ('no_effect', contexts[0])]:
        old = base['texts'][binding[field]]; binding[field] = text(old['key'], old['role'], context, literal=old['source_text'])
    binding.update(battle_id=3, enemy_name=text(enemy['name'], 4), enemy_article=text(enemy['article'], 5), win_flag='', show_intro_outro=int(entry['entry']['show_intro_outro']))
    ir['enemy_choices'] = [dict(skill=next(i for i, s in enumerate(ir['skills']) if Path(s['source']).stem == choice['skill']), weight=choice['weight']) for choice in enemy['skills']]
    victory = ir['victory']
    victory.update(initial_exp=base['victory']['reward_exp'], initial_bank=base['victory']['reward_cash'], initial_earned_cash=base['victory']['reward_cash'],
                   reward_exp=enemy['exp'], reward_cash=enemy['cash'], exp_text=text('BATTLE_MSG_EXP_ONE_ALLY', 9, dict(name=name, value=enemy['exp'])))
    from native_content import parse_pack
    room = parse_pack((ROOT / 'romfs/data/opening.encroom').read_bytes())
    body = [b for b in room['sections']['BodyRule'] if room['strings'][b['source_path_string']] == 'Objects/pillow']
    require(len(body) == 1, 'Pillow body binding'); victory['enemy_body_id'] = body[0]['body_id']
    presentation = ir['presentation']; receipt = read(ROOT / 'romfs/pillow-preview/source.json')
    source_resource = next(r for r in receipt['resources'] if r['name'] == 'enemy')
    index = next(m['resource'] for m in presentation['media'] if m['role'] == 2)
    resource = presentation['resources'][index]
    resource.update(path=source_resource['output'], width=source_resource['width'], height=source_resource['height'], columns=1, rows=1, sha256=digest(ROOT / 'romfs' / source_resource['output']))
    for media in presentation['media']:
        if media['role'] == 2: media['rect'][2:] = [resource['width'], resource['height']]
    ex.data('Graphics/Battle Sprites/pillow.png')
    # Same Ninten/equipment source capability; original ordinary defeat media
    # stays intact and no Doll boss flash, shake or callback is copied.
    ir['growth'] = [dict(g, text=inherit_text(g['text'])) for g in doll['growth']]
    ir['boss_shakes'] = []
    ir['encounter'] = dict(doll['encounter'], boss=0, keep_actor=0, post_win_script=POST_WIN, boss_flash_media=0xffffffff,
                           level_text=inherit_text(doll['encounter']['level_text']), learned_text=inherit_text(doll['encounter']['learned_text']),
                           stop_area_music_if_overworld=return_music_policy(enemy, ex.text('Scripts/UI/Battle/BattleSystem.gd')))
    conditional_growth_policy(ex.text('Scripts/global/PartyMember.gd'))
    require(victory['next_level_exp'] == room['sections']['Experience'][1]['required_total_exp'] and ir['encounter']['following_level_exp'] == room['sections']['Experience'][2]['required_total_exp'], 'Conditional progression dependency mismatch')
    ir['schema'] = 5
    ir['scope'] = 'Pinned Pillow tackle/float, ordinary defeat/removal, live session rewards with conditional level2 growth, original minnie_leave post-win boundary; no boss effects or new render path'
    ir['sources'] = ex.sources
    ir['dependencies'] = {path: digest(ROOT / path) for path in ['content/native-battle.json', 'content/native-opening.json', 'content/native-round.json', 'content/pillow-entry.json', 'content/doll-round.json']}
    return ir

def main():
    parser = argparse.ArgumentParser(); parser.add_argument('action', choices=['extract', 'compile', 'verify'], nargs='?', default='compile'); args = parser.parse_args()
    if args.action == 'extract':
        ir = build(); write(IR, ir)
        write(REPORT / 'source-review.json', dict(schema=5, commit=native.PIN, sources=ir['sources'], dependencies=ir['dependencies'], scope=ir['scope'],
              limits='Pinned source inspection and inherited mechanisms; this extraction is not a Godot execution or hardware check. Source reference/action comparison must be recorded separately.'))
    ir = read(IR); native.verify_sources(ir); blob = native.encode(native.lower(ir)); native.parse_pack(blob)
    if args.action == 'verify': require(PACK.read_bytes() == blob, 'Stale Pillow round binary')
    else:
        PACK.parent.mkdir(parents=True, exist_ok=True); PACK.write_bytes(blob)
        write(REPORT / 'round-build.json', dict(schema=5, bytes=len(blob), sha256=digest(PACK), ir_sha256=digest(IR)))
    print('Pillow round:', len(blob), 'bytes; checked ENCRND01 v5')

if __name__ == '__main__':
    try: main()
    except (ValueError, KeyError, OSError, TypeError) as error: print('PILLOW ROUND ERROR:', error, file=sys.stderr); raise SystemExit(1)
