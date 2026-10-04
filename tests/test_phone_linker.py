import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

from tools import link_phone_content as linker
from tools import native_content, native_house
from tools import link_pillow_content as pillow
from tools.extract_native_content import Extractor
from tools.extract_battle_entry import Extractor as HouseExtractor
from tools.extract_house import build as build_house


class PhoneLinkerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = Path(__file__).resolve().parents[1]
        cls.room = json.loads((cls.root / 'content/native-opening.json').read_text())
        cls.house = json.loads((cls.root / 'content/native-house.json').read_text())
        cls.audio = json.loads((cls.root / 'content/native-audio.json').read_text())
        cls.stage, cls.presentation = linker.load_stage(cls.root)
        cls.dad, cls.choices = linker.load_dad(cls.root)
        with patch.object(linker, 'append_dad_room', lambda ex, dialogue: None), patch.object(pillow, 'append_room', lambda *args: None):
            cls.phone_room, _ = Extractor(cls.root).run()
        with patch.object(linker, 'link_room', lambda ex: None), patch.object(pillow, 'append_room', lambda *args: None):
            cls.base_room, _ = Extractor(cls.root).run()
        with patch.object(linker, 'link_house', lambda ex, ir, room: ir), patch.object(pillow, 'append_house', lambda ex, ir, room: ir):
            cls.base_house = build_house(cls.root)
        # Carol's mapping now comes from reviewed House bindings before the
        # phone append pass. Project only that documented metadata to the
        # previous prefix; every other old-field equality remains checked.
        assert cls.base_house['npcs'][0]['room_actor_index'] == 5
        cls.base_house['npcs'][0]['room_actor_index'] = linker.NONE

    def program(self, identity):
        return next(p for p in self.room['sections']['Program']
                    if self.room['strings'][p['source_path_string']] == identity)

    def commands(self, identity):
        p = self.program(identity)
        return self.room['sections']['Command'][p['first_command']:p['first_command'] + p['command_count']]

    def test_all_existing_room_rows_and_strings_are_preserved(self):
        self.assertEqual(self.room['strings'][:len(self.base_room['strings'])], self.base_room['strings'])
        for name, rows in self.base_room['sections'].items():
            self.assertEqual(self.room['sections'][name][:len(rows)], rows, name)
        self.assertEqual((self.room['rules'], self.room['capabilities']), (7, 7))
        self.assertEqual(len(self.room['sections']['Program']), 16)
        self.assertEqual(len(self.room['sections']['ActorInstance']), 7)

    def test_house_prefix_and_optional_story_state_preserved(self):
        self.assertEqual(self.house['segments'][:len(self.base_house['segments'])], self.base_house['segments'])
        self.assertEqual(self.house['dialogues'][:11], self.base_house['dialogues'])
        for name in ['doors', 'openable_doors', 'boundaries']:
            self.assertEqual(self.house[name], self.base_house[name], name)
        self.assertEqual(self.house['story_conditions'][:len(self.base_house['story_conditions'])], self.base_house['story_conditions'])
        for new, old in zip(self.house['overrides'], self.base_house['overrides']):
            if new['dialogue'] == 'Podunk/minnie_door_open':
                self.assertNotEqual(new['dialogue_index'], linker.NONE)
                new = dict(new, dialogue_index=old['dialogue_index'])
            self.assertEqual(new, old)
        self.assertEqual(self.house['npcs'][1:3], self.base_house['npcs'][1:3])
        minnie = dict(self.house['npcs'][3], program_index=linker.NONE, seen_key='')
        self.assertEqual(minnie, self.base_house['npcs'][3])
        changed = copy.deepcopy(self.house['npcs'][0])
        changed['room_actor_index'] = linker.NONE
        self.assertEqual(changed, self.base_house['npcs'][0])
        self.assertEqual(self.house['npcs'][0]['room_actor_index'], 5)

    def test_only_reviewed_contact_boundaries_become_programs(self):
        for new, old in zip(self.house['story_triggers'], self.base_house['story_triggers']):
            if new['source_path'] not in ['Cutscene Area3', 'Cutscene Area4']:
                self.assertEqual(new, old)
                continue
            self.assertEqual(new['disposition'], 2)
            self.assertEqual(new['center'], [136, 824 if new['source_path'].endswith('3') else 816])
            self.assertEqual(new['extents'], [32, 8])
            restored = dict(new, disposition=old['disposition'], program_index=old['program_index'])
            self.assertEqual(restored, old)
        self.assertEqual(self.room['sections']['InitialFlag'], self.base_room['sections']['InitialFlag'])

    def test_carol_uses_shared_verified_frames_but_own_source_geometry(self):
        s = self.room['sections']
        carol, minnie = s['ActorProfile'][5], s['ActorProfile'][4]
        self.assertNotEqual(carol['primary_resource'], minnie['primary_resource'])
        self.assertEqual(carol['sprite_position'], [0, 9])
        self.assertEqual(carol['sprite_offset'], [0, -16])
        self.assertEqual(carol['emote_offset'], [0, -40])
        self.assertEqual(s['ActorInstance'][5]['position'], [192, 704])
        self.assertEqual(s['Clip'][:len(self.base_room['sections']['Clip'])], self.base_room['sections']['Clip'])
        a = s['AnimationBinding'][carol['animation_binding_first']:carol['animation_binding_first'] + 12]
        b = s['AnimationBinding'][minnie['animation_binding_first']:minnie['animation_binding_first'] + 12]
        self.assertEqual(a, [dict(row, actor_profile_index=5) for row in b])

    def test_linear_texts_have_stable_identity_and_typed_color(self):
        texts = linker.linear_texts(self.stage)
        ids = linker.text_ids(self.stage)
        self.assertEqual([d['id'] for d in self.house['dialogues'][11:11 + len(texts)]], list(range(12, 12 + len(texts))))
        tokens = [t for s in self.house['segments'][len(self.base_house['segments']):] for t in s['tokens']]
        self.assertTrue(any(t == dict(kind=3, text=self.stage['text_contracts']['hint_color_hex']) for t in tokens))
        self.assertTrue(any(t == dict(kind=4, text='') for t in tokens))
        self.assertFalse(any('[' in t['text'] or ']' in t['text'] for t in tokens if t['kind'] == 1))
        for program in self.stage['programs']:
            lowered = self.commands(program['identity'])
            self.assertEqual(len(lowered), len(program['commands']))
            for command, source in zip(lowered, program['commands']):
                if source['kind'] == 'ShowDialogue':
                    self.assertEqual(command['target_index'], ids[source['dialogue_key']])
                self.assertEqual(command['phrase'], source['phrase'])

    def test_movement_waits_target_turn_camera_axis_and_phase_timing(self):
        commands = self.commands('Podunk/cutscenes/carol_call')
        move = next(c for c in commands if c['opcode'] == 29)
        s = self.room['sections']
        path = s['MovementPath'][move['target_index']]
        self.assertEqual(s['MovementEntry'][path['first_entry']:path['first_entry'] + 2],
            [dict(kind=0, vector=[0, -32], duration=0), dict(kind=1, vector=[0, 0], duration=.1)])
        turn = next(c for c in commands if c['opcode'] == 10 and c['actor_index'] == 0)
        self.assertEqual((turn['flags'], turn['target_index'], turn['auxiliary_index']), (3, 5, linker.NONE))
        dad = self.commands('Podunk/dad_poltergeist')
        camera = next(c for c in dad if c['opcode'] == 17)
        self.assertEqual((camera['flags'], camera['target_index'], camera['vector'], camera['duration']), (1, 1, [96, 0], 1))
        self.assertFalse(any(c['actor_index'] != 65535 for c in dad))
        final = [c for c in dad if c['phrase'] == 4]
        self.assertEqual([c['opcode'] for c in final[:3]], [32, 31, 33])

    def test_phone_audio_identity_binding_and_old_asset_prefix(self):
        self.assertEqual(self.room['sections']['Binding'][-2],
            dict(stable_id=8, kind=8, flags=0, target_index=0, auxiliary_index=linker.NONE, value=0, duration=0))
        self.assertEqual(self.audio, linker.build_audio(self.root, self.room))
        self.assertEqual([a['stable_id'] for a in self.audio['assets']],
                         [21, 22, 1001, 1002, 1101, 1102, 1103, 1104, 30, 32, 33, 34, 36, 37, 1201, 1202])
        self.assertEqual([a['source_path'] for a in self.audio['assets'][12:15]],
            ['res://' + linker.RING_SOUND, 'res://' + linker.HANGUP_SOUND, 'res://Audio/Sound effects/text/Adult.mp3'])
        self.assertEqual(self.audio['assets'][15]['source_path'],'res://Audio/Music/Mother Earth.mp3')

    def test_entrance_listener_uses_original_descendant_body_and_flag(self):
        s = self.room['sections']
        body = next(b for b in s['BodyRule'] if self.room['strings'][b['source_path_string']] == 'DoorBlock/Entrance')
        flag = next(i for i, f in enumerate(s['Flag']) if self.room['strings'][f['name_string']] == 'talked_to_dad')
        self.assertEqual(s['Binding'][-1], dict(stable_id=9, kind=9, flags=0,
            target_index=body['body_id'], auxiliary_index=flag, value=1, duration=0))
        self.assertEqual(body['initially_enabled'], 1)
        self.assertEqual(s['BodyRule'], self.base_room['sections']['BodyRule'])
        self.assertEqual(s['Polygon'], self.base_room['sections']['Polygon'])
        self.assertEqual(s['Vec2'], self.base_room['sections']['Vec2'])

    def test_carol_phone_reminder_preserves_inherited_npc_talker(self):
        commands = self.commands('Podunk/carol_phone')
        for opcode in [32, 19]:
            command = next(c for c in commands if c['opcode'] == opcode)
            self.assertEqual((command['actor_index'], command['flags']), (65535, 1))
        self.assertFalse(any(c['opcode'] in [1, 20] for c in commands))

    def test_deferred_flag_listener_rejects_invalid_body_flag_and_payload(self):
        for change in [dict(target_index=0), dict(auxiliary_index=linker.NONE),
                       dict(duration=.1), dict(value=2), dict(flags=1)]:
            ir = copy.deepcopy(self.room)
            ir['sections']['Binding'][-1].update(change)
            with self.assertRaises(ValueError):
                native_content.compile_ir(ir)

    def test_entrance_source_listener_cannot_become_visibility_only(self):
        ex = Extractor(self.root)
        ex.sections = copy.deepcopy(self.base_room['sections'])
        ex.strings = self.base_room['strings'][:]
        original = ex.text
        def changed(path):
            source = original(path)
            if path == 'Scripts/Main/Flag Landmarks.gd':
                return source.replace('export var delete_if_hidden = true', 'export var delete_if_hidden = false')
            return source
        with patch.object(ex, 'text', side_effect=changed):
            with self.assertRaisesRegex(ValueError, 'disappear-only subtree deletion'):
                linker.append_entrance_flag_listener(ex, self.presentation)

    def test_dad_normal_links_every_source_branch_choice_and_save_callback(self):
        program = self.program('Reusable/dad_normal')
        self.assertEqual((program['stable_id'], program['first_command'], program['command_count']), (12, 352, 48))
        graph = self.dad['program']
        commands = self.commands(graph['identity'])
        ids = linker.dad_text_ids(self.stage, self.dad)
        self.assertEqual(list(ids.values()), list(range(29, 39)))
        for source, command in zip(graph['commands'], commands):
            self.assertEqual(native_content.OPCODES[command['opcode']], source['kind'])
            self.assertEqual(command['phrase'], source['phrase'])
            if source['kind'] == 'Jump': self.assertEqual(command['target_index'], source['target_pc'])
            elif source['kind'] in ['BranchFlag', 'BranchLeader']:
                self.assertEqual(command['auxiliary_index'], source['target_pc'])
                if source['kind'] == 'BranchLeader': self.assertEqual(self.room['strings'][command['target_index']], source['leader'])
                else:
                    flag = self.room['sections']['Flag'][command['target_index']]
                    self.assertEqual(self.room['strings'][flag['name_string']], source['flag'])
                    self.assertEqual(command['value'], source['value'])
            elif source['kind'] == 'ShowDialogue': self.assertEqual(command['target_index'], ids[source['dialogue_key']])
        self.assertEqual(commands[30]['target_index'], 0)
        group = self.choices['groups'][0]
        self.assertEqual(group, graph['choice_groups'][0])
        self.assertEqual([o['target_pc'] for o in group['options']], [31, 40])
        self.assertEqual(group['cancel_target_pc'], 40)
        self.assertEqual([c['opcode'] for c in commands[31:37]], [35, 31, 40, 41, 37, 36])
        # The source staging receipt stays unchanged; linking is a separate pass.
        self.assertEqual(self.stage['dad_normal']['execution_status'], 'not_integrated')
        self.assertEqual(graph['reviewed_noops'][0]['value'], 'money_earned')

    def test_dad_append_preserves_first_call_rows_strings_and_all_text_tokens(self):
        self.assertEqual(self.room['strings'][:len(self.phone_room['strings'])], self.phone_room['strings'])
        for name, rows in self.phone_room['sections'].items():
            self.assertEqual(self.room['sections'][name][:len(rows)], rows, name)
        ids = linker.dad_text_ids(self.stage, self.dad)
        for text in self.dad['texts']:
            dialogue = next(d for d in self.house['dialogues'] if d['id'] == ids[text['identity']])
            self.assertEqual(dialogue['source_path'], text['source_path'])
            segments = self.house['segments'][dialogue['first_segment']:dialogue['first_segment'] + dialogue['segment_count']]
            self.assertEqual(len(segments), len(text['segments']))
            for segment, source in zip(segments, text['segments']):
                self.assertEqual(segment['tokens'], source['house_tokens'])
                self.assertEqual(segment['speaker'], text['speaker_en'])
                self.assertEqual(segment['voice'], text['voice'])

    def test_room_and_house_roundtrip_with_new_schema(self):
        blob, manifest = native_content.compile_ir(self.room)
        native_content.parse_pack(blob)
        self.assertEqual(manifest['rules'], 7)
        tables = native_house.lower(self.house, root=self.root)
        blob = native_house.encode(tables, version=6)
        decoded = native_house.parse_pack(blob)
        self.assertEqual(decoded['Tokens'], [tuple(t) for t in tables['Tokens']])

    def test_invalid_linked_commands_and_old_schema_fail_closed(self):
        for mutate in [lambda r: r.update(rules=4, capabilities=4),
                       lambda r: r.update(rules=5, capabilities=5),
                       lambda r: r['sections']['Binding'][-1].update(kind=10),
                       lambda r: next(c for c in r['sections']['Command'] if c['opcode'] == 10 and c['flags'] == 3).update(target_index=65535),
                       lambda r: next(c for c in r['sections']['Command'] if c['opcode'] == 17 and c['flags'] == 1).update(vector=[96, 12])]:
            ir = copy.deepcopy(self.room)
            mutate(ir)
            with self.assertRaises(ValueError):
                native_content.compile_ir(ir)
        tables = native_house.lower(self.house, root=self.root)
        with self.assertRaises(ValueError):
            native_house.parse_pack(native_house.encode(tables, version=4))

    def test_unknown_linear_text_token_and_changed_area_geometry_reject(self):
        for mutation in ['token', 'geometry']:
            stage, presentation = copy.deepcopy((self.stage, self.presentation))
            if mutation == 'token':
                stage['texts'][0]['segments'][0]['tokens'].append(dict(kind='Unknown'))
            else:
                presentation['story_triggers'][0]['center'][0] += 1
            with patch.object(linker, 'load_stage', return_value=(stage, presentation)):
                with self.assertRaises(ValueError):
                    linker.link_house(HouseExtractor(self.root), copy.deepcopy(self.base_house), self.room)

    def test_independent_hash_seeds_reproduce_all_linked_files(self):
        for seed in ['1', '789']:
            result = subprocess.run([sys.executable, 'tools/link_phone_content.py', '--check'], cwd=self.root,
                env=dict(os.environ, PYTHONHASHSEED=seed), capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
