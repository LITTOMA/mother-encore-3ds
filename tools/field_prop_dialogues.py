"""Audited Root investigation programmes and their checked source owner.

These reusable messages have no actors, voice, item or flag side effects.
Publishing them additionally requires an admitted source ray and prop owner;
an NPC interaction or an arbitrary boundary is not a substitute.
"""
import argparse, copy, csv, hashlib, io, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, require
from tools.programme_lowering_recipe import document
from tools import native_content as nc

RECIPE = ROOT / 'content/field-prop-dialogues.json'


def read(path=RECIPE):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate prop dialogue review field')
            result[key] = value
        return result
    def invalid(value):
        raise ValueError('Nonfinite prop dialogue review value')
    p = json.loads(Path(path).read_text(encoding='utf8'), object_pairs_hook=unique,
                   parse_constant=invalid)
    require(type(p) is dict and set(p) == set('schema kind commit scene_id flag branches documents sources locales scope base_fixture base_sha256 story_fixture story_sha256'.split()), 'Prop dialogue review fields')
    require(type(p['schema']) is int and p['schema'] == 1 and p['kind'] == 'encore.field-prop-dialogues.reviewed-source' and p['commit'] == PIN, 'Prop dialogue review schema/pin')
    require(type(p['scene_id']) is int and 0 < p['scene_id'] < 65536 and type(p['flag']) is str and re.fullmatch('[a-z_]+', p['flag']), 'Prop dialogue scene/flag')
    require(p['locales'] == ['en', 'zh_CN'] and type(p['scope']) is str and 0 < len(p['scope']) <= 2048, 'Prop dialogue locales/scope')
    require(type(p['base_fixture']) is str and p['base_fixture'].startswith('tests/fixtures/') and p['base_fixture'].endswith('.encroom') and re.fullmatch(r'[A-Za-z0-9_./-]+', p['base_fixture']) and '..' not in p['base_fixture'] and type(p['base_sha256']) is str and re.fullmatch('[0-9a-f]{64}', p['base_sha256']), 'Prop dialogue frozen input identity')
    require(hashlib.sha256((ROOT / p['base_fixture']).read_bytes()).hexdigest() == p['base_sha256'], 'Prop dialogue frozen input changed')
    require(type(p['story_fixture']) is str and p['story_fixture'].startswith('tests/fixtures/') and p['story_fixture'].endswith('.encstory') and re.fullmatch(r'[A-Za-z0-9_./-]+', p['story_fixture']) and '..' not in p['story_fixture'] and type(p['story_sha256']) is str and re.fullmatch('[0-9a-f]{64}', p['story_sha256']), 'Prop dialogue frozen story identity')
    require(hashlib.sha256((ROOT / p['story_fixture']).read_bytes()).hexdigest() == p['story_sha256'], 'Prop dialogue frozen story changed')
    require(type(p['sources']) is dict and p['sources'] and all(type(name) is str and re.fullmatch(r'[A-Za-z0-9 _./-]+', name) and '..' not in name and not name.startswith('/') and type(digest) is str and re.fullmatch('[0-9a-f]{64}', digest) for name, digest in p['sources'].items()), 'Prop dialogue source identities')
    require(type(p['branches']) is list and len(p['branches']) == 2 and type(p['documents']) is dict, 'Prop dialogue branch coverage')
    ex = Extractor(ROOT)
    for name, digest in p['sources'].items():
        require(hashlib.sha256(ex.data(name)).hexdigest() == digest, 'Prop dialogue reviewed source changed: ' + name)
    script = ex.text('Scripts/Main/Dead Bush.gd')
    branch = re.search(r'func interact\(\):\s+if visible:\s+if globaldata.flags\["([a-z_]+)"\]:\s+uiManager.open_dialogue_box\("([A-Za-z0-9_/]+)"\)\s+else: uiManager.open_dialogue_box\("([A-Za-z0-9_/]+)"\)', script)
    require(branch is not None and p['flag'] == branch[1], 'Prop source visible/flag branch changed')
    ids = set()
    for i, row in enumerate(p['branches']):
        require(type(row) is dict and set(row) == {'expected', 'source', 'dialogue_id'} and type(row['expected']) is bool and row['expected'] == bool(i), 'Prop dialogue branch order/type')
        require(row['source'] == 'Data/Dialogue/' + branch[2 if i else 3] + '.yaml' and row['source'] in p['sources'], 'Prop dialogue source selection changed')
        require(type(row['dialogue_id']) is int and 0 < row['dialogue_id'] < 1000000 and row['dialogue_id'] not in ids, 'Prop dialogue identity')
        ids.add(row['dialogue_id'])
        source = document(p, row['source'], ex.yaml(row['source']))
        require(list(source) == ['0'] and type(source['0']) is dict and set(source['0']) == {'text'} and type(source['0']['text']) is str, 'Unmapped reusable prop dialogue mechanism')
    require(set(p['documents']) == {r['source'] for r in p['branches']}, 'Prop dialogue document coverage')
    ui = ex.text('Scripts/global/uiManager.gd')
    opening = ui[ui.index('func open_dialogue_box(dialogue_id:'):ui.index('func close_dialogue_box():')]
    require('var was_paused := global.get_player().is_paused()' in opening and 'global.get_player().pause(true, true)' in opening and 'yield(_dialogue_box, "ready")' in opening and 'yield(_dialogue_box, "done")' in opening and 'if !was_paused:\n\t\tglobal.get_player().unpause()' in opening and 'toggle_black_bars(true)' in opening and 'toggle_black_bars(false)' in opening, 'Prop dialogue pause/ready/completion owner changed')
    box = ex.text('Scripts/UI/DialogueBox.gd')
    voice = box[box.index('if _curr_phrase.get("sound", null):'):box.index('# soundeffect:')]
    require('$AudioStreamPlayer.stream = null' in voice, 'Prop missing sound no longer means no text voice')
    abstract = ex.text('Scripts/UI/AbstractDialogueBox.gd')
    require('elif btn_next:\n\t\tif _finished:\n\t\t\t_next_phrase()' in abstract and 'else:\n\t\t_end_dialogue()' in abstract, 'Prop manual text/end contract changed')
    player = ex.text('Scripts/Main/party/Player.gd')
    require('if Input.is_action_just_pressed("ui_accept") and !_paused and can_interact and !global.entering_door and eventRayCaster.is_colliding():' in player and 'if !uiManager.is_stack_empty():' in player and 'global.party_call("try_to_turn",c)' in player, 'Prop source player/UI interaction gate changed')
    party = ex.text('Scripts/Main/party/party_object.gd')
    require('if !target or !target.get("player_turn"):' in party, 'Prop source unconstrained interaction turn changed')
    ex.data('Translations/TranslatedText/dialogue_Reusable - sheet.csv')
    require(set(ex.sources) <= set(p['sources']), 'Prop review omits an audited dialogue dependency')
    return p, ex


def texts(p, ex):
    from tools.graveyard_story import checked_text
    table = {}
    for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Reusable - sheet.csv'))):
        require(row['key'] not in table, 'Duplicate reusable dialogue translation')
        table[row['key']] = row
    result = []
    for row in p['branches']:
        key = p['documents'][row['source']]['0']['text']
        require(key in table, 'Prop dialogue translation key missing')
        for locale in p['locales']:
            require(bool(table[key].get(locale)), 'Prop dialogue translation missing')
            result.append(dict(id=row['dialogue_id'], locale='zh_Hans_CN' if locale == 'zh_CN' else locale,
                               speaker='', body=checked_text(table[key][locale].replace('\\n', '\n')), voice=''))
    return result


def interaction_policy(path=RECIPE):
    """Admit this field's source ray against the actual shared collision set."""
    import math, zlib
    from tools import field_props, podunk_field as f
    from tools.extract_battle_entry import node
    p, ex = read(path)
    recipe = f.load_recipe()
    model = f.Model(recipe)
    ray = node(ex.text(recipe['player']['scene']), 'EventDetector')
    require(set(ray) <= {'position', 'scale', 'enabled', 'cast_to', 'collide_with_areas', 'collide_with_bodies', 'collision_mask'}, 'Unmapped prop investigation ray override')
    require(ray.get('enabled', False) is True and ray.get('collide_with_areas', False) is True and ray.get('collide_with_bodies', True) is True and ray['cast_to'][0] == 0 and ray['cast_to'][1] > 0 and ray['scale'][1] == 1, 'Unsupported prop source investigation ray')
    origin = ray['position'];length = ray['cast_to'][1];mask = ray.get('collision_mask', 1)
    require(type(mask) is int and 0 < mask < 2**20 and all(type(v) in (int, float) and math.isfinite(v) for v in [*origin, length]), 'Invalid prop source ray geometry/filter')
    # ENCMAP stores the walking collision set. A source filter difference
    # must require new data, never silently treat walking as ray collision.
    for source in model.scene.nodes:
        if source.type not in ('TileMap', 'StaticBody2D', 'KinematicBody2D'):
            continue
        if source.type == 'StaticBody2D' and model.scene.category[source.path] not in ('static_body', 'prop', 'openable_door', 'field_object'):
            continue
        if source.type == 'KinematicBody2D' and model.scene.category[source.path] != 'npc':
            continue
        layer, source_mask = source.props.get('collision_layer', 1), source.props.get('collision_mask', 1)
        require(model.collides(layer, source_mask) == bool(layer & mask), 'Prop ray/walking collider set differs: ' + source.path)
    prop_review = field_props.read()
    filters = []
    for row in prop_review['instances']:
        area = model.scene.by_path[row['path'] + '/interact']
        require(area.type == 'Area2D' and set(area.props) <= {'monitoring', 'monitorable', 'collision_layer', 'collision_mask', 'position'}, 'Unmapped prop source interaction area')
        require(area.props.get('position', f.Vec2(0, 0)) == f.Vec2(0, 0) and area.props.get('monitoring', True) is False and area.props.get('monitorable', True) is True, 'Prop interaction area owner/filter changed')
        layer = area.props.get('collision_layer', 1)
        require(type(layer) is int and 0 < layer < 2**20 and layer & mask, 'Prop area is absent from source ray mask')
        filters.append(layer)
    require(len(set(filters)) == 1, 'Prop source area filters differ')
    data, clips, _ = field_props.source()
    raw = field_props.encode(data, clips, capability=3)
    require(raw == (ROOT / 'romfs' / prop_review['output']).read_bytes(), 'Prop dialogue action companion is stale')
    require(data['policy']['scene'] == p['scene_id'] and data['flags'][data['policy']['bat_flag']] == p['flag'] and [data['policy']['hard'], data['policy']['easy']] == [r['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml') for r in p['branches']], 'Prop source reusable selection differs')
    return dict(flag=p['flag'], ray_origin=origin, ray_length=length, ray_mask=mask, stop_running=True,
                area_layer=filters[0], area_flags=2, props_bytes=len(raw), props_crc=zlib.crc32(raw))


def add_room(path, scene, sections, strings, S):
    from tools.doll_postwin import return_duration
    p, ex = read(path)
    require(scene == p['scene_id'], 'Prop dialogue Room scene differs')
    flag_names = [strings[r['name_string']] for r in sections['Flag']]
    require(flag_names.count(p['flag']) == 1, 'Prop dialogue flag missing/ambiguous')
    existing_ids = {r['target_index'] for r in sections['Command'] if r['opcode'] == nc.OPCODES.index('ShowDialogue')}
    require(existing_ids.isdisjoint(r['dialogue_id'] for r in p['branches']), 'Prop dialogue identity overlaps existing programme')
    existing_sources = {strings[r['source_path_string']] for r in sections['Program']}
    duration = return_duration(ex.text('Scripts/UI/DialogueBox.gd'))
    result = []
    for row in p['branches']:
        source = row['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml')
        require(source not in existing_sources, 'Prop reusable programme already present')
        first = len(sections['Command'])
        for kind, target, flags, seconds in [('BeginCutscene', nc.NONE, 0, 0), ('ShowDialogue', row['dialogue_id'], 1, 0), ('AwaitDialogue', nc.NONE, 0, 0), ('HideDialogue', nc.NONE, 1, 0), ('SetTalker', nc.NONE, 0, 0), ('CutsceneEnded', nc.NONE, 0, 0), ('DialogueDone', nc.NONE, 0, duration)]:
            sections['Command'].append(dict(opcode=nc.OPCODES.index(kind), actor_index=nc.NO_ACTOR, phrase=0,
                                            target_index=target, auxiliary_index=nc.NONE, flags=flags,
                                            vector=[0, 0], value=0, duration=seconds))
        index = len(sections['Program'])
        sections['Program'].append(dict(stable_id=max((r['stable_id'] for r in sections['Program']), default=0) + 1,
                                        first_command=first, command_count=7, phrase_count=1, source_path_string=S(source)))
        result.append(dict(programme=index, source=source, expected=row['expected']))
    texts(p, ex)
    dependencies = {Path(path).resolve().relative_to(ROOT).as_posix(): hashlib.sha256(Path(path).read_bytes()).hexdigest(),
                    'tools/field_prop_dialogues.py': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                    p['base_fixture']: p['base_sha256'], p['story_fixture']: p['story_sha256'],
                    **{'upstream/MOTHER-Encore/' + name: digest for name, digest in ex.sources.items()}}
    return result, dependencies


def metadata(path, first_programme, ref):
    import struct
    p, ex = read(path)
    policy = interaction_policy(path)
    programmes = [struct.pack('<6I', first_programme + i, 0, 0, *ref(row['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml')), 2) for i, row in enumerate(p['branches'])]
    localized = [struct.pack('<9I', row['id'], *ref(row['locale']), *ref(row['speaker']), *ref(row['body']), *ref(row['voice'])) for row in texts(p, ex)]
    binding = struct.pack('<4I3f6I', *ref(policy['flag']), first_programme, first_programme + 1,
                          *policy['ray_origin'], policy['ray_length'], policy['ray_mask'], policy['area_layer'],
                          policy['area_flags'], policy['props_bytes'], policy['props_crc'], int(policy['stop_running']))
    return programmes, localized, binding


def story_candidate(base, path=RECIPE):
    """Old formal metadata plus two checked reusable programmes and policy."""
    from tools.podunk_field import unpack_container, pack_container
    p, _ = read(path)
    _, sections = unpack_container(base, b'ENCFST01', (1, 24, 36, 40, 12, 32, 1), 128 * 1024, capability=4)
    sections = list(sections)
    pool = bytearray(sections[0])
    def ref(value):
        if not value:
            return 0, 0
        offset = len(pool)
        pool.extend(value.encode())
        return offset, len(value.encode())
    first = len(sections[1]) // 24
    programmes, localized, binding = metadata(path, first, ref)
    sections[0] = bytes(pool)
    sections[1] += b''.join(programmes)
    sections[2] += b''.join(localized)
    sections.append(binding)
    return pack_container(b'ENCFST01', p['scene_id'], sections, (1, 24, 36, 40, 12, 32, 1, 52), 128 * 1024, capability=5)


def candidate(base, path=RECIPE):
    p, _ = read(path)
    base_path = ROOT / p['base_fixture']
    require(base == base_path.read_bytes(), 'Prop candidate requires the current checked formal Room')
    source = nc.parse_pack(base)
    ir = copy.deepcopy(source)
    strings, sections = ir['strings'], ir['sections']
    def S(value):
        if value not in strings:
            strings.append(value)
        return strings.index(value)
    programmes, dependencies = add_room(path, ir['scene_id'], sections, strings, S)
    ir.pop('build_input_sha256')
    # This offline composition names its actual binary input. It does not
    # pretend a decoded packet contains its original compiler's provenance.
    dependencies = {**dependencies, base_path.relative_to(ROOT).as_posix(): hashlib.sha256(base).hexdigest()}
    ir['provenance'] = {'sources': dependencies}
    blob, _ = nc.compile_ir(ir)
    nc.parse_pack(blob)
    return blob, programmes, dependencies


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    a = parser.parse_args()
    out = a.out.resolve()
    require(out.is_relative_to(ROOT / 'build') and out != ROOT / 'build', 'Prop candidate output must remain inside owned build directory')
    blob, programmes, sources = candidate(a.base.read_bytes())
    out.mkdir(parents=True, exist_ok=True)
    (out / 'podunk.encroom').write_bytes(blob)
    (out / 'source.json').write_text(json.dumps(dict(scope=__doc__, programmes=programmes, sources=sources,
                                                   bytes=len(blob), sha256=hashlib.sha256(blob).hexdigest()), indent=2) + '\n', encoding='utf8')
    print('Root investigation candidate compiled; not staged:', len(blob), 'bytes')


if __name__ == '__main__':
    main()
