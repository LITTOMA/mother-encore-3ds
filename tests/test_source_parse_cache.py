import unittest
from tools import godot_text
from tests.source_parse_cache import cached_source_parser

SOURCE = '[gd_scene format=2]\n\n[node name="Root" type="Node"]\nvalue = 1\n'


class SourceParseCacheTests(unittest.TestCase):
    def test_exact_text_key_and_detached_callers(self):
        original = godot_text.parse
        with cached_source_parser(minimum_bytes=0) as info:
            first = godot_text.parse(SOURCE)
            expected = original(SOURCE)
            first[1].props['value'] = 99
            self.assertEqual(godot_text.parse(SOURCE), expected)
            changed = SOURCE.replace('value = 1', 'value = 2')
            self.assertEqual(godot_text.parse(changed), original(changed))
            self.assertEqual((info().hits, info().misses), (1, 2))
        self.assertIs(godot_text.parse, original)
        self.assertEqual(info().currsize, 0)

    def test_unknown_input_is_rejected_and_failure_is_not_cached(self):
        with cached_source_parser(minimum_bytes=0) as info:
            for _ in range(2):
                with self.assertRaises(godot_text.GodotTextError):
                    godot_text.parse(SOURCE + 'unknown = Vector3(1, 2, 3)\n')
            self.assertEqual((info().hits, info().misses, info().currsize), (0, 2, 0))

    def test_exception_cleanup_and_bounded_lifetime(self):
        original = godot_text.parse
        with self.assertRaisesRegex(RuntimeError, 'probe'):
            with cached_source_parser(minimum_bytes=0) as info:
                for n in range(20):
                    godot_text.parse(SOURCE.replace('value = 1', 'value = ' + str(n)))
                self.assertEqual((info().maxsize, info().currsize), (16, 16))
                raise RuntimeError('probe')
        self.assertIs(godot_text.parse, original)
        self.assertEqual(info().currsize, 0)

    def test_small_text_does_not_evict_large_source(self):
        large = SOURCE + 'label = "' + 'x' * 32768 + '"\n'
        with cached_source_parser() as info:
            expected = godot_text.parse(large)
            for n in range(20):
                godot_text.parse(SOURCE.replace('value = 1', 'value = ' + str(n)))
            self.assertEqual(godot_text.parse(large), expected)
            self.assertEqual((info().hits, info().misses, info().currsize), (1, 1, 1))


if __name__ == '__main__':
    unittest.main()
