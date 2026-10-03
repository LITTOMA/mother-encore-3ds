import copy
import unittest
from tools.scene_reference import external, quarantine
from tools.scene_data import validate, require_gameplay


BASE = b'''[gd_scene load_steps=2 format=2]
[ext_resource path="res://Scripts/test.gd" type="Script" id=1]
[node name="Root" type="Node2D"]
script = ExtResource( 1 )
custom_default_override = 96
[connection signal="ready" from="." to="." method="on_ready"]
'''


def document():
    return {'schema': 1, 'coverage': 'native_instantiated_data_with_raw_scene_states', 'native_compatible': False,
            'godot': {'major': 3, 'minor': 6, 'patch': 2, 'status': 'stable'},
            'resources': [{'id': 0, 'class': 'RectangleShape2D', 'path': 'res://sample.tscn::1',
                           'properties': {'extents': {'type': 'Vector2', 'x': 7, 'y': 3}}}],
            'nodes': [{'path': '.', 'class': 'Node2D', 'properties': {'script': None}},
                      {'path': 'Body', 'class': 'StaticBody2D', 'physics_shape_owners': [], 'properties': {'shape': {'type': 'ResourceReference', 'id': 0}}}],
            'scene_states': [{'source': 'res://sample.tscn', 'nodes': [{'properties': {'custom': {'type': 'int64', 'value': '96'}},
                                                                     'instance': None, 'groups': {'type': 'PoolStringArray', 'value': []}}],
                              'connections': [{'binds': {'type': 'Array', 'value': []}}]}]}


class SceneQuarantineTests(unittest.TestCase):
    def test_retains_native_bytes_custom_overrides_and_source_locations(self):
        converted, refs, scripts, signals = quarantine(BASE, 'sample.tscn')
        self.assertEqual(converted, b'[gd_scene load_steps=2 format=2]\n[node name="Root" type="Node2D"]\nscript = null\ncustom_default_override = 96\n')
        self.assertEqual(refs[0]['path'], 'Scripts/test.gd')
        self.assertEqual(scripts[0]['line'], 4)
        self.assertEqual(scripts[0]['script'], 'Scripts/test.gd')
        self.assertEqual(signals[0]['line'], 6)

    def test_quoted_space_apostrophe_and_reordered_attributes(self):
        ref = external('[ext_resource id=5 type="Texture" path="res://Graphics/Ninten\\\'s House.png"]\n')
        self.assertEqual(ref['path'], "Graphics/Ninten's House.png")

    def test_unknown_format_fields_language_utf8_are_rejected(self):
        cases = [BASE.replace(b'format=2', b'format=3'), BASE.replace(b'id=1]', b'id=1 uid=99]'),
                 BASE.replace(b'test.gd', b'test.cs'), BASE + b'\xff']
        for raw in cases:
            with self.subTest(raw=raw), self.assertRaises((ValueError, UnicodeDecodeError)):
                quarantine(raw, 'sample.tscn')

    def test_ambiguous_duplicate_or_malformed_resources_rejected(self):
        duplicate = BASE.replace(b'[node', b'[ext_resource path="res://other.gd" type="Script" id=1]\n[node', 1)
        for raw in [duplicate, BASE.replace(b'id=1]', b'id=1 id=2]'), BASE.replace(b'id=1]', b'id=0]'), BASE.replace(b'id=1]', b'id=1')]:
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                quarantine(raw, 'sample.tscn')

    def test_path_escape_noncanonical_paths_and_unknown_escapes_rejected(self):
        for path in ['../bad.gd', '/bad.gd', 'C:/bad.gd', 'folder//bad.gd', 'folder/./bad.gd', 'bad\\q.gd']:
            with self.subTest(path=path), self.assertRaises(ValueError):
                external('[ext_resource path="res://' + path + '" type="Script" id=1]')

    def test_script_values_outside_attachment_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'outside node attachment'):
            quarantine(BASE.replace(b'script = ', b'some_callback = '), 'sample.tscn')

    def test_unattached_scripts_are_not_silently_discarded(self):
        with self.assertRaisesRegex(ValueError, 'Unattached script'):
            quarantine(BASE.replace(b'script = ExtResource( 1 )\n', b''), 'sample.tscn')

    def test_embedded_script_preserved_and_never_loaded(self):
        raw = b'''[gd_scene load_steps=2 format=2]
[sub_resource type="GDScript" id=7]
script/source = "extends Node2D
# Header-looking string content is not a real resource header.
[node false_header]
"
[node name="Root" type="Node2D"]
script = SubResource( 7 )
custom = 1
'''
        converted, refs, attachments, signals = quarantine(raw, 'embedded.tscn')
        self.assertEqual(len(attachments), 1)
        self.assertIn('[node false_header]', attachments[0]['embedded_script']['embedded_declaration'])
        self.assertNotIn(b'script/source', converted)
        self.assertIn(b'custom = 1', converted)
        self.assertFalse(refs or signals)

    def test_unknown_embedded_language_and_unterminated_strings_rejected(self):
        for raw in [b'[gd_scene format=2]\n[sub_resource type="VisualScript" id=1]\n',
                    b'[gd_scene format=2]\n[node name="unterminated]\n']:
            with self.assertRaises(ValueError):
                quarantine(raw, 'sample.tscn')


class SceneDataValidationTests(unittest.TestCase):
    def test_valid_resource_graph_and_explicit_gameplay_rejection(self):
        validate(document())
        with self.assertRaisesRegex(ValueError, 'not playable content'):
            require_gameplay(document())

    def test_schema_engine_and_approval_tampering_rejected(self):
        for field, replacement in [('schema', 2), ('coverage', 'raw'), ('native_compatible', True), ('nodes', []), ('scene_states', [])]:
            d = document()
            d[field] = replacement
            with self.subTest(field=field), self.assertRaises(ValueError):
                validate(d)
        d = document()
        d['godot']['patch'] = 3
        with self.assertRaises(ValueError):
            validate(d)

    def test_dangling_resources_and_duplicate_nodes_rejected(self):
        for change in ('reference', 'graph', 'boolean_id', 'duplicate', 'parent'):
            d = document()
            if change == 'reference': d['nodes'][1]['properties']['shape']['id'] = 1
            if change == 'graph': d['resources'][0]['id'] = 5
            if change == 'boolean_id': d['resources'][0]['id'] = False
            if change == 'duplicate': d['nodes'].append(copy.deepcopy(d['nodes'][1]))
            if change == 'parent': d['nodes'][1]['path'] = 'Absent/Child'
            with self.subTest(change=change), self.assertRaises(ValueError):
                validate(d)

    def test_unknown_variants_nonfinite_values_and_wrong_fields_rejected(self):
        cases = [{'type': 'Transform3D'}, float('nan'), float('inf'),
                 {'type': 'Vector2', 'x': 1, 'y': False}, {'type': 'Vector2', 'x': 1, 'y': 2, 'z': 0},
                 {'type': 'Rect2', 'position': None, 'size': None}]
        for variant in cases:
            d = document()
            d['nodes'][0]['properties']['test'] = variant
            with self.subTest(variant=variant), self.assertRaises(ValueError):
                validate(d)

    def test_integer_precision_is_explicit_and_checked(self):
        for text in ['9223372036854775807', '-9223372036854775808']:
            d = document()
            d['nodes'][0]['properties']['test'] = {'type': 'int64', 'value': text}
            validate(d)
        for text in ['9223372036854775808', '-9223372036854775809', '01', '-0', '1.0', '1e5', 1]:
            d = document()
            d['nodes'][0]['properties']['test'] = {'type': 'int64', 'value': text}
            with self.subTest(text=text), self.assertRaises(ValueError):
                validate(d)

    def test_typed_arrays_and_node_references_checked(self):
        for value in [{'type': 'PoolIntArray', 'value': [1]}, {'type': 'PoolByteArray', 'value': [{'type': 'int64', 'value': '256'}]},
                      {'type': 'PoolStringArray', 'value': [False]}, {'type': 'PoolVector2Array', 'value': ['point']},
                      {'type': 'Dictionary', 'pairs': [[1]]}, {'type': 'NodeReference', 'path': 'Missing'}]:
            d = document()
            d['nodes'][0]['properties']['test'] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate(d)

    def test_missing_and_unknown_tilemap_cell_data_rejected(self):
        d = document()
        d['nodes'][1]['class'] = 'TileMap'
        with self.assertRaises(ValueError): validate(d)
        d['nodes'][1]['cells'] = [{'tile': 1}]
        with self.assertRaises(ValueError): validate(d)

    def test_collision_owners_and_flags_are_not_discarded(self):
        d = document()
        del d['nodes'][1]['physics_shape_owners']
        with self.assertRaises(ValueError): validate(d)
        d['nodes'][1]['physics_shape_owners'] = [{'disabled': True}]
        with self.assertRaises(ValueError): validate(d)


if __name__ == '__main__':
    unittest.main()
