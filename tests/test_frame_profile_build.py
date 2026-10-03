from pathlib import Path
import unittest

from tools import build_continue_text_qa as qa


ROOT = Path(__file__).resolve().parents[1]


class FrameProfileBuildTests(unittest.TestCase):
    def test_default_remains_unprofiled(self):
        config = qa.configuration()
        self.assertEqual(config['defines'], ['-DENCORE_TEXT_QA'])
        self.assertEqual(config['output_name'], 'continue-text-qa')
        self.assertEqual(config['log_path'], 'sdmc:/encore-continue-character-qa.log')

    def test_profile_is_separate_and_explicit(self):
        config = qa.configuration(True)
        self.assertEqual(config['defines'], ['-DENCORE_TEXT_QA', '-DENCORE_FRAME_PROFILE'])
        self.assertEqual(config['output_name'], 'frame-profile-qa')
        self.assertEqual(config['log_path'], 'sdmc:/encore-frame-profile-qa.log')
        for enabled in (False, True):
            command = qa.compile_command(Path('/project'), Path('/output'), Path('/sdk'), Path('/arm'), enabled)
            self.assertEqual('-DENCORE_FRAME_PROFILE' in command, enabled)
            self.assertIn('-ffp-contract=off', command)
            self.assertNotIn('-ffast-math', command)
            self.assertEqual(command[-4:], ['-c', '/output/main.cpp', '-o', '/output/main.o'])

    def test_source_instrumentation_preserves_input_and_game_code(self):
        source = (ROOT / 'platform/ctr/main.cpp').read_text()
        ordinary = qa.instrument_source(source)
        profile = qa.instrument_source(source, True)
        self.assertEqual(ordinary.replace('encore-continue-character-qa.log', 'encore-frame-profile-qa.log'), profile)
        self.assertIn('FRAME_PROFILE sequence=', profile)
        self.assertIn('BACKGROUND_FRAME t=', profile)
        self.assertIn('qa_record(down);PROFILE_MARK(6);PROFILE_FINISH();', profile)
        # Instrumentation only inserts read-only lines before the error log and
        # changes the QA state key/log identity; it never injects controls.
        self.assertEqual(source.count('hidScanInput()'), profile.count('hidScanInput()'))
        self.assertEqual(source.count('shader_time+=dt'), profile.count('shader_time+=dt'))
        self.assertIn('#define PROFILE_BACKGROUND_DONE() ((void)0)', source)

    def test_unreviewed_instrumentation_boundary_rejected(self):
        for source in ('', 'if (!round_error.empty()) {}'):
            with self.assertRaisesRegex(ValueError, 'exactly one'):
                qa.instrument_source(source, True)
        source = (ROOT / 'platform/ctr/main.cpp').read_text()
        with self.assertRaisesRegex(ValueError, 'exactly one'):
            qa.instrument_source(source + source, True)
        for anchor in ('sdmc:/encore-native-qa.log', 'ENCORE_TEXT_QA v1 no image capture',
                       'const uint64_t state=uint64_t(gameplay_scene->world.stage())'):
            with self.subTest(anchor=anchor), self.assertRaisesRegex(ValueError, 'replacement anchor'):
                qa.instrument_source(source.replace(anchor, 'unreviewed replacement'), True)


if __name__ == '__main__':
    unittest.main()
