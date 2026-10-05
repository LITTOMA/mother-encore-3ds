"""Manual-only source and Room opcode admission regression cases."""
import copy, json, unittest
from pathlib import Path
from unittest.mock import patch
from tools import storage_dialogue as source
from tools.native_content import validate_ir, ContentError

class StorageDialogueTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir=source.load()
        cls.room=json.loads((source.ROOT/'content/native-opening.json').read_text(encoding='utf8'))

    def test_choice_close_and_dormant_phrase_are_preserved(self):
        d=self.ir; group=d['choice_group']; commands=d['commands']
        self.assertEqual([o['target_label']for o in group['options']],['1','3'])
        self.assertEqual(group['cancel_target_pc'],d['labels']['3'])
        at=d['labels']['1']
        self.assertEqual([c['kind']for c in commands[at:at+4]],['HideDialogue','OpenStorage','AwaitSubmenu','Jump'])
        self.assertEqual(commands[at+3]['target_pc'],d['labels']['3'])
        self.assertEqual(next(t['raw']for t in d['texts']if t['label']=='2'),'DIALOGUE_PODUNK_MINNIE_STORAGE_2')

    def test_source_ir_wrong_types_unknown_command_and_targets_rejected(self):
        for mutate in (
            lambda d:d.update(schema=True),
            lambda d:d.update(commit='0'*40),
            lambda d:d['document']['1'].update(open_storage=1),
            lambda d:d['commands'][d['labels']['1']+1].update(kind='OpenGodStorage'),
            lambda d:d['choice_group'].update(cancel_target_pc=0),
            lambda d:d['choice_group']['options'][0].update(target_pc=99999),
            lambda d:d.update(unreviewed=True)):
            candidate=copy.deepcopy(self.ir);mutate(candidate)
            with patch.object(source,'read_json',return_value=candidate),self.assertRaises(ValueError):source.load()

    def test_opcode_requires_explicit_capability_and_immediate_gate(self):
        validate_ir(self.room)
        at=next(i for i,c in enumerate(self.room['sections']['Command'])if c['opcode']==43)
        for mutate in (
            lambda d:d.update(capabilities=7),
            lambda d:d.update(capabilities=9),
            lambda d:d['sections']['Command'][at].update(opcode=44),
            lambda d:d['sections']['Command'][at].update(actor_index=0),
            lambda d:d['sections']['Command'][at].update(target_index=0),
            lambda d:d['sections']['Command'][at].update(flags=1),
            lambda d:d['sections']['Command'][at+1].update(opcode=25),
            lambda d:d['sections']['Command'][at].update(opcode=25)):
            d=copy.deepcopy(self.room);mutate(d)
            with self.assertRaises(ContentError):validate_ir(d)

    def test_audio_unknown_conversion_and_duplicate_identity_rejected(self):
        from tools import storage_audio
        config=storage_audio.read_json(source.ROOT/'content/storage-audio-binding.json')
        storage_audio.bindings(source.ROOT)
        for mutate in (lambda d:d.update(schema=True),lambda d:d['assets'][0].update(conversion={'output_sample_rate':22050}),lambda d:d['assets'][1].update(identity=d['assets'][0]['identity']),lambda d:d['assets'][0].update(pcm='../sound/effects/escape.pcm'),lambda d:d['assets'][0].update(source='Audio/unknown.wav')):
            d=copy.deepcopy(config);mutate(d)
            with patch.object(storage_audio,'read_json',return_value=d),self.assertRaises(ValueError):storage_audio.bindings(source.ROOT)

if __name__=='__main__':unittest.main()
