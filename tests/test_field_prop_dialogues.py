import copy, json, tempfile, unittest
from pathlib import Path
from tools import field_prop_dialogues as d, native_content as nc


class PropDialogueSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.policy, cls.source = d.read()
        cls.base = (d.ROOT / cls.policy['base_fixture']).read_bytes()
        cls.original = nc.parse_pack(cls.base)

    def test_actual_source_no_voice_name_actors_or_grants(self):
        rows = d.texts(self.policy, self.source)
        self.assertEqual(len(rows), 4)
        self.assertEqual({r['locale'] for r in rows}, {'en', 'zh_Hans_CN'})
        self.assertTrue(all(r['voice'] == r['speaker'] == '' for r in rows))
        self.assertEqual([r['body'] for r in rows if r['locale'] == 'en'],
                         ['[@]This looks too hard to break.', '[@]You could easily break this with your bat.'])

    def test_bounded_candidate_only_appends_two_actorless_programmes(self):
        blob, rows, sources = d.candidate(self.base)
        room = nc.parse_pack(blob)
        self.assertEqual(room['capabilities'], self.original['capabilities'])
        self.assertEqual([r['expected'] for r in rows], [False, True])
        self.assertIn(self.policy['base_fixture'], sources)
        for name, old in self.original['sections'].items():
            if name not in ('StringRef', 'StringBytes'):
                self.assertEqual(room['sections'][name][:len(old)], old, name)
        self.assertEqual(room['strings'][:len(self.original['strings'])], self.original['strings'])
        for row in room['sections']['Program'][-2:]:
            commands = room['sections']['Command'][row['first_command']:row['first_command'] + row['command_count']]
            self.assertEqual([nc.OPCODES[c['opcode']] for c in commands], ['BeginCutscene', 'ShowDialogue', 'AwaitDialogue', 'HideDialogue', 'SetTalker', 'CutsceneEnded', 'DialogueDone'])
            self.assertTrue(all(c['actor_index'] == nc.NO_ACTOR for c in commands))
            self.assertEqual(commands[1]['flags'], 1)

    def changed(self, mutate):
        with tempfile.TemporaryDirectory(prefix='prop-dialogue-source-', dir=d.ROOT / 'build') as folder:
            value = copy.deepcopy(self.policy)
            mutate(value)
            path = Path(folder) / 'review.json'
            path.write_text(json.dumps(value), encoding='utf8')
            with self.assertRaises(ValueError):
                d.read(path)

    def test_unknown_schema_and_malformed_paths_fail_closed(self):
        for key, value in [('schema', 2), ('schema', True), ('commit', '0' * 40), ('scene_id', True), ('flag', 'other'), ('locales', ['en']), ('documents', {}), ('sources', {})]:
            with self.subTest(key=key, value=value):
                self.changed(lambda p: p.update({key: value}))
        self.changed(lambda p: p['sources'].update({'../unsafe': '0' * 64}))
        self.changed(lambda p: p['sources'].update({'Scripts/Main/Dead Bush.gd': '0' * 64}))

    def test_branch_order_source_and_identity_reject(self):
        for key, value in [('expected', 0), ('expected', True), ('source', '../escape'), ('dialogue_id', True), ('dialogue_id', 0)]:
            with self.subTest(key=key, value=value):
                self.changed(lambda p: p['branches'][0].update({key: value}))
        self.changed(lambda p: p['branches'][1].update(dialogue_id=p['branches'][0]['dialogue_id']))

    def test_unmapped_source_document_fields_and_changed_text_reject(self):
        for key, value in [('item', 'Shovel'), ('sound', 'Kid'), ('actors', {}), ('setflags', 'got_shovel'), ('text', 'OTHER')]:
            with self.subTest(key=key):
                self.changed(lambda p: p['documents'][p['branches'][0]['source']]['0'].update({key: value}))

    def test_composition_rejects_scene_and_existing_dialogue_ids(self):
        for kind in ('scene', 'duplicate', 'repeated-source'):
            room = copy.deepcopy(self.original)
            def S(value):
                if value not in room['strings']:
                    room['strings'].append(value)
                return room['strings'].index(value)
            scene = room['scene_id'] + int(kind == 'scene')
            if kind == 'duplicate':
                room['sections']['Command'].append(dict(opcode=nc.OPCODES.index('ShowDialogue'), target_index=3300))
            if kind == 'repeated-source':
                room['sections']['Program'].append(dict(source_path_string=S('Reusable/hardtobreak')))
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                d.add_room(d.RECIPE, scene, room['sections'], room['strings'], S)


if __name__ == '__main__':
    unittest.main()
