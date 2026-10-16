import math
from types import SimpleNamespace
import unittest
from tools import podunk_field as pf


class FieldCameraTests(unittest.TestCase):
    def test_pinned_control_parent_contributes_to_area_world_position(self):
        scene = pf.Scene(pf.Sources(), pf.load_recipe())
        parent = scene.global_xf['Camareas/CamareaRect']
        shape = scene.global_xf['Camareas/CamareaRect/camarea/CollisionShape2D']
        self.assertEqual((parent.ox, parent.oy), (1952, 1064))
        self.assertEqual((shape.ox, shape.oy), (2390, 1308))

    def test_static_reference_rect_translation_composes_with_node2d(self):
        node = SimpleNamespace(type='ReferenceRect', path='rect', props=dict(margin_left=12.5, margin_top=-8, margin_right=100, margin_bottom=50))
        result = pf.compose(pf.Transform2D(2, 0, 0, 3, 7, 11), pf.local_transform(node))
        self.assertEqual(result, pf.Transform2D(2, 0, 0, 3, 32, -13))

    def test_unreviewed_control_layout_and_nonfinite_margins_rejected(self):
        for key, value in [('anchor_left', .5), ('anchor_top', 1), ('rect_rotation', 90), ('rect_scale', pf.Vec2(2, 1)), ('rect_pivot_offset', pf.Vec2(1, 0)), ('rect_position', pf.Vec2(1, 0)), ('transform', pf.Transform2D(1, 0, 0, 1, 1, 1))]:
            with self.subTest(key=key), self.assertRaises(pf.FieldError):
                pf.local_transform(SimpleNamespace(type='ReferenceRect', path='rect', props={key: value}))
        for key in ('margin_left', 'margin_top', 'margin_right', 'margin_bottom'):
            for value in (math.nan, math.inf, -math.inf, True, '12'):
                with self.subTest(key=key, value=value), self.assertRaises(pf.FieldError):
                    pf.local_transform(SimpleNamespace(type='ReferenceRect', path='rect', props={key: value}))


if __name__ == '__main__':
    unittest.main()
