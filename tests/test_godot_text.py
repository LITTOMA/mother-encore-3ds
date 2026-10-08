import unittest

from tools.godot_text import (Document, GodotTextError, instantiate, parse, parse_project_value,
                              Vec2, Rect2, Transform2D, Color, ExtRef, SubRef, NodePathValue)

SCENE = '''[gd_scene load_steps=3 format=2]

[ext_resource path="res://child.tscn" type="PackedScene" id=1]
[ext_resource path="res://a.png" type="Texture" id=2]

[sub_resource type="RectangleShape2D" id=1]
extents = Vector2( 4, 2.5 )

[node name="Root" type="Node2D"]

[node name="Child" parent="." instance=ExtResource( 1 )]
position = Vector2( 10, -2 )

[node name="Sprite" parent="Child" index="0"]
texture = ExtResource( 2 )

[node name="Shape" type="CollisionShape2D" parent="Child"]
shape = SubResource( 1 )

[editable path="Child"]
'''

CHILD = '''[gd_scene load_steps=2 format=2]

[sub_resource type="GDScript" id=1]
script/source = "extends Node2D
"

[node name="Base" type="Node2D"]
script = SubResource( 1 )
"anims/Come In" = 1

[node name="Sprite" type="Sprite" parent="."]
offset = Vector2( 0, -8 )
'''


class GodotTextTests(unittest.TestCase):
    def test_values(self):
        doc = Document('x.tres', '[gd_resource type="Resource" format=2]\n\n[resource]\n'
                       'a = Rect2( 1, 2, 3, 4 )\nb = Transform2D( 1, 0, 0, 1, 5, 6 )\nc = Color( 1, 0.5, 0, 1 )\n'
                       'd = PoolIntArray( 1, -2, 3 )\ne = PoolVector2Array( 0, 1, 2, 3 )\nf = PoolStringArray( "a", "b\\\'c" )\n'
                       'g = NodePath("../x")\nh = { "k": [ 1, 2.5, null, true ] }\ni = "\\u00e9\\t"\n')
        r = doc.resource
        self.assertEqual(r['a'], Rect2(1, 2, 3, 4))
        self.assertEqual(r['b'], Transform2D(1, 0, 0, 1, 5, 6))
        self.assertEqual(r['c'], Color(1, 0.5, 0, 1))
        self.assertEqual(r['d'], [1, -2, 3])
        self.assertEqual(r['e'], [Vec2(0, 1), Vec2(2, 3)])
        self.assertEqual(r['f'], ['a', "b'c"])
        self.assertEqual(r['g'], NodePathValue('../x'))
        self.assertEqual(r['h'], {'k': [1, 2.5, None, True]})
        self.assertEqual(r['i'], '\u00e9\t')

    def test_instancing_merges_overrides(self):
        docs = {'main.tscn': Document('main.tscn', SCENE), 'child.tscn': Document('child.tscn', CHILD)}
        nodes = {n.path: n for n in instantiate(lambda p: docs[p], 'main.tscn')}
        self.assertEqual(set(nodes), {'.', 'Child', 'Child/Sprite', 'Child/Shape'})
        self.assertEqual(nodes['Child'].scene, 'child.tscn')
        self.assertEqual(nodes['Child'].props['position'], Vec2(10, -2))
        self.assertEqual(nodes['Child'].script, 'child.tscn#GDScript1')
        self.assertEqual(nodes['Child/Sprite'].props['offset'], Vec2(0, -8))
        self.assertEqual(nodes['Child/Sprite'].props['texture'], ExtRef(2))
        self.assertIs(nodes['Child/Sprite'].overrides['texture'], docs['main.tscn'])
        self.assertEqual(nodes['Child/Shape'].props['shape'], SubRef(1))
        self.assertEqual(docs['main.tscn'].editable, ['Child'])

    def test_rejections(self):
        bad = [
            '[gd_scene format=2]\r\n',
            '[node name="A" type="Node2D"]\n',
            '[gd_scene format=1]\n',
            '[gd_scene format=2]\n\n[node name="A" type="Node2D"]\nx = Basis( 1, 0, 0 )\n',
            '[gd_scene format=2]\n\n[node name="A" type="Node2D"]\nx = 1\nx = 2\n',
            '[gd_scene format=2]\n\n[node name="A" type="Node2D"]\nx = "open\n',
            '[gd_scene format=2]\n\n[node name="A" type="Node2D"]\nx = "\\q"\n',
            '[gd_scene format=2]\n\n[node name="A" type="Node2D"]\nx = 1 2\n',
            '[gd_scene format=2]\n\n[node name="A" type="Node2D"]\nx = inf\n',
            '[gd_scene format=2]\n\n[node name="A" type="Node2D"]\nx = PoolVector2Array( 1, 2, 3 )\n',
            '[gd_scene format=2]\n\n[unknown a=1]\n',
        ]
        for text in bad:
            with self.subTest(text=text), self.assertRaises(GodotTextError):
                Document('bad.tscn', text)
        with self.assertRaises(GodotTextError):
            Document('dup.tscn', '[gd_scene format=2]\n\n[ext_resource path="res://a" type="Texture" id=1]\n[ext_resource path="res://b" type="Texture" id=1]\n')
        orphan = Document('orphan.tscn', '[gd_scene format=2]\n\n[node name="A" type="Node2D"]\n\n[node name="B" type="Node2D" parent="Missing"]\n')
        with self.assertRaises(GodotTextError):
            instantiate(lambda p: orphan, 'orphan.tscn')
        typed = {'main.tscn': Document('main.tscn', SCENE.replace('[node name="Sprite" parent="Child" index="0"]', '[node name="Sprite" type="Sprite" parent="Child"]')),
                 'child.tscn': Document('child.tscn', CHILD)}
        with self.assertRaises(GodotTextError):
            instantiate(lambda p: typed[p], 'main.tscn')

    def test_project_value(self):
        text = '[a]\n\nx=1\n\n[locale]\n\nremaps={\n"res://t.png": PoolStringArray( "res://t_zh.png:zh_Hans_CN" )\n}\n'
        self.assertEqual(parse_project_value(text, 'locale', 'remaps'), {'res://t.png': ['res://t_zh.png:zh_Hans_CN']})
        with self.assertRaises(GodotTextError):
            parse_project_value(text, 'locale', 'missing')
        with self.assertRaises(GodotTextError):
            parse_project_value(text, 'other', 'x')


if __name__ == '__main__':
    unittest.main()
