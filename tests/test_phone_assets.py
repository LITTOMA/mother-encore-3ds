import copy
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from tools import phone_assets as p
from tools.doll_dialogue import decode


class PhoneAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir = p.build()
        cls.phone = cls.ir['objects'][0]

    def test_original_atlas_output_and_recipe_verified(self):
        p.verify()
        resource = self.ir['resources'][0]
        self.assertEqual((resource['width'], resource['height'], resource['columns'], resource['rows']), (76, 22, 4, 1))
        self.assertGreater((p.OUT / 'phone.t3x').stat().st_size, 76 * 22 * 4)
        self.assertFalse(self.ir['shipping_integrated'])

    def test_root_override_inheritance_and_effective_collision(self):
        self.assertEqual(self.phone['position'], [148, 677])
        self.assertEqual(self.phone['sprite']['center'], [148, 680])
        self.assertEqual(self.phone['sprite']['frame_size'], [19, 22])
        self.assertEqual(self.phone['interaction']['center'], [148, 692])
        self.assertEqual(self.phone['interaction']['source_extents'], [15.7493, 19.1346])
        self.assertEqual(self.phone['interaction']['source_scale'], [1.01592, .940705])
        self.assertAlmostEqual(self.phone['interaction']['effective_extents'][0], 16, places=4)
        self.assertAlmostEqual(self.phone['interaction']['effective_extents'][1], 18, places=4)
        self.assertEqual(self.phone['collider']['center'], [149, 685.5])
        self.assertEqual(self.phone['collider']['extents'], [7.5, 4.5])
        self.assertEqual(self.phone['collider']['collision_layer'], 573)

    def test_inherited_area_offsets_and_flags_are_preserved(self):
        areas = self.ir['story_triggers']
        self.assertEqual([(a['source_path'], a['center'], a['extents']) for a in areas],
                         [('Cutscene Area4', [136, 816], [32, 8]), ('Cutscene Area3', [136, 824], [32, 8])])
        self.assertEqual(areas[0]['conditions'], [{'flag': 'doll_melody', 'value': True}, {'flag': 'phone_ring', 'value': False}])
        self.assertEqual(areas[1]['conditions'], [{'flag': 'phone_ring', 'value': True}, {'flag': 'talked_to_dad', 'value': False}])
        self.assertTrue(all(a['activation'] == 'player_contact_then_source_idle_process' for a in areas))
        self.assertEqual(self.ir['entrance_guard']['disappear_flag'], 'talked_to_dad')

    def test_phone_is_interactdialog_not_actor_npc_or_directional_sprite(self):
        self.assertEqual(self.phone['type'], 'InteractDialog')
        for key in ['creates_actor', 'turns_phone', 'sets_talker', 'marks_npc_seen']:
            self.assertFalse(self.phone['interaction'][key])
        for key in ['shadow', 'talk_animation']:
            self.assertFalse(self.phone['sprite'][key])
        self.assertEqual(self.phone['sprite']['direction_count'], 0)
        self.assertEqual(self.phone['interaction']['player_turn'], {'x': True, 'y': True})

    def test_last_matching_ordered_dispatch_and_no_seen(self):
        dispatch = self.phone['dispatch']
        self.assertEqual(dispatch['default'], 'Reusable/phonenoanswer')
        self.assertEqual(dispatch['policy'], 'last_matching_nonempty_flag')
        self.assertEqual(dispatch['overrides'], [{'flag': 'phone_ring', 'dialogue': 'Podunk/dad_poltergeist'},
                                                {'flag': 'talked_to_dad', 'dialogue': 'Reusable/dad_normal'}])
        carol = self.ir['carol_dispatch']
        self.assertTrue(carol['marks_selected_seen'])
        self.assertEqual([r['flag'] for r in carol['rows']][:4], ['', 'doll_melody', 'phone_ring', 'talked_to_dad'])
        self.assertTrue(carol['rows'][1]['seen_key'].endswith('/Objects/npc:doll_melody:1:Podunk/carol_call'))

    def test_ring_retains_exact_source_frames_sound_times_and_idempotence(self):
        ring = self.phone['clips'][1]
        self.assertEqual((ring['name'], ring['length'], ring['loop']), ('Ring', 1.25, True))
        frames = [e for e in ring['events'] if e['kind'] == 'Frame']
        sounds = [e for e in ring['events'] if e['kind'] == 'PlaySound']
        self.assertEqual([e['frame'] for e in frames], [1, 2, 3, 2, 3, 2, 1, 0])
        self.assertEqual([e['time'] for e in frames], [p.f32(t) for t in [0, .0833333, .166667, .25, .333333, .416667, .5, .583333]])
        self.assertEqual([e['time'] for e in sounds], [p.f32(.083), p.f32(.332)])
        self.assertTrue(all(e['resource'] == p.RING_SOUND for e in sounds))
        self.assertEqual(self.phone['ring']['idempotent_when_current'], 'Ring')
        self.assertFalse(self.phone['ring']['automatic_on_load'])
        self.assertEqual(self.phone['audio']['center'], [148, 686])

    def test_free_phone_use_order_consumes_no_card_or_money(self):
        use = self.phone['use']
        self.assertTrue(use['searches_for_phone_card'])
        for key in ['is_payphone', 'consumes_phone_card', 'consumes_cash']:
            self.assertFalse(use[key])
        self.assertEqual(use['save_location'], '')
        self.assertEqual([x['kind'] for x in use['ordered_actions']],
                         ['SetAudioStream', 'PlayAnimation', 'SetPhoneLocation', 'OpenDialogue', 'StartAudio'])
        self.assertEqual(use['ordered_actions'][0]['resource'], p.HANGUP_SOUND)
        self.assertEqual(use['ordered_actions'][1]['clip'], 'Idle')

    def test_native_animation_oracle_provenance_and_exact_keys(self):
        source=json.loads((p.ROOT/'content/phone-stage/native-animation.json').read_text())
        review=json.loads((p.ROOT/'compatibility/reviews/phone-animation.json').read_text())
        self.assertFalse(review['whole_handler_approved'])
        self.assertEqual(review['commit'],p.PIN)
        self.assertEqual(review['native_receipt_sha256'],p.sha(p.ROOT/'content/phone-stage/native-animation.json'))
        self.assertEqual(review['binary_fixture_sha256'],p.sha(p.ROOT/'content/phone-stage/native-animation.bin'))
        self.assertEqual(source['sources'],review['sources'])
        self.assertEqual(source['godot']['hash'],'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8')
        data=decode(source)
        self.assertEqual(data['delta'],p.f32(1/60))
        ring=data['clips']['Ring']
        self.assertEqual([k[0]for k in ring['tracks'][0]['keys']],
                         [e['time']for e in self.phone['clips'][1]['events']if e['kind']=='Frame'])
        self.assertEqual([k[0]for k in ring['tracks'][1]['keys']],
                         [e['time']for e in self.phone['clips'][1]['events']if e['kind']=='PlaySound'])
        self.assertEqual(len(data['records']),360)
        self.assertEqual(data['records'][155]['before'],1)
        self.assertEqual(data['records'][155]['frame'],0)

    def test_unknown_recipe_source_resources_or_version_rejected(self):
        recipe = p.recipe_from_ir(self.ir)
        for mutate in [lambda r: r.update(schema=2), lambda r: r.update(extra=True),
                       lambda r: r['resources'][0].update(columns=5),
                       lambda r: r['resources'][0].update(path='../escape.t3x'),
                       lambda r: r['sources'].update({p.SCENE: '0' * 64})]:
            changed = copy.deepcopy(recipe)
            mutate(changed)
            with self.assertRaisesRegex(ValueError, 'unreviewed'):
                p.validate_recipe(p.ROOT, changed)

    def test_missing_mutated_or_extra_compiled_assets_rejected(self):
        for action in ['missing', 'mutated', 'extra']:
            with tempfile.TemporaryDirectory() as td:
                out = Path(td)
                shutil.copy(p.OUT / 'source.json', out)
                if action != 'missing':
                    shutil.copy(p.OUT / 'phone.t3x', out)
                if action == 'mutated':
                    (out / 'phone.t3x').write_bytes(b'changed')
                if action == 'extra':
                    (out / 'unexpected').write_text('unreviewed')
                with self.assertRaises(ValueError):
                    p.verify(p.ROOT, out)


if __name__ == '__main__':
    unittest.main()
