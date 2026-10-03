import copy
import json
import tempfile
import unittest
from pathlib import Path
from tools import phone_dialogue as p


class PhoneDialogueTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir = p.build()
        cls.ex, cls.documents = p.load_receipt()

    def program(self, path):
        return next(x for x in self.ir['programs'] if x['source_path'] == path)

    def test_native_parser_source_gate_and_reproducible_output(self):
        p.verify()
        self.assertEqual(set(self.ir['sources']), set(p.SOURCES))
        self.assertFalse(self.ir['shipping_integrated'])
        self.assertEqual(len(self.ir['programs']), 6)

    def test_direct_carol_flag_timing_and_restoration(self):
        program = self.program(p.DIRECT)
        self.assertEqual(program['source_labels'], ['0', '3', '4', '5', '6', '7'])
        self.assertEqual(program['actor_bindings'], [{'actor': 'Carol', 'source': 'Objects/npc'}])
        commands = program['commands']
        timed = [c for c in commands if c['source_label'] == '5']
        self.assertEqual([c['kind'] for c in timed],
                         ['HideDialogue', 'StartWait', 'CallObjectDeferred', 'TurnActor', 'AwaitTimer'])
        self.assertEqual(timed[1]['duration'], 1)
        self.assertEqual(timed[2]['binding'], 'phone_ring')
        self.assertEqual(timed[3]['vector'], [-1, 0])
        self.assertEqual(timed[3]['duration'], .05)
        self.assertEqual(timed[3]['flags'], 0)
        final = [c for c in commands if c['source_label'] == '7']
        self.assertEqual([c['kind'] for c in final[:3]], ['ShowDialogue', 'SetFlag', 'AwaitDialogue'])
        self.assertEqual(final[1]['flag'], 'phone_ring')
        self.assertEqual([c['actor'] for c in commands if c['kind'] == 'RestoreActor'], ['Carol'])
        self.assertEqual([c['flags'] for c in commands if c['source_label'] == '3' and c['kind'] == 'AwaitDialogue'], [2])

    def test_area_call_preserves_wait_steps_and_enqueue_time_target(self):
        commands = self.program(p.AREA)['commands']
        self.assertEqual([c['actor'] for c in commands if c['kind'] == 'BindActor'], ['Ninten', 'Carol'])
        movement = next(c for c in commands if c['kind'] == 'MoveActorPath')
        self.assertEqual(movement['path']['movement'], [{'x': 0, 'y': -32}, {'wait': .1}])
        turn = next(c for c in commands if c['kind'] == 'TurnActor' and c['actor'] == 'Ninten')
        self.assertEqual((turn['target_actor'], turn['flags'], turn['vector'], turn['duration']), ('Carol', 3, [0, 0], .1))
        self.assertLess(commands.index(movement), commands.index(turn))
        self.assertEqual([c['actor'] for c in commands if c['kind'] == 'RestoreActor'], ['Ninten', 'Carol'])

    def test_reminder_has_no_ring_or_flags_and_original_motion(self):
        commands = self.program(p.REMINDER)['commands']
        self.assertFalse(any(c['kind'] in ['SetFlag', 'CallObjectDeferred'] for c in commands))
        movement = next(c for c in commands if c['kind'] == 'MoveActorPath')
        self.assertEqual(movement['path']['movement'], [{'wait': .1}, {'x': 0, 'y': -16}])
        self.assertEqual(next(c for c in commands if c['kind'] == 'ReturnCamera')['duration'], .1)

    def test_first_dad_has_no_proxy_actor_and_only_final_flag(self):
        program = self.program(p.FIRST_DAD)
        self.assertEqual(program['source_labels'], ['0', '1', '7', '9', '13'])
        self.assertEqual(program['actor_bindings'], [])
        commands = program['commands']
        self.assertFalse(any(c['kind'] in ['BindActor', 'RestoreActor', 'StopInteraction'] for c in commands))
        self.assertTrue(all(c['flags'] == 1 and c['actor'] == 'None'
                            for c in commands if c['kind'] == 'ShowDialogue'))
        voiced = next(t for t in self.ir['texts'] if t['identity'] == p.FIRST_DAD + '::1')
        self.assertEqual(voiced['voice'], 'Audio/Sound effects/text/Adult.mp3')
        move = next(c for c in commands if c['kind'] == 'MoveCamera')
        self.assertEqual((move['source_label'], move['vector'], move['flags'], move['duration']), ('7', [96, 0], 1, 1))
        flag = next(c for c in commands if c['kind'] == 'SetFlag')
        self.assertEqual((flag['source_label'], flag['flag'], flag['value']), ('13', 'talked_to_dad', 1))
        self.assertEqual([c['kind'] for c in commands if c['source_label'] == '13'][:3],
                         ['ShowDialogue', 'SetFlag', 'AwaitDialogue'])
        self.assertTrue(program['clears_phone_location_on_finish'])

    def test_no_answer_and_carol_reminder_are_literal_programs(self):
        for path in [p.NO_ANSWER, p.CAROL_PHONE]:
            commands = self.program(path)['commands']
            self.assertEqual(sum(c['kind'] == 'ShowDialogue' for c in commands), 1)
            self.assertFalse(any(c['kind'] in ['SetFlag', 'BindActor', 'RestoreActor'] for c in commands))

    def test_unknown_document_fields_labels_targets_and_types_reject(self):
        mutations = [lambda d: d['0'].update(commands=['dangerous()']), lambda d: d['0'].update(goto='99'),
            lambda d: d['5']['objectsfunction'].update({'Objects/Phone': '_unknown'}),
            lambda d: d['3'].update(autoadvance=1), lambda d: d['5'].update(autowait=True),
            lambda d: d['4'].update(wait=float('nan')), lambda d: d['5']['actorsturn']['carol'].update(x=True),
            lambda d: d.pop('3'), lambda d: d['0']['actors'].update(phone='Objects/Phone')]
        for mutate in mutations:
            doc = copy.deepcopy(self.documents[p.DIRECT])
            mutate(doc)
            with self.assertRaises(ValueError):
                p.compile_program(p.DIRECT, doc, .5)
        for value in [0, -1, True, float('inf')]:
            with self.assertRaises(ValueError):
                p.compile_program(p.DIRECT, self.documents[p.DIRECT], value)
        with self.assertRaises(ValueError):
            p.compile_program(p.NORMAL, self.documents[p.NORMAL], .5)

    def test_source_phrase_key_order_cannot_change_handler_order(self):
        original = self.documents[p.AREA]
        reordered = {k: dict(reversed(list(v.items()))) for k, v in original.items()}
        self.assertEqual(p.compile_program(p.AREA, original, .5), p.compile_program(p.AREA, reordered, .5))
        reordered['0']['actors'] = dict(reversed(list(reordered['0']['actors'].items())))
        with self.assertRaisesRegex(ValueError, 'binding order'):
            p.compile_program(p.AREA, reordered, .5)

    def test_hints_preserve_source_tokens_and_color(self):
        dad = next(t for t in self.ir['texts'] if t['identity'] == p.FIRST_DAD + '::1')
        self.assertEqual(len(dad['segments']), 6)
        kinds = [t['kind'] for s in dad['segments'] for t in s['tokens']]
        self.assertEqual(kinds.count('HintStart'), 2)
        self.assertEqual(kinds.count('HintEnd'), 2)
        self.assertEqual(self.ir['text_contracts']['hint_color_hex'], 'ea8b2c')
        for text in ['Hello', '[@][Unknown]', '[@][color]unterminated', '[@][/color]bad', '[@][EarnedCash]']:
            with self.assertRaises(ValueError):
                p.text_segments(text)

    def test_normal_graph_preserves_leaders_money_and_record_suspend(self):
        graph = self.ir['dad_normal']
        nodes = {n['label']: n for n in graph['nodes']}
        self.assertEqual(graph['execution_status'], 'not_integrated')
        for leader in ['lloyd', 'ana', 'teddy', 'pippi']:
            self.assertEqual(nodes['check_' + leader]['branch'],
                             {'kind': 'LeaderEquals', 'value': leader, 'target': leader + '_leader'})
        self.assertEqual(nodes['check_earned_cash']['branch'],
                         {'kind': 'FlagEquals', 'flag': 'earned_cash', 'value': True, 'target': '1'})
        earned = nodes['1']['text_replacement_effects'][0]
        self.assertEqual((earned['capture'], earned['reset_amount'], earned['clear_flag'], earned['flags_updated']),
                         ('earned_cash', 0, 'earned_cash', False))
        self.assertEqual(earned['frequency'], 'once_per_phrase_entry')
        self.assertFalse(nodes['2']['text_replacement_effects'][0]['mutate'])
        self.assertEqual(nodes['2']['reviewed_noop']['flag'], 'money_earned')
        self.assertEqual([(x['text_en'], x['target']) for x in nodes['2']['choices']], [('Record', '4'), ('Nothing, really', '6')])
        self.assertEqual((nodes['2']['cancel_target'], nodes['2']['initial_selection']), ('6', 0))
        self.assertEqual(nodes['4']['evaluate_branch'], 'submenu_callback')
        self.assertEqual(nodes['4']['entry'][2], {'kind': 'SetFlag', 'flag': 'saved', 'value': False})
        self.assertFalse(nodes['4']['textless_immediate'])
        self.assertTrue(nodes['4']['hidden_text_does_not_finish'])
        self.assertEqual((nodes['4']['branch']['target'], nodes['4']['fallback']), ('5', '6'))

    def test_normal_options_and_noop_cannot_be_silently_changed(self):
        _, translations = p.extract_texts(self.ex, self.documents)
        for mutate in [lambda d: d['2'].update(unsetflags='earned_cash'),
                       lambda d: d['4'].pop('text'), lambda d: d['4'].update(caninput=True),
                       lambda d: d['2']['options'].update(cancel='4'),
                       lambda d: d['check_earned_cash']['if']['flags'].update(earned_cash=1)]:
            doc = copy.deepcopy(self.documents[p.NORMAL])
            mutate(doc)
            with self.assertRaises(ValueError):
                p.compile_normal_graph(doc, translations)

    def test_changed_receipt_source_hash_version_and_extra_fields_reject(self):
        receipt = json.loads((p.ROOT / p.RECEIPT).read_text())
        review = json.loads((p.ROOT / p.REVIEW).read_text())
        for mutate in [lambda d: d.update(schema=2), lambda d: d.update(extra=1),
                       lambda d: d['godot'].update(major=4), lambda d: d['sources'].update({p.DIRECT: '0' * 64})]:
            with tempfile.TemporaryDirectory() as td:
                root = Path(td)
                (root / 'upstream').symlink_to(p.ROOT / 'upstream', target_is_directory=True)
                (root / 'upstream.lock').symlink_to(p.ROOT / 'upstream.lock')
                (root / 'compatibility').mkdir()
                (root / 'compatibility/upstream-inventory.json').symlink_to(p.ROOT / 'compatibility/upstream-inventory.json')
                changed = copy.deepcopy(receipt)
                mutate(changed)
                p.write_json(root / p.RECEIPT, changed)
                changed_review = copy.deepcopy(review)
                changed_review['native_receipt_sha256'] = p.sha(root / p.RECEIPT)
                p.write_json(root / p.REVIEW, changed_review)
                with self.assertRaises(ValueError):
                    p.load_receipt(root)


if __name__ == '__main__':
    unittest.main()
