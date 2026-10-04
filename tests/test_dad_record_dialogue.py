#!/usr/bin/env python3
import copy
import json
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.dad_record_dialogue import build, compile_graph, validate_graph, REPORT
from tools.phone_dialogue import expected_documents, NORMAL


class DadRecord(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = build(ROOT)
        cls.graph = cls.data['program']

    def test_stage_is_reproducible(self):
        self.assertEqual(self.data, json.loads((ROOT / REPORT).read_text()))

    def walk(self, leader, earned, selection, saved):
        graph = self.graph
        pc, output, effects, suspended = 0, [], [], False
        group = graph['choice_groups'][0]
        for _ in range(128):
            command = graph['commands'][pc]
            kind = command['kind']
            next_pc = pc + 1
            if kind == 'ShowDialogue': output.append(command['source_label'])
            elif kind == 'Jump': next_pc = command['target_pc']
            elif kind == 'BranchLeader' and command['leader'] == leader: next_pc = command['target_pc']
            elif kind == 'BranchFlag':
                flag_value = earned if command['flag'] == 'earned_cash' else saved
                if flag_value == bool(command['value']): next_pc = command['target_pc']
            elif kind == 'AwaitChoices':
                next_pc = group['cancel_target_pc'] if selection == 'cancel' else group['options'][selection]['target_pc']
            elif kind == 'SetFlag': effects.append((command['flag'], command['value']))
            elif kind == 'OpenSave': effects.append(('OpenSave', True))
            elif kind == 'AwaitSubmenu': suspended = True
            elif kind == 'DialogueDone': return output, effects, suspended
            pc = next_pc
        self.fail('Compiled graph did not terminate')

    def test_every_leader_money_option_and_save_branch(self):
        for leader in ['ninten', 'lloyd', 'ana', 'teddy', 'pippi']:
            for earned in [False, True]:
                for choice in [0, 1, 'cancel']:
                    for saved in [False, True]:
                        with self.subTest(leader=leader, earned=earned, choice=choice, saved=saved):
                            texts, effects, suspended = self.walk(leader, earned, choice, saved)
                            expected = ['0'] + ([] if leader == 'ninten' else [leader + '_leader'])
                            expected += ['1'] if earned else []
                            expected += ['2'] + (['5'] if choice == 0 and saved else []) + ['6', '7']
                            self.assertEqual(texts, expected)
                            self.assertEqual(effects, [('saved', 0), ('OpenSave', True)] if choice == 0 else [])
                            self.assertEqual(suspended, choice == 0)

    def test_hidden_save_source_order_and_no_text_gate(self):
        commands = [c['kind'] for c in self.graph['commands'] if c['source_label'] == '4']
        self.assertEqual(commands, ['HideDialogue', 'SetFlag', 'OpenSave', 'AwaitSubmenu', 'BranchFlag', 'Jump'])
        self.assertNotIn('AwaitDialogue', commands)

    def test_choice_wait_is_final_print_completion(self):
        commands = [c['kind'] for c in self.graph['commands'] if c['source_label'] == '2']
        self.assertEqual(commands, ['ShowDialogue', 'AwaitChoices'])
        text = next(t for t in self.data['texts'] if t['source_label'] == '2')
        self.assertEqual([s['wait_for_input'] for s in text['segments']], [True, False])

    def test_money_tokens_are_effectful_only_at_source_phrase(self):
        amounts = [(t['source_label'], tok['kind']) for t in self.data['texts'] for s in t['segments']
                   for tok in s['house_tokens'] if tok['kind'] in [5, 6, 7]]
        self.assertEqual(amounts, [('1', 5), ('2', 6)])
        self.assertEqual(self.graph['reviewed_noops'], [dict(source_label='2', command='unsetflags',
            value='money_earned', before_pc=self.graph['label_to_pc']['2'] + 1, mutation=False, signal=False)])

    def test_alternate_leader_delays_and_newlines_preserved(self):
        for leader, amount in [('lloyd', '4'), ('ana', '4'), ('teddy', '5'), ('pippi', '5')]:
            t = next(t for t in self.data['texts'] if t['source_label'] == leader + '_leader')
            tokens = t['segments'][0]['house_tokens']
            self.assertEqual([v['text'] for v in tokens if v['kind'] == 8], [amount])
            self.assertEqual([v for v in tokens if v['kind'] == 9], [dict(kind=9, text='')])
            self.assertTrue(all('\n' not in v['text'] for v in tokens))

    def test_unknown_source_field_or_changed_option_fails_closed(self):
        for label, key, value in [('2', 'unsetflags', 'something_unknown'), ('4', 'save', False),
                                  ('check_lloyd', 'if', {'leader': 'eve', 'goto': 'lloyd_leader'})]:
            doc = copy.deepcopy(expected_documents()[NORMAL]); doc[label][key] = value
            with self.assertRaises(ValueError): compile_graph(doc, .2)
        doc = copy.deepcopy(expected_documents()[NORMAL]); doc['0']['unknown_command'] = True
        with self.assertRaises(ValueError): compile_graph(doc, .2)

    def test_unknown_and_out_of_bounds_branches_rejected(self):
        g = copy.deepcopy(self.graph); g['commands'][4]['target_pc'] = len(g['commands'])
        with self.assertRaises(ValueError): validate_graph(g)
        g = copy.deepcopy(self.graph); g['choice_groups'][0]['options'][0]['target_pc'] = len(g['commands'])
        with self.assertRaises(ValueError): validate_graph(g)
        g = copy.deepcopy(self.graph); g['commands'][0]['kind'] = 'PretendSuccess'
        with self.assertRaises(ValueError): validate_graph(g)

    def test_graph_must_use_reviewed_source_binding(self):
        for key in ('identity', 'source_path'):
            g = copy.deepcopy(self.graph); g[key] = 'unknown'
            with self.assertRaises(ValueError): validate_graph(g)
        for key, value in (('id', 'unknown::2'), ('source_label', 'unknown'),
                           ('initial_selection', 1)):
            g = copy.deepcopy(self.graph); g['choice_groups'][0][key] = value
            with self.assertRaises(ValueError): validate_graph(g)
        for kind, field in (('BranchFlag', 'flag'), ('BranchLeader', 'leader')):
            g = copy.deepcopy(self.graph)
            next(c for c in g['commands'] if c['kind'] == kind)[field] = 'unknown'
            with self.assertRaises(ValueError): validate_graph(g)


if __name__ == '__main__': unittest.main()
