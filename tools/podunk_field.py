#!/usr/bin/env python3
"""Compile the pinned Podunk exterior into checked native field resources.

Build-time adapter only. content/podunk-field.json holds reviewed policy (which
source scene classes are drawn, collide, route, or remain explicit development
boundaries). TileMap cells, tile shapes, sprites and doors are read directly
from the pinned upstream scene every compile; no intermediate dump is stored.
Every flattened source node must match exactly one policy class or the compile
fails. Outputs: data/podunk.encmap, data/podunk.encroom, data/world.enclinks and
packed RGBA8 atlases under graphics/world/podunk.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import re
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'tools'))
from tools.godot_text import (Document, GodotTextError, instantiate, owner_of, parse_project_value,  # noqa: E402
                              Vec2, Rect2, Transform2D, ExtRef, SubRef, NodePathValue)

PIN = '7d9246600fffe518408f5830d4848635019005a3'
RECIPE = ROOT / 'content/podunk-field.json'
NONE = 0xffffffff
NODE2D = {'Node2D', 'Sprite', 'Position2D', 'YSort', 'TileMap', 'Area2D', 'StaticBody2D', 'KinematicBody2D',
          'CollisionShape2D', 'CollisionPolygon2D', 'VisibilityNotifier2D', 'VisibilityEnabler2D', 'AnimatedSprite',
          'RayCast2D', 'Camera2D', 'AudioStreamPlayer2D'}
CANVAS_CONTROL = {'TextureRect', 'HBoxContainer', 'Label', 'ReferenceRect'}
STRUCTURAL = {'Node2D', 'YSort', 'Node', 'Position2D'}


class FieldError(ValueError):
    pass


def require(ok, message):
    if not ok:
        raise FieldError(message)


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


# ---------------------------------------------------------------- sources
class Sources:
    """Pinned upstream access with inventory digests (see Extractor)."""

    def __init__(self, root=ROOT):
        from tools.extract_battle_entry import Extractor
        self.extractor = Extractor(root)
        self.docs = {}

    def text(self, path):
        return self.extractor.text(path)

    def data(self, path):
        return self.extractor.data(path)

    def doc(self, path):
        if path not in self.docs:
            self.docs[path] = Document(path, self.text(path))
        return self.docs[path]

    def digests(self):
        return dict(sorted(self.extractor.sources.items()))


# ---------------------------------------------------------------- source RNG
MULTIPLIER = 6364136223846793005
INCREMENT = ((1442695040888963407 << 1) | 1) & 0xffffffffffffffff


class SourceRandom:
    """Python mirror of runtime/source_random.cpp (Godot 3.6.2 global RNG)."""

    def __init__(self, seed_value=0):
        self.seed(seed_value)

    def seed(self, value):
        self.state = 0
        self.randi()
        self.state = (self.state + value) & 0xffffffffffffffff
        self.randi()

    def randi(self):
        old = self.state
        self.state = (old * MULTIPLIER + INCREMENT) & 0xffffffffffffffff
        mixed = (((old >> 18) ^ old) >> 27) & 0xffffffff
        rotation = old >> 59
        return ((mixed >> rotation) | (mixed << ((-rotation) & 31))) & 0xffffffff


def godot_string_hash(text):
    """String::hash() of Godot 3.6.2: djb2 over UTF-32 code points."""
    value = 5381
    for ch in text:
        value = ((value << 5) + value + ord(ch)) & 0xffffffff
    return value


# ---------------------------------------------------------------- transforms
def local_transform(node):
    props = node.props
    if node.type not in NODE2D:
        return Transform2D(1, 0, 0, 1, 0, 0)
    for key in ('transform', 'skew', 'global_position', 'rotation_degrees'):
        require(key not in props, 'Unreviewed transform property %s on %s' % (key, node.path))
    pos = props.get('position', Vec2(0, 0))
    rot = float(props.get('rotation', 0.0))
    scale = props.get('scale', Vec2(1, 1))
    require(isinstance(pos, Vec2) and isinstance(scale, Vec2), 'Bad transform on ' + node.path)
    c, s = math.cos(rot), math.sin(rot)
    if rot == 0:
        c, s = 1.0, 0.0
    return Transform2D(c * scale.x, s * scale.x, -s * scale.y, c * scale.y, pos.x, pos.y)


def compose(a, b):
    """a * b for Godot Transform2D (columns x, y, origin)."""
    return Transform2D(a.xx * b.xx + a.yx * b.xy, a.xy * b.xx + a.yy * b.xy,
                       a.xx * b.yx + a.yx * b.yy, a.xy * b.yx + a.yy * b.yy,
                       a.xx * b.ox + a.yx * b.oy + a.ox, a.xy * b.ox + a.yy * b.oy + a.oy)


def parent_path(path):
    if path == '.':
        return None
    return path.rsplit('/', 1)[0] if '/' in path else '.'


class Scene:
    def __init__(self, sources, recipe):
        self.sources = sources
        self.recipe = recipe
        self.source = recipe['scene']['source']
        self.nodes = instantiate(sources.doc, self.source)
        self.by_path = {n.path: n for n in self.nodes}
        self.index = {n.path: i for i, n in enumerate(self.nodes)}
        self.children = {n.path: [] for n in self.nodes}
        for n in self.nodes:
            p = parent_path(n.path)
            if p is not None:
                self.children[p].append(n.path)
        self.global_xf = {}
        for n in self.nodes:
            p = parent_path(n.path)
            base = self.global_xf[p] if p is not None else Transform2D(1, 0, 0, 1, 0, 0)
            if n.type not in NODE2D and n.type not in CANVAS_CONTROL and p is not None:
                # Plain Node breaks the CanvasItem chain; its children restart at the canvas.
                self.global_xf[n.path] = Transform2D(1, 0, 0, 1, 0, 0)
                continue
            self.global_xf[n.path] = compose(base, local_transform(n))
        self.category = {}
        self.owner_instance = {}
        self._classify()

    def ancestors(self, path):
        chain = []
        while path is not None:
            chain.append(path)
            path = parent_path(path)
        return list(reversed(chain))

    def instance_root(self, node):
        return node.scene is not None and node.path != '.'

    def _classify(self):
        cats = self.recipe['categories']
        for n in self.nodes:
            owner = None
            for a in self.ancestors(n.path):
                an = self.by_path[a]
                if self.instance_root(an):
                    require(an.scene in cats, 'Unclassified instanced scene %s at %s' % (an.scene, an.path))
                    if cats[an.scene] != 'flag_landmark':
                        owner = an
                        break
            if owner is not None:
                self.category[n.path] = cats[owner.scene]
                self.owner_instance[n.path] = owner.path
                continue
            if n.path == '.':
                self.category[n.path] = 'root'
            elif self.instance_root(n):
                self.category[n.path] = 'flag_landmark'
            elif n.type == 'TileMap':
                self.category[n.path] = 'tilemap'
            elif n.type in ('StaticBody2D', 'CollisionShape2D', 'CollisionPolygon2D'):
                self.category[n.path] = 'static_body'
            elif n.type in STRUCTURAL:
                self.category[n.path] = 'structure'
            elif n.type == 'ReferenceRect':
                self.category[n.path] = 'camera_rect'
            elif n.type in ('Camera2D', 'AnimationPlayer') and not n.props:
                # An empty camera is never current and an empty player has no animation to autoplay.
                self.category[n.path] = 'structure'
            else:
                raise FieldError('Unclassified scene node %s (%s)' % (n.path, n.type))

    def conditions(self, path):
        """FlagLandmark ancestors as (flag, expected) pairs; all must hold."""
        out = []
        for a in self.ancestors(path):
            node = self.by_path[a]
            if self.category.get(a) == 'flag_landmark' and self.instance_root(node):
                require(node.props.get('delete_if_hidden', True) is True, 'Unreviewed FlagLandmark visibility mode at ' + a)
                appear, disappear = node.props.get('appear_flag', ''), node.props.get('disappear_flag', '')
                require(isinstance(appear, str) and isinstance(disappear, str), 'Bad FlagLandmark flags at ' + a)
                if appear:
                    out.append((appear, True))
                if disappear:
                    out.append((disappear, False))
            if a != path and node.type == 'TileMap' and 'disappear_flag' in node.props:
                raise FieldError('Unreviewed nested flag TileMap at ' + a)
        node = self.by_path[path]
        if node.type == 'TileMap' and node.script:
            require(node.script == 'Scripts/Main/Flag Landmarks.gd', 'Unreviewed TileMap script at ' + path)
            appear, disappear = node.props.get('appear_flag', ''), node.props.get('disappear_flag', '')
            if appear:
                out.append((appear, True))
            if disappear:
                out.append((disappear, False))
        return out

    def visible(self, path):
        for a in self.ancestors(path):
            if self.by_path[a].props.get('visible', True) is False:
                return False
        return True


def load_recipe(path=RECIPE):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate recipe field: ' + key)
            result[key] = value
        return result
    recipe = json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)
    require(recipe.get('schema') == 1 and recipe.get('kind') == 'encore.podunk-field.recipe', 'Unsupported Podunk recipe')
    require(recipe.get('commit') == PIN, 'Podunk recipe source pin mismatch')
    expected = {'schema', 'kind', 'commit', 'scope', 'scene', 'house', 'player', 'tileset', 'door', 'routes', 'locales',
                'categories', 'prop_sprites', 'labels', 'atlas_directory', 'outputs'}
    require(set(recipe) == expected, 'Unknown/missing Podunk recipe fields')
    valid = {'door', 'openable_door', 'grass', 'prop', 'flag_landmark', 'camera_area', 'boundary', 'notice'}
    require(all(v in valid for v in recipe['categories'].values()), 'Unknown recipe category')
    require(set(recipe['prop_sprites']) == {k for k, v in recipe['categories'].items() if v == 'prop'}, 'Prop sprite policy coverage')
    return recipe


# ---------------------------------------------------------------- convex decomposition
# Port of Godot 3.6.2 Geometry::decompose_polygon_in_convex, which calls
# thirdparty/misc/triangulator.cpp (polypartition, Copyright (C) 2011 Ivan
# Fratric, MIT license) TriangulatorPartition::ConvexPartition_HM. real_t is
# binary32 in official 3.6.2 builds; arithmetic is rounded per operation.
def _is_convex(p1, p2, p3):
    return f32(f32(f32(p3[1] - p1[1]) * f32(p2[0] - p1[0])) - f32(f32(p3[0] - p1[0]) * f32(p2[1] - p1[1]))) > 0


def _is_reflex(p1, p2, p3):
    return f32(f32(f32(p3[1] - p1[1]) * f32(p2[0] - p1[0])) - f32(f32(p3[0] - p1[0]) * f32(p2[1] - p1[1]))) < 0


def _is_inside(p1, p2, p3, p):
    return not (_is_convex(p1, p, p2) or _is_convex(p2, p, p3) or _is_convex(p3, p, p1))


def _normalize(v):
    n = f32(math.sqrt(f32(f32(v[0] * v[0]) + f32(v[1] * v[1]))))
    return (f32(v[0] / n), f32(v[1] / n)) if n != 0 else (0.0, 0.0)


def _orientation(points):
    area = 0.0
    for i in range(len(points)):
        a, b = points[i], points[(i + 1) % len(points)]
        area = f32(area + f32(f32(a[0] * b[1]) - f32(a[1] * b[0])))
    return 1 if area > 0 else (-1 if area < 0 else 0)


def _triangulate_ec(points):
    n = len(points)
    if n < 3:
        return None
    if n == 3:
        return [list(points)]
    verts = [dict(p=points[i], active=True, prev=(i - 1) % n, next=(i + 1) % n, convex=False, ear=False, angle=0.0) for i in range(n)]

    def update(i):
        v = verts[i]
        p1, p2, p3 = verts[v['prev']]['p'], v['p'], verts[v['next']]['p']
        v['convex'] = _is_convex(p1, p2, p3)
        a, b = _normalize((p1[0] - p2[0], p1[1] - p2[1])), _normalize((p3[0] - p2[0], p3[1] - p2[1]))
        v['angle'] = f32(f32(a[0] * b[0]) + f32(a[1] * b[1]))
        if v['convex']:
            v['ear'] = True
            for other in verts:
                q = other['p']
                if q == p2 or q == p1 or q == p3:
                    continue
                if _is_inside(p1, p2, p3, q):
                    v['ear'] = False
                    break
        else:
            v['ear'] = False
    for i in range(n):
        update(i)
    triangles = []
    for step in range(n - 3):
        ear = None
        for j in range(n):
            v = verts[j]
            if not v['active'] or not v['ear']:
                continue
            if ear is None or v['angle'] > verts[ear]['angle']:
                ear = j
        if ear is None:
            return None
        v = verts[ear]
        triangles.append([verts[v['prev']]['p'], v['p'], verts[v['next']]['p']])
        v['active'] = False
        verts[v['prev']]['next'] = v['next']
        verts[v['next']]['prev'] = v['prev']
        if step == n - 4:
            break
        update(v['prev'])
        update(v['next'])
    for v in verts:
        if v['active']:
            triangles.append([verts[v['prev']]['p'], v['p'], verts[v['next']]['p']])
            break
    return triangles


def decompose_polygon_in_convex(points):
    """Convex parts exactly as Godot's runtime TileSet/CollisionPolygon2D decomposition."""
    poly = [(f32(p[0]), f32(p[1])) for p in points]
    if _orientation(poly) == -1:
        poly = list(reversed(poly))
    n = len(poly)
    if not any(_is_reflex(poly[(i - 1) % n], poly[i], poly[(i + 1) % n]) for i in range(n)):
        return [poly]
    triangles = _triangulate_ec(poly)
    if triangles is None:
        return []
    i1 = 0
    while i1 < len(triangles):
        poly1 = triangles[i1]
        i11 = 0
        while i11 < len(poly1):
            d1 = poly1[i11]
            i12 = (i11 + 1) % len(poly1)
            d2 = poly1[i12]
            found = None
            for i2 in range(i1 + 1, len(triangles)):
                poly2 = triangles[i2]
                for i21 in range(len(poly2)):
                    if d2 != poly2[i21]:
                        continue
                    i22 = (i21 + 1) % len(poly2)
                    if d1 != poly2[i22]:
                        continue
                    found = (i2, i21, i22)
                    break
                if found:
                    break
            if not found:
                i11 += 1
                continue
            i2, i21, i22 = found
            poly2 = triangles[i2]
            p2 = poly1[i11]
            p1 = poly1[len(poly1) - 1 if i11 == 0 else i11 - 1]
            p3 = poly2[0 if i22 == len(poly2) - 1 else i22 + 1]
            if not _is_convex(p1, p2, p3):
                i11 += 1
                continue
            p2 = poly1[i12]
            p3 = poly1[0 if i12 == len(poly1) - 1 else i12 + 1]
            p1 = poly2[len(poly2) - 1 if i21 == 0 else i21 - 1]
            if not _is_convex(p1, p2, p3):
                i11 += 1
                continue
            merged = []
            j = i12
            while j != i11:
                merged.append(poly1[j])
                j = (j + 1) % len(poly1)
            j = i22
            while j != i21:
                merged.append(poly2[j])
                j = (j + 1) % len(poly2)
            del triangles[i2]
            triangles[i1] = merged
            poly1 = merged
            i11 = 0
        i1 += 1
    return triangles


# ---------------------------------------------------------------- textures
class Images:
    """Source image regions, packed later into RGBA8 atlas pages."""

    def __init__(self, sources):
        self.sources = sources
        self.keys = []
        self.index = {}
        self.sizes = {}

    def size(self, path):
        if path not in self.sizes:
            raw = self.sources.data(path)
            require(raw[:8] == b'\x89PNG\r\n\x1a\n', 'Expected PNG source: ' + path)
            self.sizes[path] = struct.unpack('>II', raw[16:24])
        return self.sizes[path]

    def add(self, path, x, y, w, h):
        width, height = self.size(path)
        require(w > 0 and h > 0 and 0 <= x and 0 <= y and x + w <= width and y + h <= height,
                'Image region outside source %s' % path)
        key = (path, int(x), int(y), int(w), int(h))
        require(key[1:] == (x, y, w, h), 'Nonintegral image region in ' + path)
        if key not in self.index:
            self.index[key] = len(self.keys)
            self.keys.append(key)
        return self.index[key]


class Pictures:
    """Drawable pictures: one or more frames, optional per-locale variants."""

    def __init__(self, images):
        self.images = images
        self.rows = []
        self.index = {}

    def add(self, frames, fps=0.0, variants=()):
        key = (tuple(frames), float(fps), tuple(variants))
        if key not in self.index:
            require(frames and len(frames) <= 64, 'Unsupported picture frame count')
            self.index[key] = len(self.rows)
            self.rows.append(dict(frames=list(frames), fps=float(fps), variants=list(variants)))
        return self.index[key]


class Textures:
    """Texture resources referenced by the TileSet and sprites."""

    def __init__(self, sources, recipe, images):
        self.sources = sources
        self.images = images
        project = sources.text('project.godot')
        remaps = parse_project_value(project, 'locale', 'translation_remaps')
        require(isinstance(remaps, dict), 'Unreviewed translation remap table')
        self.remaps = remaps
        self.locales = recipe['locales']
        require(self.locales and self.locales[0] == 'en', 'Default locale must be first')

    def frames_of(self, path):
        """Return [(png path, fps)] frames of a Texture or AnimatedTexture resource."""
        if path.endswith('.png'):
            return [path], 0.0
        require(path.endswith('.tres'), 'Unreviewed texture resource ' + path)
        doc = self.sources.doc(path)
        require(doc.header.attrs.get('type') == 'AnimatedTexture' and doc.resource is not None, 'Unreviewed texture type ' + path)
        props = doc.resource
        count = props.get('frames')
        require(isinstance(count, int) and 1 <= count <= 16, 'Bad AnimatedTexture frame count')
        allowed = {'frames', 'fps', 'oneshot', 'pause'} | {'frame_%d/texture' % i for i in range(count)} | \
            {'frame_%d/delay_sec' % i for i in range(count)}
        require(set(props) <= allowed and not props.get('oneshot', False) and not props.get('pause', False), 'Unreviewed AnimatedTexture state')
        frames = []
        for i in range(count):
            require(props.get('frame_%d/delay_sec' % i, 0.0) == 0.0, 'Unreviewed AnimatedTexture frame delay')
            frames.append(doc.ext_path(props['frame_%d/texture' % i], 'Texture'))
        return frames, float(props.get('fps', 4.0))

    def locale_path(self, path, locale):
        """ResourceLoader translation remap: exact locale, else first same-language entry."""
        entries = self.remaps.get('res://' + path)
        if entries is None or locale == 'en':
            return path
        language = locale.split('_')[0]
        chosen = path
        near = False
        for entry in entries:
            split = entry.rfind(':')
            if split < 0:
                continue
            target, code = entry[:split], entry[split + 1:].strip()
            require(target.startswith('res://'), 'Unreviewed remap target')
            if code == locale:
                return target[6:]
            if near:
                continue
            if code.split('_')[0] == language:
                chosen, near = target[6:], True
        return chosen

    def picture(self, pictures, path, x, y, w, h):
        """Picture for a region of a texture resource, with frames and locale variants."""
        frames, fps = self.frames_of(path)
        base = [self.images.add(f, x, y, w, h) for f in frames]
        variants = []
        for locale in self.locales[1:]:
            mapped = [self.locale_path(f, locale) for f in frames]
            if mapped != frames:
                variant = pictures.add([self.images.add(f, x, y, w, h) for f in mapped], fps)
                variants.append((locale, variant))
        return pictures.add(base, fps, variants)


# ---------------------------------------------------------------- tileset
TILE_KEYS = {'name', 'texture', 'tex_offset', 'modulate', 'region', 'tile_mode', 'occluder_offset', 'navigation_offset',
             'shape_offset', 'shape_transform', 'shape_one_way', 'shape_one_way_margin', 'shapes', 'z_index',
             'autotile/icon_coordinate', 'autotile/navpoly_map', 'autotile/occluder_map', 'autotile/priority_map',
             'autotile/spacing', 'autotile/tile_size', 'autotile/z_index_map', 'autotile/bitmask_flags',
             'autotile/bitmask_mode', 'autotile/fallback_mode', 'shape'}


class TileSet:
    def __init__(self, sources, path):
        self.doc = sources.doc(path)
        require(self.doc.header.attrs.get('type') == 'TileSet' and self.doc.resource is not None, 'Not a TileSet: ' + path)
        tiles = {}
        for key, value in self.doc.resource.items():
            m = re.fullmatch(r'(\d+)/(.+)', key)
            require(m is not None, 'Unreviewed TileSet property ' + key)
            tiles.setdefault(int(m[1]), {})[m[2]] = value
        for tid, t in tiles.items():
            require(set(t) <= TILE_KEYS, 'Unreviewed tile property in tile %d: %s' % (tid, sorted(set(t) - TILE_KEYS)))
            require(t.get('tile_mode', 0) in (0, 1, 2), 'Unreviewed tile mode')
            require(t.get('modulate', None) in (None,) or (t['modulate'].r, t['modulate'].g, t['modulate'].b, t['modulate'].a) == (1, 1, 1, 1), 'Unreviewed tile modulate')
            require(not t.get('autotile/z_index_map'), 'Unreviewed autotile z map')
            for s in t.get('shapes', []):
                require(set(s) <= {'autotile_coord', 'one_way', 'one_way_margin', 'shape', 'shape_transform'} and not s.get('one_way', False), 'Unreviewed tile shape data')
        self.tiles = tiles

    def region(self, tid, coord):
        t = self.tiles[tid]
        r = t.get('region', Rect2(0, 0, 0, 0))
        if t.get('tile_mode', 0) in (1, 2):
            size = t['autotile/tile_size']
            spacing = t.get('autotile/spacing', 0)
            require(isinstance(spacing, int), 'Bad autotile spacing')
            return Rect2(r.x + (size.x + spacing) * coord[0], r.y + (size.y + spacing) * coord[1], size.x, size.y)
        return r

    def shapes(self, tid, coord):
        t = self.tiles[tid]
        out = []
        for s in t.get('shapes', []):
            if t.get('tile_mode', 0) != 0:
                ac = s.get('autotile_coord', Vec2(0, 0))
                if (int(ac.x), int(ac.y)) != coord:
                    continue
            xf = s.get('shape_transform', Transform2D(1, 0, 0, 1, 0, 0))
            require((xf.xx, xf.xy, xf.yx, xf.yy) == (1, 0, 0, 1), 'Unreviewed rotated/scaled tile shape')
            kind = self.doc.sub_type(s['shape'])
            props = self.doc.sub_resource(s['shape'])
            require(set(props) <= {'points', 'segments', 'custom_solver_bias'} and not props.get('custom_solver_bias'), 'Unreviewed tile shape resource')
            if kind == 'ConvexPolygonShape2D':
                # TileSet::_decompose_convex_shape: more than one part replaces the shape.
                parts = decompose_polygon_in_convex([(p.x, p.y) for p in props['points']])
                if len(parts) > 1:
                    for part in parts:
                        out.append(('convex', [Vec2(x, y) for x, y in part], Vec2(xf.ox, xf.oy)))
                else:
                    out.append(('convex', props['points'], Vec2(xf.ox, xf.oy)))
            elif kind == 'ConcavePolygonShape2D':
                seg = props['segments']
                require(len(seg) % 2 == 0 and seg, 'Bad concave segments')
                out.append(('concave', seg, Vec2(xf.ox, xf.oy)))
            else:
                raise FieldError('Unreviewed tile shape type ' + kind)
        return out


TILEMAP_KEYS = {'tile_set', 'cell_size', 'collision_layer', 'collision_mask', 'format', 'tile_data', 'show_collision',
                'cell_custom_transform', 'cell_y_sort', 'cell_tile_origin', 'position', 'show_behind_parent', 'script',
                'disappear_flag', 'appear_flag'}


def tilemap_policy(node):
    require(set(node.props) <= TILEMAP_KEYS, 'Unreviewed TileMap property on %s: %s' % (node.path, sorted(set(node.props) - TILEMAP_KEYS)))
    require(node.props.get('format', 1) == 1, 'Unreviewed TileMap format')
    if node.props.get('tile_data'):
        size = node.props.get('cell_size', Vec2(64, 64))
        require(size == Vec2(16, 16), 'Unreviewed TileMap cell size on ' + node.path)
    origin = node.props.get('cell_tile_origin', 0)
    require(origin in (0, 2), 'Unreviewed tile origin on ' + node.path)
    return origin


def decode_cells(node):
    data = node.props.get('tile_data', [])
    require(len(data) % 3 == 0, 'Bad tile_data on ' + node.path)
    cells = []
    for i in range(0, len(data), 3):
        packed, raw, coord = data[i] & 0xffffffff, data[i + 1] & 0xffffffff, data[i + 2] & 0xffffffff
        x, y = packed & 0xffff, packed >> 16
        x, y = x - 0x10000 if x >= 0x8000 else x, y - 0x10000 if y >= 0x8000 else y
        ax, ay = coord & 0xffff, coord >> 16
        flags = raw >> 29
        require(not flags & 4, 'Transposed tile cells are not reviewed (%s)' % node.path)
        cells.append((x, y, raw & ((1 << 29) - 1), flags & 1, flags >> 1 & 1, (ax, ay), i // 3))
    keys = [(c[0], c[1]) for c in cells]
    require(len(set(keys)) == len(keys), 'Duplicate TileMap cell on ' + node.path)
    return cells


# ---------------------------------------------------------------- compile model
class Model:
    def __init__(self, recipe, sources=None):
        self.recipe = recipe
        self.sources = sources or Sources()
        self.scene = Scene(self.sources, recipe)
        self.images = Images(self.sources)
        self.pictures = Pictures(self.images)
        self.textures = Textures(self.sources, recipe, self.images)
        self.tileset = TileSet(self.sources, recipe['tileset'])
        self.strings = bytearray()
        self.string_index = {}
        self.conditions = []
        self.condition_index = {}
        self.tile_rows = []
        self.tile_index = {}
        self.shape_rows = []
        self.points = []
        self.layers = []
        self.chunks = []
        self.cells = []
        self.sprites = []
        self.items = []
        self.groups = []
        self.openables = []
        self.doors = []
        self.boundaries = []
        self.notices = []
        self.cameras = []
        self.bodies = []
        self.undefined_cells = 0
        player = recipe['player']
        self.player_layer, self.player_mask = player['collision_layer'], player['collision_mask']
        self.layer_by_path = {}
        self.order = 0
        self._build()

    # strings / conditions
    def string(self, text):
        raw = text.encode('utf-8')
        require(b'\0' not in raw and len(raw) <= 4096, 'Invalid field string')
        if not raw:
            return (0, 0)
        if text not in self.string_index:
            self.string_index[text] = (len(self.strings), len(raw))
            self.strings += raw
        return self.string_index[text]

    def condition_span(self, pairs):
        if not pairs:
            return (0, 0)
        key = tuple(pairs)
        if key not in self.condition_index:
            first = len(self.conditions)
            for flag, value in pairs:
                self.conditions.append((self.string(flag), bool(value)))
            self.condition_index[key] = (first, len(pairs))
        return self.condition_index[key]

    def collides(self, layer, mask):
        return bool(layer & self.player_mask) or bool(mask & self.player_layer)

    # tiles
    def tile(self, tid, coord, flip_h, flip_v):
        key = (tid, coord, flip_h, flip_v)
        if key in self.tile_index:
            return self.tile_index[key]
        t = self.tileset.tiles[tid]
        tex_path = self.tileset.doc.ext_path(t['texture'], 'Texture')
        r = self.tileset.region(tid, coord)
        if (r.w, r.h) == (0, 0):
            frames, _ = self.textures.frames_of(tex_path)
            w, h = self.images.size(frames[0])
            r = Rect2(0, 0, w, h)
        picture = self.textures.picture(self.pictures, tex_path, r.x, r.y, r.w, r.h)
        ofs = t.get('tex_offset', Vec2(0, 0))
        s = Vec2(r.w, r.h)
        draw = Vec2(-ofs.x if flip_h else ofs.x, -ofs.y if flip_v else ofs.y)
        fx, fy = (-1 if flip_h else 1), (-1 if flip_v else 1)
        shape_first = len(self.shape_rows)
        for kind, pts, shape_ofs in self.tileset.shapes(tid, coord):
            off = Vec2(s.x - shape_ofs.x if flip_h else shape_ofs.x, s.y - shape_ofs.y if flip_v else shape_ofs.y)
            def xf(p):
                return (f32(fx * p.x + ofs.x + off.x), f32(fy * p.y + ofs.y + off.y))
            if kind == 'convex':
                self._shape(1, [xf(p) for p in pts], (0.0, 0.0))
            else:
                for i in range(0, len(pts), 2):
                    a, b = pts[i], pts[i + 1]
                    n = (b.y - a.y, -(b.x - a.x))
                    length = math.hypot(*n)
                    require(length > 0, 'Degenerate concave segment')
                    n = (fx * n[0] / length, fy * n[1] / length)
                    length = math.hypot(*n)
                    self._shape(2, [xf(a), xf(b)], (f32(n[0] / length), f32(n[1] / length)))
        row = dict(picture=picture, w=int(r.w), h=int(r.h), ox=f32(draw.x), oy=f32(draw.y),
                   z=int(t.get('z_index', 0)), shape_first=shape_first, shape_count=len(self.shape_rows) - shape_first,
                   flags=(1 if flip_h else 0) | (2 if flip_v else 0))
        self.tile_index[key] = len(self.tile_rows)
        self.tile_rows.append(row)
        return self.tile_index[key]

    def _shape(self, kind, pts, normal, strict=False):
        if kind == 1:
            # Godot accepts degenerate ConvexPolygonShape2D data (zero edges give zero
            # normals and the SAT falls back to a (0, 1) axis); bodies stay strict.
            require(2 <= len(pts) <= 64, 'Unsupported convex shape point count %d' % len(pts))
            area = sum(a[0] * b[1] - a[1] * b[0] for a, b in zip(pts, pts[1:] + pts[:1]))
            require(not strict or abs(area) > 1e-5, 'Degenerate convex body shape')
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        self.shape_rows.append(dict(kind=kind, first=len(self.points), count=len(pts), nx=normal[0], ny=normal[1],
                                    minx=min(xs), miny=min(ys), maxx=max(xs), maxy=max(ys)))
        self.points.extend(pts)

    # layers
    def layer(self, node):
        if node.path in self.layer_by_path:
            return self.layer_by_path[node.path]
        origin = tilemap_policy(node)
        xf = self.scene.global_xf[node.path]
        require((xf.xx, xf.xy, xf.yx, xf.yy) == (1, 0, 0, 1), 'Unreviewed TileMap transform on ' + node.path)
        ts = node.props.get('tile_set')
        cells = decode_cells(node)
        if cells:
            require(owner_of(node, 'tile_set').ext_path(ts, 'TileSet') == self.recipe['tileset'], 'Unreviewed TileSet on ' + node.path)
        ysort = bool(node.props.get('cell_y_sort', False))
        layer_mask = node.props.get('collision_layer', 1)
        mask = node.props.get('collision_mask', 1)
        collides = self.collides(layer_mask, mask)
        index = len(self.layers)
        self.layer_by_path[node.path] = index
        valid = []
        for c in cells:
            if c[2] not in self.tileset.tiles:
                self.undefined_cells += 1
                continue
            valid.append(c)
        chunk_first = len(self.chunks)
        by_chunk = {}
        for c in valid:
            by_chunk.setdefault((c[1] // 16, c[0] // 16), []).append(c)
        for (cy, cx) in sorted(by_chunk):
            first = len(self.cells)
            members = sorted(by_chunk[(cy, cx)], key=lambda c: (c[1], c[0]))
            minx = miny = math.inf
            maxx = maxy = -math.inf
            for c in members:
                require(c[6] < 65536 or not ysort, 'Too many y-sorted cells on ' + node.path)
                tile = self.tile(c[2], c[5], c[3], c[4])
                row = self.tile_rows[tile]
                self.cells.append((c[0], c[1], tile, c[6] if ysort else 0))
                bx, by = xf.ox + c[0] * 16, xf.oy + c[1] * 16
                for s in range(row['shape_first'], row['shape_first'] + row['shape_count']):
                    sh = self.shape_rows[s]
                    minx, miny = min(minx, bx + sh['minx']), min(miny, by + sh['miny'])
                    maxx, maxy = max(maxx, bx + sh['maxx']), max(maxy, by + sh['maxy'])
            if minx == math.inf:
                minx = miny = maxx = maxy = 0.0
            self.chunks.append(dict(cx=cx, cy=cy, first=first, count=len(self.cells) - first,
                                    minx=f32(minx), miny=f32(miny), maxx=f32(maxx), maxy=f32(maxy)))
        cond = self.condition_span(self.scene.conditions(node.path))
        name = self.string(node.path)
        self.layers.append(dict(name=name, flags=(1 if ysort else 0) | (2 if collides else 0), cond=cond,
                                x=f32(xf.ox), y=f32(xf.oy), chunk_first=chunk_first, chunk_count=len(self.chunks) - chunk_first,
                                origin_y=16.0 if origin == 2 else 0.0, visible=self.scene.visible(node.path)))
        return index

    # sprites
    def sprite(self, node, xf, cond, openable=0xffff, texture=None, flip_h=None):
        props = node.props
        allowed = {'position', 'texture', 'hframes', 'vframes', 'frame', 'offset', 'centered', 'flip_h', 'flip_v',
                   'region_enabled', 'region_rect', 'visible', 'use_parent_material', 'script', 'sprite', 'dialog', 'item',
                   'show_behind_parent', 'material', 'scale'}
        extra = set(props) - allowed
        require(not extra, 'Unreviewed sprite property on %s: %s' % (node.path, sorted(extra)))
        if texture is None:
            if props.get('texture') is None:
                return None
            texture = owner_of(node, 'texture').ext_path(props['texture'], 'Texture')
        frames, _ = self.textures.frames_of(texture)
        tw, th = self.images.size(frames[0])
        if props.get('region_enabled', False):
            r = props['region_rect']
        else:
            r = Rect2(0, 0, tw, th)
        hf, vf = props.get('hframes', 1), props.get('vframes', 1)
        frame = props.get('frame', 0)
        require(isinstance(hf, int) and isinstance(vf, int) and hf > 0 and vf > 0 and 0 <= frame < hf * vf, 'Bad sprite frames on ' + node.path)
        fw, fh = r.w / hf, r.h / vf
        require(fw == int(fw) and fh == int(fh), 'Nonintegral sprite frame on ' + node.path)
        sx, sy = r.x + fw * (frame % hf), r.y + fh * (frame // hf)
        offset = props.get('offset', Vec2(0, 0))
        dx, dy = offset.x, offset.y
        if props.get('centered', True):
            dx, dy = dx - fw / 2, dy - fh / 2
        dx, dy = math.floor(dx), math.floor(dy)
        require((xf.xx, xf.xy, xf.yx, xf.yy) == (1, 0, 0, 1), 'Unreviewed scaled/rotated sprite at ' + node.path)
        picture = self.textures.picture(self.pictures, texture, sx, sy, fw, fh)
        fh_flag = props.get('flip_h', False) if flip_h is None else flip_h
        index = len(self.sprites)
        self.sprites.append(dict(picture=picture, x=f32(xf.ox + dx), y=f32(xf.oy + dy), w=int(fw), h=int(fh),
                                 flags=(1 if fh_flag else 0) | (2 if props.get('flip_v', False) else 0),
                                 openable=openable, cond=cond))
        return index

    def _build(self):
        sc = self.scene
        for n in sc.nodes:
            if sc.category[n.path] == 'tilemap':
                self.layer(n)
        self._openables()
        self._doors_and_areas()
        self._bodies()
        self._draw_program()
        house = {n.path: n for n in instantiate(self.sources.doc, self.recipe['house']['source'])}
        self.route_rows = route_rows(self.recipe, self.sources, {self.recipe['house']['id']: house, self.recipe['scene']['id']: sc.by_path})

    # collision bodies in world space (tile shapes stay in per-cell templates)
    def _body_shapes(self, body_path, cond):
        sc = self.scene
        body = sc.by_path[body_path]
        layer, mask = body.props.get('collision_layer', 1), body.props.get('collision_mask', 1)
        if not self.collides(layer, mask):
            return
        for child in sc.children[body_path]:
            node = sc.by_path[child]
            if node.type not in ('CollisionShape2D', 'CollisionPolygon2D'):
                continue
            if node.props.get('disabled', False):
                continue
            if node.type == 'CollisionShape2D' and node.props.get('shape') is None:
                continue
            xf = sc.global_xf[child]
            require((xf.xx, xf.xy, xf.yx, xf.yy) in ((1, 0, 0, 1),) or (abs(xf.xy) < 1e-9 and abs(xf.yx) < 1e-9), 'Unreviewed rotated body shape at ' + child)
            first = len(self.shape_rows)
            if node.type == 'CollisionShape2D':
                doc = owner_of(node, 'shape')
                kind = doc.sub_type(node.props['shape'])
                props = doc.sub_resource(node.props['shape'])
                if kind == 'RectangleShape2D':
                    e = props['extents']
                    pts = [Vec2(-e.x, -e.y), Vec2(e.x, -e.y), Vec2(e.x, e.y), Vec2(-e.x, e.y)]
                elif kind == 'ConvexPolygonShape2D':
                    pts = props['points']
                else:
                    raise FieldError('Unreviewed body shape %s at %s' % (kind, child))
                self._shape(1, [(f32(xf.apply(p).x), f32(xf.apply(p).y)) for p in pts], (0.0, 0.0), strict=True)
            else:
                require(node.props.get('build_mode', 0) == 0 and not node.props.get('one_way_collision', False), 'Unreviewed polygon build mode at ' + child)
                # CollisionPolygon2D::_build_polygon (solids): one shape per decomposed part.
                parts = decompose_polygon_in_convex([(p.x, p.y) for p in node.props['polygon']])
                require(parts, 'CollisionPolygon2D decomposition failed at ' + child)
                for part in parts:
                    coords = [(f32(xf.apply(Vec2(x, y)).x), f32(xf.apply(Vec2(x, y)).y)) for x, y in part]
                    self._shape(1, coords, (0.0, 0.0), strict=True)
            self.bodies.append(dict(first=first, count=len(self.shape_rows) - first, cond=cond))

    def _bodies(self):
        sc = self.scene
        for n in sc.nodes:
            if n.type != 'StaticBody2D' or not sc.visible(n.path) and False:
                continue
            cat = sc.category[n.path]
            if cat in ('static_body', 'prop', 'openable_door'):
                self._body_shapes(n.path, self.condition_span(sc.conditions(n.path)))

    # openable doors (unlocked only); locked variants become boundaries
    def _openables(self):
        sc = self.scene
        script = sc.sources.text('Scripts/Main/Openable Door.gd')
        defaults = {}
        for key in ('sound', 'end_sound'):
            m = re.search(r'^export \(String, [^)]*\) var ' + key + r' := "([^"]*)"$', script, re.M)
            require(m is not None, 'Openable door default changed: ' + key)
            defaults[key] = m[1]
        timer = sc.sources.doc('Nodes/Overworld/Objects/Openable Door.tscn')
        m = re.search(r'^export \(Vector2\) var door_offset = Vector2\(([-0-9., ]+)\) setget _set_offset$', script, re.M)
        require(m is not None, 'Openable door offset default changed')
        script_offset = Vec2(*[float(v) for v in m[1].split(',')])
        require('func _update_positions():\n\t$Sprite.position = door_offset\n\tfor obj in [$Area2D, $interact, $StaticBody2D, $NonPlayerStaticBody2D]:\n\t\tobj.position.y = door_offset.y + 32' in script
                and 'func _set_texture(tex: Texture):\n\tsprite = tex\n\t$Sprite.texture = sprite' in script
                and '\t_update_positions()\n\t_update_door_state()' in script, 'Openable door runtime layout contract changed')
        self.openable_ids = {}
        for n in sc.nodes:
            if not (sc.category[n.path] == 'openable_door' and sc.owner_instance[n.path] == n.path):
                continue
            props = n.props
            locked = any(props.get(k) for k in ('key', 'blocked', 'locked', 'one_way'))
            require(not props.get('flag') and not props.get('activates_flag') and not props.get('deactivates_flag'), 'Unreviewed flagged openable door ' + n.path)
            root_xf = sc.global_xf[n.path]
            require((root_xf.xx, root_xf.xy, root_xf.yx, root_xf.yy) == (1, 0, 0, 1), 'Unreviewed openable door transform ' + n.path)
            offset = props.get('door_offset', script_offset)
            area_node = sc.by_path[n.path + '/Area2D']
            area = sc.by_path[n.path + '/Area2D/CollisionShape2D']
            doc = owner_of(area, 'shape')
            extents = doc.sub_resource(area.props['shape'], 'RectangleShape2D')['extents']
            area_pos = area_node.props.get('position', Vec2(0, 0))
            shape_pos = area.props.get('position', Vec2(0, 0))
            center = Transform2D(1, 0, 0, 1, root_xf.ox + area_pos.x + shape_pos.x, root_xf.oy + offset.y + 32 + shape_pos.y)
            cond = self.condition_span(sc.conditions(n.path))
            if locked:
                self._boundary(1, n.path, 'Locked door', center, extents, cond)
                continue
            index = len(self.openables)
            self.openable_ids[n.path] = index
            sound = props.get('sound', defaults['sound'])
            end_sound = props.get('end_sound', defaults['end_sound'])
            sprite = sc.by_path[n.path + '/Sprite']
            texture = owner_of(n, 'sprite').ext_path(props['sprite'], 'Texture') if props.get('sprite') is not None else self._sprite_texture(sprite)
            require(texture is not None, 'Openable door without texture ' + n.path)
            sprite_xf = Transform2D(1, 0, 0, 1, root_xf.ox + offset.x, root_xf.oy + offset.y)
            sprite_index = self.sprite(sprite, sprite_xf, cond, openable=index, texture=texture)
            self.openables.append(dict(path=self.string(n.path), cx=f32(center.ox), cy=f32(center.oy), ex=f32(extents.x * abs(center.xx)),
                                       ey=f32(extents.y * abs(center.yy)), sound=self.string('' if sound == 'None' else 'res://Audio/Sound effects/' + sound),
                                       end_sound=self.string('' if end_sound == 'None' else 'res://Audio/Sound effects/' + end_sound),
                                       timer=f32(0.3), sprite=sprite_index, cond=cond))
        tim = [s for s in timer.nodes if s.attrs.get('name') == 'Timer']
        require(len(tim) == 1 and tim[0].props.get('wait_time') == 0.3 and tim[0].props.get('one_shot') is True, 'Openable door timer changed')

    def _boundary(self, kind, path, label, xf, extents, cond):
        require(abs(xf.xy) < 1e-9 and abs(xf.yx) < 1e-9, 'Unreviewed rotated boundary at ' + path)
        ex, ey = abs(extents.x * xf.xx), abs(extents.y * xf.yy)
        self.boundaries.append(dict(kind=kind, path=self.string(path), label=self.string(label), cx=f32(xf.ox), cy=f32(xf.oy),
                                    ex=f32(ex), ey=f32(ey), cond=cond))

    def _area_rects(self, root):
        """Rectangle Area2D shapes below an instanced root, in global coordinates."""
        sc = self.scene
        out = []
        for p in sc.nodes:
            if not (p.path == root or p.path.startswith(root + '/')) or p.type != 'CollisionShape2D':
                continue
            parent = sc.by_path[parent_path(p.path)]
            if parent.type != 'Area2D' or p.props.get('disabled', False) or p.props.get('shape') is None:
                continue
            if parent.path.endswith('/interact') or parent.path.endswith('/VisibilityNotifier2D'):
                continue
            doc = owner_of(p, 'shape')
            kind = doc.sub_type(p.props['shape'])
            if kind == 'RectangleShape2D':
                out.append((sc.global_xf[p.path], doc.sub_resource(p.props['shape'])['extents']))
            elif kind == 'CircleShape2D':
                r = doc.sub_resource(p.props['shape'])['radius']
                out.append((sc.global_xf[p.path], Vec2(r, r)))
            else:
                raise FieldError('Unreviewed area shape %s at %s' % (kind, p.path))
        return out

    def _doors_and_areas(self):
        sc = self.scene
        recipe = self.recipe
        labels = recipe['labels']
        routes = {r['door']: r for r in recipe['routes'] if r['from'] == recipe['scene']['id']}
        self.door_params = {}
        for n in sc.nodes:
            cat = sc.category[n.path]
            if sc.owner_instance.get(n.path) != n.path:
                if cat == 'camera_area' and n.type == 'Area2D':
                    pass
                continue
            cond = self.condition_span(sc.conditions(n.path))
            if cat == 'door':
                rects = self._area_rects(n.path)
                require(len(rects) == 1, 'Door area shape count at ' + n.path)
                xf, extents = rects[0]
                target = n.props.get('targetScene', '')
                route = routes.get(n.path)
                index = len(self.doors)
                self.doors.append(dict(path=self.string(n.path), target=self.string('res://Maps/' + target + '.tscn' if target else ''),
                                       route=route['id'] if route else 0, cx=f32(xf.ox), cy=f32(xf.oy),
                                       ex=f32(abs(extents.x * xf.xx)), ey=f32(abs(extents.y * xf.yy)), cond=cond))
                if route:
                    require(target, 'Routed door lacks targetScene ' + n.path)
                self.door_params[n.path] = n
            elif cat == 'boundary':
                rects = self._area_rects(n.path)
                require(rects, 'Boundary without area shape ' + n.path)
                for xf, extents in rects:
                    self._boundary(2, n.path, labels[n.scene], xf, extents, cond)
            elif cat == 'notice':
                xf = sc.global_xf[n.path]
                self.notices.append(dict(kind=1, path=self.string(n.path), label=self.string(labels[n.scene]),
                                         x=f32(xf.ox), y=f32(xf.oy), cond=cond))
            elif cat == 'prop':
                xf = sc.global_xf[n.path]
                self.notices.append(dict(kind=2, path=self.string(n.path), label=self.string(labels['prop']),
                                         x=f32(xf.ox), y=f32(xf.oy), cond=cond))
            elif cat == 'camera_area':
                self._camera(n, cond)
        require(set(routes) <= set(self.door_params), 'Recipe route door missing from scene')

    def _camera(self, node, cond):
        sc = self.scene
        rects = self._area_rects(node.path)
        require(len(rects) == 1, 'Camera area shape count')
        xf, extents = rects[0]
        size = Vec2(round(extents.x * 2 * abs(sc.global_xf[node.path].xx)), round(extents.y * 2 * abs(sc.global_xf[node.path].yy)))
        rect_path = node.props.get('cam_rect_path')
        offset = node.props.get('camera_offset', Vec2(0, 0))
        if isinstance(rect_path, NodePathValue) and rect_path.path:
            target = sc.by_path[self._resolve(node.path, rect_path.path)]
            require(target.type == 'ReferenceRect', 'Camera rect is not a ReferenceRect')
            keys = set(target.props) - {'margin_left', 'margin_top', 'margin_right', 'margin_bottom', 'mouse_filter', 'border_color', 'editor_only', 'visible'}
            require(not keys, 'Unreviewed camera ReferenceRect properties')
            left, top = target.props.get('margin_left', 0), target.props.get('margin_top', 0)
            width = target.props.get('margin_right', 0) - left
            height = target.props.get('margin_bottom', 0) - top
            size = Vec2(round(width), round(height))
            limit = (int(left), int(top))
        else:
            limit = None
        self.cameras.append(dict(cx=f32(xf.ox), cy=f32(xf.oy), ex=f32(abs(extents.x * xf.xx)), ey=f32(abs(extents.y * xf.yy)),
                                 left=float(limit[0]) if limit else math.nan, top=float(limit[1]) if limit else math.nan,
                                 w=float(size.x), h=float(size.y), ox=f32(offset.x), oy=f32(offset.y), cond=cond))

    def _resolve(self, base, relative):
        parts = base.split('/') if base != '.' else []
        for part in relative.split('/'):
            if part == '..':
                require(parts, 'NodePath escapes scene root')
                parts.pop()
            elif part and part != '.':
                parts.append(part)
        return '/'.join(parts) if parts else '.'

    # canvas draw order (see docs/PODUNK_FIELD.md for tie-order assumptions)
    def _sort_y(self, node):
        return node.type == 'YSort' or (node.type == 'TileMap' and bool(node.props.get('cell_y_sort', False)))

    def _drawable(self, path):
        sc = self.scene
        node = sc.by_path[path]
        return node.type in NODE2D or node.type in CANVAS_CONTROL

    def _draw_program(self):
        sc = self.scene
        self.player_parent = 'YSort' if 'YSort' in sc.by_path else 'Objects'
        require(self.player_parent in sc.by_path and self._sort_y(sc.by_path[self.player_parent]), 'Player parent is not a y-sorted node')
        self._static = []
        self._render(sc.nodes[0].path)
        self._flush_static()

    def _flush_static(self):
        if self._static:
            first = len(self.items)
            self.items.extend(self._static)
            self.groups.append(dict(kind=1, first=first, count=len(self._static)))
            self._static = []

    def _item(self, kind, first, count, sort_y=0.0, order=0, cond=(0, 0)):
        return dict(kind=kind, first=first, count=count, sort_y=f32(sort_y), order=order, cond=cond)

    def _content(self, path):
        """Draw items produced by one non-y-sorted canvas node and its subtree, in canvas order."""
        sc = self.scene
        node = sc.by_path[path]
        out = []
        if not sc.visible(path) or not self._drawable(path):
            return out
        cat = sc.category[path]
        cond = self.condition_span(sc.conditions(path))
        if cat in ('notice', 'boundary', 'door', 'camera_area', 'camera_rect', 'static_body'):
            return out
        if cat == 'grass':
            if sc.owner_instance[path] == path:
                out.append(self._grass(node, cond))
            return out
        if cat == 'prop':
            owner = sc.owner_instance[path]
            if owner == path:
                rel = self.recipe['prop_sprites'][node.scene]
                first = len(self.sprites)
                for r in rel:
                    target = path if r == '.' else path + '/' + r
                    if sc.visible(target):
                        child = sc.by_path[target]
                        self.sprite(child, sc.global_xf[target], cond, texture=self._sprite_texture(child) if r == '.' else None)
                if len(self.sprites) > first:
                    out.append(self._item(3, first, len(self.sprites) - first, cond=cond))
            return out
        if cat == 'openable_door':
            owner = sc.owner_instance[path]
            if owner == path and owner in self.openable_ids:
                index = self.openables[self.openable_ids[owner]]['sprite']
                out.append(self._item(3, index, 1, cond=cond))
            return out
        if node.type == 'TileMap':
            layer = self.layer_by_path[path]
            if self.layers[layer]['chunk_count']:
                out.append(self._item(1, layer, 1, cond=cond))
            for child in sc.children[path]:
                out.extend(self._subtree(child))
            return out
        if cat == 'structure' and node.type == 'Camera2D':
            require(not sc.children[path], 'Unreviewed camera children at ' + path)
            return out
        require(node.type in STRUCTURAL, 'Unreviewed drawable node ' + path)
        for child in sc.children[path]:
            out.extend(self._subtree(child))
        return out

    def _sprite_texture(self, node):
        return owner_of(node, 'texture').ext_path(node.props['texture'], 'Texture') if node.props.get('texture') is not None else None

    def _subtree(self, path):
        """Items for a child while rendering a non-y-sorted parent (nested y-sort groups become entries)."""
        node = self.scene.by_path[path]
        if self._sort_y(node) and self.scene.visible(path):
            return [('ysort', path)]
        return self._content(path)

    def _render(self, path):
        for entry in self._content_or_groups(path):
            if isinstance(entry, tuple):
                self._flush_static()
                self._ysort_group(entry[1])
            else:
                self._static.append(entry)

    def _content_or_groups(self, path):
        node = self.scene.by_path[path]
        if self._sort_y(node):
            return [('ysort', path)]
        return self._content(path)

    def _ysort_group(self, path):
        sc = self.scene
        entries = []
        counter = [0]

        def collect(item_path, base_cond):
            node = sc.by_path[item_path]
            # Quadrant items (index 0) precede node children after the draw-index sort.
            if node.type == 'TileMap':
                layer = self.layer_by_path[item_path]
                if self.layers[layer]['chunk_count']:
                    entries.append(self._item(2 if self._sort_y(node) else 1, layer, 1, order=counter[0], cond=self.condition_span(sc.conditions(item_path))))
                    counter[0] += 65536 if self._sort_y(node) else 1
            for child in sc.children[item_path]:
                cn = sc.by_path[child]
                if not sc.visible(child) or not self._drawable(child):
                    continue
                if self._sort_y(cn):
                    collect(child, base_cond)
                    continue
                content = self._content(child)
                xf = sc.global_xf[child]
                for c in content:
                    if isinstance(c, tuple):
                        raise FieldError('Nested y-sort below a non-y-sorted item is not reviewed: ' + child)
                    c = dict(c)
                    if c['kind'] == 1 and sc.by_path[child].type == 'TileMap':
                        c['kind'] = 5
                    c['sort_y'] = f32(xf.oy)
                    c['order'] = counter[0]
                    entries.append(c)
                counter[0] += 1
            if item_path == self.player_parent:
                entries.append(self._item(4, 0, 0, order=counter[0]))
                counter[0] += 1

        collect(path, None)
        first = len(self.items)
        self.items.extend(entries)
        self.groups.append(dict(kind=2, first=first, count=len(entries)))

    def _grass(self, node, cond):
        script = self.sources.text('Scripts/misc/grass spawner.gd')
        require('seed(self.name.hash())' in script and 'randi()%grass_types+0' in script and '(randi()%2+0) == 1' in script, 'Grass spawner RNG contract changed')
        grass = self.sources.doc('Nodes/Overworld/Grass/grass.tscn')
        sprite = [s for s in grass.nodes if s.attrs.get('name') == 'Sprite']
        require(len(sprite) == 1, 'Grass sprite changed')
        sprite_props = sprite[0].props
        kinds = node.props.get('grass_types', 1)
        folder = node.props.get('sprite', 'Podunk')
        rng = SourceRandom()
        rng.seed(godot_string_hash(node.name))
        variant = rng.randi() % kinds
        flipped = rng.randi() % 2 == 1
        texture = 'Graphics/Objects/Grass/%s/%d.png' % (folder, variant)
        fake = type('N', (), {})()
        fake.path = node.path
        fake.props = {k: v for k, v in sprite_props.items() if k not in ('texture',)}
        fake.props['position'] = Vec2(0, 0)
        spawner_local = node.props.get('position', Vec2(0, 0))
        xf = Transform2D(1, 0, 0, 1, spawner_local.x, spawner_local.y)
        index = self.sprite(fake, xf, cond, texture=texture, flip_h=flipped)
        return self._item(3, index, 1, cond=cond)


# ---------------------------------------------------------------- encoding
MAP_MAGIC = b'ENCMAP01'
LINK_MAGIC = b'ENCLNK01'
HEADER = struct.Struct('<8s6I20s3I')
MAP_LIMIT = 8 * 1024 * 1024
LINK_LIMIT = 64 * 1024
MAP_SECTIONS = ('Bytes', 'Atlases', 'Frames', 'Pictures', 'Variants', 'Tiles', 'Shapes', 'Points', 'Conditions',
                'Layers', 'Chunks', 'Cells', 'Sprites', 'Items', 'Groups', 'Openables', 'Doors', 'Boundaries',
                'Notices', 'Cameras', 'Bodies')
MAP_STRIDES = (1, 16, 12, 16, 12, 32, 40, 8, 12, 48, 32, 8, 32, 32, 16, 56, 48, 48, 40, 56, 16)
LINK_SECTIONS = ('Bytes', 'Scenes', 'Routes')
LINK_STRIDES = (1, 24, 84)
TRANSITIONS = {'Fade': 0, 'Circle Focus': 1, 'Circle Pop': 2}


def pack_container(magic, scene_id, sections, strides, limit):
    count = len(sections)
    offset = HEADER.size + count * 16
    directory = bytearray()
    body = bytearray()
    for index, (rows, stride) in enumerate(zip(sections, strides)):
        raw = bytes(rows)
        require(len(raw) % stride == 0, 'Section stride mismatch')
        n = len(raw) // stride
        if n:
            pad = (-(offset + len(body))) % 4
            body += bytes(pad)
            start = offset + len(body)
            body += raw
        else:
            start = 0
        directory += struct.pack('<4I', index + 1, start, n, stride)
    payload = bytearray(HEADER.size) + directory + body
    total = len(payload)
    require(total <= limit, 'Field resource exceeds its format bound')
    HEADER.pack_into(payload, 0, magic, 1, total, 0, 1, scene_id, 0, bytes.fromhex(PIN), count, 0, 0)
    crc = zlib.crc32(bytes(payload[32:]))
    struct.pack_into('<I', payload, 16, crc)
    return bytes(payload)


def unpack_container(blob, magic, strides, limit):
    require(isinstance(blob, (bytes, bytearray)) and HEADER.size <= len(blob) <= limit, 'Field resource size rejected')
    m, version, total, crc, capability, scene_id, reserved, pin, count, r1, r2 = HEADER.unpack_from(blob)
    require(m == magic and version == 1 and total == len(blob) and capability == 1 and not reserved and not r1 and not r2,
            'Field resource header rejected')
    require(pin == bytes.fromhex(PIN), 'Field resource source pin rejected')
    require(zlib.crc32(bytes(blob[32:])) == crc, 'Field resource checksum rejected')
    require(count == len(strides), 'Field resource section count rejected')
    sections = []
    cursor = HEADER.size + count * 16
    for i in range(count):
        kind, offset, n, stride = struct.unpack_from('<4I', blob, HEADER.size + i * 16)
        require(kind == i + 1 and stride == strides[i], 'Field resource directory rejected')
        if n == 0:
            require(offset == 0, 'Noncanonical empty section')
            sections.append(b'')
            continue
        require(offset >= cursor and offset % 4 == 0 and n <= (len(blob) - offset) // stride, 'Field resource section range rejected')
        require(not any(blob[cursor:offset]), 'Nonzero section padding')
        sections.append(bytes(blob[offset:offset + n * stride]))
        cursor = offset + n * stride
    require(cursor == len(blob), 'Trailing field resource bytes')
    return scene_id, sections


class Atlases:
    PAGE = 1024

    def __init__(self, model):
        from PIL import Image
        self.model = model
        sources = model.sources
        cache = {}
        items = []
        for index, (path, x, y, w, h) in enumerate(model.images.keys):
            if path not in cache:
                import io
                cache[path] = Image.open(io.BytesIO(sources.data(path))).convert('RGBA')
            items.append((index, cache[path].crop((x, y, x + w, y + h))))
        order = sorted(items, key=lambda it: (-it[1].height, -it[1].width, it[0]))
        pages = []
        placement = {}
        for index, img in order:
            w, h = img.size
            require(w + 1 <= self.PAGE and h + 1 <= self.PAGE, 'Image too large for an atlas page')
            placed = False
            for p, page in enumerate(pages):
                spot = self._fit(page, w + 1, h + 1)
                if spot:
                    placement[index] = (p, spot[0], spot[1])
                    page['images'].append((index, img, spot))
                    placed = True
                    break
            if not placed:
                page = dict(shelves=[], height=0, images=[])
                spot = self._fit(page, w + 1, h + 1)
                pages.append(page)
                placement[index] = (len(pages) - 1, spot[0], spot[1])
                page['images'].append((index, img, spot))
        self.pages = []
        for p, page in enumerate(pages):
            width = max(spot[0] + img.width for _, img, spot in page['images'])
            height = max(spot[1] + img.height for _, img, spot in page['images'])
            pw = 8
            while pw < width:
                pw *= 2
            ph = 8
            while ph < height:
                ph *= 2
            canvas = Image.new('RGBA', (pw, ph), (0, 0, 0, 0))
            for _, img, spot in page['images']:
                canvas.paste(img, spot)
            self.pages.append(canvas)
        self.placement = placement

    def _fit(self, page, w, h):
        for shelf in page['shelves']:
            if shelf['h'] >= h and shelf['x'] + w <= self.PAGE:
                spot = (shelf['x'], shelf['y'])
                shelf['x'] += w
                return spot
        if page['height'] + h > self.PAGE:
            return None
        shelf = dict(x=w, y=page['height'], h=h)
        page['shelves'].append(shelf)
        page['height'] += h
        return (0, shelf['y'])

    def digest(self, index):
        page = self.pages[index]
        return hashlib.sha256(struct.pack('<2I', *page.size) + page.tobytes()).hexdigest()


def encode_map(model, atlases, atlas_paths):
    S = model.string
    secs = {name: bytearray() for name in MAP_SECTIONS}
    for path, page in zip(atlas_paths, atlases.pages):
        off, length = S(path)
        secs['Atlases'] += struct.pack('<2I2HI', off, length, page.width, page.height, 0)
    # Pictures reference contiguous frame runs; each picture owns its frame rows.
    frame_rows = []
    for index, key in enumerate(model.images.keys):
        page, x, y = atlases.placement[index]
        frame_rows.append(struct.pack('<6H', page, x, y, key[3], key[4], 0))
    variant_rows = []
    for row in model.pictures.rows:
        first_frame = len(secs['Frames']) // 12
        for image in row['frames']:
            secs['Frames'] += frame_rows[image]
        first_variant = len(variant_rows) if row['variants'] else NONE
        for locale, picture in row['variants']:
            variant_rows.append((S(locale), picture))
        secs['Pictures'] += struct.pack('<IHHfI', first_frame, len(row['frames']), len(row['variants']), row['fps'], first_variant)
    for (off, length), picture in variant_rows:
        secs['Variants'] += struct.pack('<3I', off, length, picture)
    for t in model.tile_rows:
        secs['Tiles'] += struct.pack('<I2H2fi3I', t['picture'], t['w'], t['h'], t['ox'], t['oy'], t['z'], t['shape_first'], t['shape_count'], t['flags'])
    for s in model.shape_rows:
        secs['Shapes'] += struct.pack('<2HI6f2I', s['kind'], s['count'], s['first'], s['nx'], s['ny'], s['minx'], s['miny'], s['maxx'], s['maxy'], 0, 0)
    for x, y in model.points:
        secs['Points'] += struct.pack('<2f', x, y)
    for (off, length), value in model.conditions:
        secs['Conditions'] += struct.pack('<2IB3x', off, length, 1 if value else 0)
    for layer in model.layers:
        off, length = layer['name']
        flags = layer['flags'] | (0 if layer['visible'] else 4)
        secs['Layers'] += struct.pack('<5I2f2If2I', off, length, flags, layer['cond'][0], layer['cond'][1], layer['x'], layer['y'],
                                      layer['chunk_first'], layer['chunk_count'], layer['origin_y'], 0, 0)
    for c in model.chunks:
        secs['Chunks'] += struct.pack('<2h2I4fI', c['cx'], c['cy'], c['first'], c['count'], c['minx'], c['miny'], c['maxx'], c['maxy'], 0)
    for x, y, tile, order in model.cells:
        require(-32768 <= x < 32768 and -32768 <= y < 32768 and tile < 65536 and order < 65536, 'Cell field overflow')
        secs['Cells'] += struct.pack('<2h2H', x, y, tile, order)
    for s in model.sprites:
        secs['Sprites'] += struct.pack('<I2f4H3I', s['picture'], s['x'], s['y'], s['w'], s['h'], s['flags'], s['openable'], s['cond'][0], s['cond'][1], 0)
    groups = [g for g in model.groups if g['count']]
    for g in groups:
        secs['Groups'] += struct.pack('<4I', g['kind'], g['first'], g['count'], 0)
    for it in model.items:
        secs['Items'] += struct.pack('<2H2IfI3I', it['kind'], 0, it['first'], it['count'], it['sort_y'], it['order'], it['cond'][0], it['cond'][1], 0)
    for o in model.openables:
        secs['Openables'] += struct.pack('<2I4f4IfI2I', *o['path'], o['cx'], o['cy'], o['ex'], o['ey'], *o['sound'], *o['end_sound'], o['timer'], o['sprite'], *o['cond'])
    for d in model.doors:
        secs['Doors'] += struct.pack('<5I4f3I', *d['path'], *d['target'], d['route'], d['cx'], d['cy'], d['ex'], d['ey'], d['cond'][0], d['cond'][1], 0)
    for b in model.boundaries:
        secs['Boundaries'] += struct.pack('<2H4I4f3I', b['kind'], 0, *b['path'], *b['label'], b['cx'], b['cy'], b['ex'], b['ey'], b['cond'][0], b['cond'][1], 0)
    for n in model.notices:
        secs['Notices'] += struct.pack('<2H4I2f3I', n['kind'], 0, *n['path'], *n['label'], n['x'], n['y'], n['cond'][0], n['cond'][1], 0)
    for c in model.cameras:
        has_rect = not math.isnan(c['left'])
        secs['Cameras'] += struct.pack('<10f4I', c['cx'], c['cy'], c['ex'], c['ey'], c['left'] if has_rect else 0.0, c['top'] if has_rect else 0.0,
                                       c['w'], c['h'], c['ox'], c['oy'], c['cond'][0], c['cond'][1], 1 if has_rect else 0, 0)
    for b in model.bodies:
        secs['Bodies'] += struct.pack('<4I', b['first'], b['count'], b['cond'][0], b['cond'][1])
    secs['Bytes'] = bytearray(model.strings)
    return pack_container(MAP_MAGIC, model.recipe['scene']['id'], [secs[n] for n in MAP_SECTIONS], MAP_STRIDES, MAP_LIMIT)


def door_parameters(sources, recipe, scene_doc_nodes, door_path):
    """Effective Door.gd export values of one door instance (script defaults + instance overrides)."""
    script = sources.text(recipe['door']['script'])
    defaults = {}
    for m in re.finditer(r'^export(?: \([^)]*\))? var (\w+)(?: :)?= (.+)$', script, re.M):
        name, raw = m[1], m[2].strip()
        if not raw.startswith('"') and '#' in raw:
            raw = raw.split('#', 1)[0].strip()
        if raw.startswith('"'):
            defaults[name] = json.loads(raw)
        elif raw in ('true', 'false'):
            defaults[name] = raw == 'true'
        elif raw == 'Vector2.ZERO':
            defaults[name] = Vec2(0, 0)
        elif raw == 'Color.black':
            defaults[name] = 'black'
        elif raw == '[]':
            defaults[name] = []
        else:
            defaults[name] = float(raw) if '.' in raw else int(raw)
    node = scene_doc_nodes[door_path]
    require(node.script == recipe['door']['script'], 'Route door is not a Door: ' + door_path)
    values = dict(defaults)
    for key, value in node.props.items():
        if key in defaults:
            values[key] = value
    require(values['_target_scene_params'] == [] and values['set_respawn'] is False and values['unpause_player'] is True
            and values['show_player_after_warp'] is True and values['transit_in_color'] == 'black' and values['transit_out_color'] == 'black',
            'Unreviewed door state at ' + door_path)
    require(values['transit_in_anim'] in TRANSITIONS and values['transit_out_anim'] in TRANSITIONS, 'Unported door transition at ' + door_path)
    target = node.path + '/Position2D'
    return values, target


def route_rows(recipe, sources, scenes):
    """Routes from both scenes; destinations follow Door.change_scene (targetY - 7)."""
    rows = []
    for route in recipe['routes']:
        values, _ = door_parameters(sources, recipe, scenes[route['from']], route['door'])
        target = 'res://Maps/' + values['targetScene'] + '.tscn'
        to = [s for s in (recipe['scene'], recipe['house']) if s['id'] == route['to']][0]
        require(target == 'res://' + to['source'], 'Route target differs from source door: ' + route['door'])
        rows.append(dict(id=route['id'], frm=route['from'], to=route['to'], door=route['door'],
                         x=f32(values['targetX']), y=f32(values['targetY'] + recipe['door']['destination_offset_y']),
                         dx=f32(values['dir'].x), dy=f32(values['dir'].y),
                         sound='' if values['sound'] == 'None' else 'res://Audio/Sound effects/' + values['sound'],
                         end_sound='' if values['end_sound'] == 'None' else 'res://Audio/Sound effects/' + values['end_sound'],
                         in_kind=TRANSITIONS[values['transit_in_anim']], out_kind=TRANSITIONS[values['transit_out_anim'] or values['transit_in_anim']],
                         in_speed=f32(values['fade_in_speed']), out_speed=f32(values['fade_out_speed']),
                         music_fade=f32(values['fadeout_music_length']) if values['fadeout_music_on_scene_change'] else 0.0,
                         flag=values['flag_set'], flag_value=bool(values['set_flag_state'])))
    return rows


def encode_links(recipe, rows, room_roles):
    strings = bytearray()
    index = {}

    def S(text):
        raw = text.encode('utf-8')
        if not raw:
            return (0, 0)
        if text not in index:
            index[text] = (len(strings), len(raw))
            strings.extend(raw)
        return index[text]
    scenes = bytearray()
    for s in (recipe['house'], recipe['scene']):
        off, length = S('res://' + s['source'])
        room, field = room_roles[s['id']]
        scenes += struct.pack('<6I', s['id'], off, length, room, field, 0)
    routes = bytearray()
    for r in rows:
        flags = 1 if r['music_fade'] > 0 else 0
        routes += struct.pack('<3I2I4f4I2H3f2I2I', r['id'], r['frm'], r['to'], *S(r['door']), r['x'], r['y'], r['dx'], r['dy'],
                              *S(r['sound']), *S(r['end_sound']), r['in_kind'], r['out_kind'], r['in_speed'], r['out_speed'],
                              r['music_fade'], *S(r['flag']), 1 if r['flag_value'] else 0, flags)
    return pack_container(LINK_MAGIC, 0, [strings, scenes, routes], LINK_STRIDES, LINK_LIMIT)


def build_room(recipe, model, house_ir_path):
    """Podunk room IR: the House player profile, flag table, rules and experience with Podunk scene identity."""
    from tools import native_content
    house = json.loads((ROOT / house_ir_path).read_text(encoding='utf-8'))
    native_content.validate_ir(house)
    hs, hstr = house['sections'], house['strings']
    scene = hs['Scene'][0]
    strings = ['']
    sindex = {'': 0}

    def S(text):
        if text not in sindex:
            sindex[text] = len(strings)
            strings.append(text)
        return sindex[text]
    sections = {name: [] for name in native_content.SCHEMAS if name not in ('StringRef', 'StringBytes')}
    player = hs['ActorInstance'][scene['player_instance_index']]
    profile = hs['ActorProfile'][player['profile_index']]
    resources = {}

    def resource(index):
        if index == NONE:
            return NONE
        if index not in resources:
            row = dict(hs['Resource'][index])
            require(row['kind'] == 1, 'Player resource must be a texture')
            row['path_string'] = S(hstr[row['path_string']])
            resources[index] = len(sections['Resource'])
            sections['Resource'].append(row)
        return resources[index]
    clips = {}

    def clip(index):
        if index == NONE:
            return NONE
        if index not in clips:
            row = dict(hs['Clip'][index])
            first = len(sections['Key'])
            sections['Key'].extend(dict(k) for k in hs['Key'][row['first_key']:row['first_key'] + row['key_count']])
            row['first_key'] = first
            clips[index] = len(sections['Clip'])
            sections['Clip'].append(row)
        return clips[index]
    prof = dict(profile)
    prof['primary_resource'] = resource(profile['primary_resource'])
    prof['shadow_resource'] = resource(profile['shadow_resource'])
    prof['emote_resource'] = resource(profile['emote_resource'])
    prof['idle_clip'] = clip(profile['idle_clip'])
    prof['emote_clip'] = clip(profile['emote_clip'])
    direction_first = len(sections['DirectionFrame'])
    sections['DirectionFrame'].extend(dict(d) for d in hs['DirectionFrame'][profile['direction_first']:profile['direction_first'] + profile['direction_count']])
    prof['direction_first'] = direction_first if profile['direction_count'] else 0
    binding_first = len(sections['AnimationBinding'])
    for b in hs['AnimationBinding'][profile['animation_binding_first']:profile['animation_binding_first'] + profile['animation_binding_count']]:
        row = dict(b)
        row['actor_profile_index'] = 0
        row['clip_index'] = clip(b['clip_index'])
        sections['AnimationBinding'].append(row)
    prof['animation_binding_first'] = binding_first
    sections['ActorProfile'].append(prof)
    inst = dict(player)
    inst['profile_index'] = 0
    inst['display_name_string'] = S(hstr[player['display_name_string']])
    inst['initial_clip'] = clip(player['initial_clip'])
    sections['ActorInstance'].append(inst)
    for f in hs['Flag']:
        row = dict(f)
        row['name_string'] = S(hstr[f['name_string']])
        sections['Flag'].append(row)
    sections['Rule'] = [dict(r) for r in hs['Rule']]
    sections['Experience'] = [dict(r) for r in hs['Experience']]
    hull_first = len(sections['Vec2'])
    sections['Vec2'].extend(dict(v) for v in hs['Vec2'][scene['actor_hull_first']:scene['actor_hull_first'] + scene['actor_hull_count']])
    sections['CameraArea'].append(dict(stable_id=1, flags=0, center=[0.0, 0.0], extents=[1000000.0, 1000000.0]))
    entry = [r for r in model.route_rows if r['to'] == recipe['scene']['id']][0]
    sections['Scene'].append(dict(stable_id=recipe['scene']['id'], display_name_string=S(recipe['scene']['display_name']),
                                  version_string=S(hstr[scene['version_string']]), source_scene_string=S('res://' + recipe['scene']['source']),
                                  player_instance_index=0, initial_program_index=NONE, actor_hull_first=hull_first,
                                  actor_hull_count=scene['actor_hull_count'], spawn=[entry['x'], entry['y']],
                                  start_direction=[entry['dx'], entry['dy']], initial_motion_state=0, initial_frame=scene['initial_frame'],
                                  initial_flag_first=0, initial_flag_count=0, body_rule_first=0, body_rule_count=0,
                                  default_camera_area=0, rule_profile_id=1, flags=0))
    provenance = dict(sorted({**{('upstream/MOTHER-Encore/' + k): v for k, v in model.sources.digests().items()},
                              house_ir_path: hashlib.sha256((ROOT / house_ir_path).read_bytes()).hexdigest(),
                              'content/podunk-field.json': hashlib.sha256(RECIPE.read_bytes()).hexdigest()}.items()))
    ir = dict(schema=1, family=native_content.FAMILY, rules=house['rules'], capabilities=house['capabilities'],
              scene_id=recipe['scene']['id'], upstream_commit=PIN, exporter_version=1, adapter_revision=1,
              strings=strings, sections=sections, provenance=dict(sources=provenance))
    blob, _ = native_content.compile_ir(ir)
    return blob


ROOM_ROLES = {1: (1, 0), 2: (31, 32)}
RECEIPT = ROOT / 'content/asset-receipts/graphics/world/podunk/source.json'
STAGING = ROOT / 'build/podunk-field'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def build_outputs(recipe):
    model = Model(recipe)
    atlases = Atlases(model)
    directory = recipe['atlas_directory']
    atlas_paths = ['%s/atlas-%02d.t3x' % (directory, i) for i in range(len(atlases.pages))]
    outputs = {
        recipe['outputs']['map']: encode_map(model, atlases, atlas_paths),
        recipe['outputs']['room']: build_room(recipe, model, recipe['house']['room_ir']),
        recipe['outputs']['links']: encode_links(recipe, model.route_rows, ROOM_ROLES),
    }
    unpack_container(outputs[recipe['outputs']['map']], MAP_MAGIC, MAP_STRIDES, MAP_LIMIT)
    unpack_container(outputs[recipe['outputs']['links']], LINK_MAGIC, LINK_STRIDES, LINK_LIMIT)
    return model, atlases, atlas_paths, outputs


def compile_all(recipe, tex3ds, romfs=ROOT / 'romfs', command=None):
    """tex3ds is the binary recorded in the receipt. command optionally runs that
    same binary elsewhere (e.g. a pinned container); file arguments are then
    repository-relative POSIX paths and the working directory is the repository."""
    require(tex3ds is not None and Path(tex3ds).is_file(), 'Need tex3ds to compile Podunk atlases')
    model, atlases, atlas_paths, outputs = build_outputs(recipe)
    STAGING.mkdir(parents=True, exist_ok=True)
    pages = []
    for index, (page, path) in enumerate(zip(atlases.pages, atlas_paths)):
        png = STAGING / ('atlas-%02d.png' % index)
        page.save(png, optimize=False)
        target = romfs / path
        target.parent.mkdir(parents=True, exist_ok=True)
        if command:
            rel = lambda p: Path(p).resolve().relative_to(ROOT).as_posix()
            subprocess.run(list(command) + ['-f', 'rgba8', '-z', 'none', '-o', rel(target), rel(png)], check=True, cwd=ROOT)
        else:
            subprocess.run([str(tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(target), str(png)], check=True)
        blob = target.read_bytes()
        pages.append(dict(path=path, width=page.width, height=page.height, rgba_sha256=atlases.digest(index),
                          t3x_sha256=sha(blob), bytes=len(blob)))
    for stale in sorted((romfs / recipe['atlas_directory']).glob('atlas-*.t3x')):
        require(stale.relative_to(romfs).as_posix() in atlas_paths, 'Stale Podunk atlas left in RomFS: ' + stale.name)
    for path, blob in outputs.items():
        target = romfs / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(blob)
    receipt = dict(schema=1, commit=PIN, recipe_sha256=sha(RECIPE.read_bytes()), compiler_sha256=sha(Path(__file__).read_bytes()),
                   tex3ds_sha256=sha(Path(tex3ds).read_bytes()), format='rgba8', compression='none', pages=pages,
                   outputs={path: dict(sha256=sha(blob), bytes=len(blob)) for path, blob in sorted(outputs.items())},
                   sources=model.sources.digests(), undefined_tile_cells=model.undefined_cells)
    RECEIPT.parent.mkdir(parents=True, exist_ok=True)
    RECEIPT.write_bytes((json.dumps(receipt, indent=2, sort_keys=True) + '\n').encode('utf-8'))
    return model, outputs, pages


def verify_all(recipe, romfs=ROOT / 'romfs'):
    receipt = json.loads(RECEIPT.read_text(encoding='utf-8'))
    require(receipt.get('schema') == 1 and receipt.get('commit') == PIN, 'Podunk asset receipt rejected')
    require(receipt['recipe_sha256'] == sha(RECIPE.read_bytes()), 'Podunk recipe changed since the last compile')
    model, atlases, atlas_paths, outputs = build_outputs(recipe)
    require(receipt['sources'] == model.sources.digests(), 'Podunk upstream source set changed')
    require([p['path'] for p in receipt['pages']] == atlas_paths, 'Podunk atlas page set changed')
    for index, page in enumerate(receipt['pages']):
        require(page['rgba_sha256'] == atlases.digest(index) and (page['width'], page['height']) == atlases.pages[index].size, 'Podunk atlas pixels are stale')
        blob = (romfs / page['path']).read_bytes()
        require(sha(blob) == page['t3x_sha256'] and len(blob) == page['bytes'], 'Podunk atlas texture differs from receipt: ' + page['path'])
    for path, blob in outputs.items():
        require((romfs / path).read_bytes() == blob, 'Podunk field resource is stale: ' + path)
        require(receipt['outputs'][path] == dict(sha256=sha(blob), bytes=len(blob)), 'Podunk receipt output mismatch: ' + path)
    return model, outputs


def stage_files(source_root=ROOT / 'romfs'):
    """Receipt-checked Podunk outputs and atlases for RomFS staging (no re-extraction)."""
    from tools import native_content
    source_root = Path(source_root)
    recipe = load_recipe()
    receipt = json.loads(RECEIPT.read_text(encoding='utf-8'))
    require(receipt.get('schema') == 1 and receipt.get('commit') == PIN and receipt['recipe_sha256'] == sha(RECIPE.read_bytes()), 'Podunk asset receipt is stale')
    files = {}
    for path, expected in receipt['outputs'].items():
        blob = (source_root / path).read_bytes()
        require(sha(blob) == expected['sha256'] and len(blob) == expected['bytes'], 'Podunk staged resource differs from receipt: ' + path)
        files[Path(path)] = blob
    require(set(receipt['outputs']) == set(recipe['outputs'].values()), 'Podunk receipt output set changed')
    room = native_content.parse_pack(files[Path(recipe['outputs']['room'])])
    require(room['scene_id'] == recipe['scene']['id'], 'Podunk room scene identity changed')
    for resource in room['sections']['Resource']:
        path = Path(room['strings'][resource['path_string']])
        data = (source_root / path).read_bytes()
        require(hashlib.sha256(data).hexdigest() == resource['sha256'], 'Podunk player texture fingerprint mismatch: ' + str(path))
        files[path] = data
    _, sections = unpack_container(files[Path(recipe['outputs']['map'])], MAP_MAGIC, MAP_STRIDES, MAP_LIMIT)
    unpack_container(files[Path(recipe['outputs']['links'])], LINK_MAGIC, LINK_STRIDES, LINK_LIMIT)
    pool = sections[0]
    atlases = []
    for i in range(len(sections[1]) // 16):
        off, length = struct.unpack_from('<2I', sections[1], i * 16)
        atlases.append(pool[off:off + length].decode('ascii'))
    require(atlases == [p['path'] for p in receipt['pages']], 'Podunk map atlas references differ from receipt')
    for page in receipt['pages']:
        blob = (source_root / page['path']).read_bytes()
        require(sha(blob) == page['t3x_sha256'] and len(blob) == page['bytes'], 'Podunk atlas differs from receipt: ' + page['path'])
        files[Path(page['path'])] = blob
    return files


def stats(recipe):
    model = Model(recipe)
    import collections
    print('nodes', len(model.scene.nodes), 'layers', len(model.layers), 'cells', len(model.cells), 'undefined', model.undefined_cells)
    print('tiles', len(model.tile_rows), 'shapes', len(model.shape_rows), 'points', len(model.points))
    print('images', len(model.images.keys), 'pictures', len(model.pictures.rows))
    print('sprites', len(model.sprites), 'items', len(model.items), 'groups', [(g['kind'], g['count']) for g in model.groups])
    print('openables', len(model.openables), 'doors', len(model.doors), 'boundaries', len(model.boundaries), 'notices', len(model.notices), 'cameras', len(model.cameras), 'bodies', len(model.bodies))
    print('conditions', len(model.conditions), 'strings', len(model.strings))
    print(collections.Counter(i['kind'] for i in model.items))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('stats', 'compile', 'verify'))
    parser.add_argument('--recipe', type=Path, default=RECIPE)
    parser.add_argument('--tex3ds', type=Path)
    parser.add_argument('--tex3ds-command', help='optional command that runs the same tex3ds binary (shell-split)')
    args = parser.parse_args()
    try:
        recipe = load_recipe(args.recipe)
        if args.command == 'stats':
            stats(recipe)
        elif args.command == 'compile':
            import shlex
            model, outputs, pages = compile_all(recipe, args.tex3ds, command=shlex.split(args.tex3ds_command) if args.tex3ds_command else None)
            print('Podunk field: %d layers, %d cells, %d atlas pages, %s' % (
                len(model.layers), len(model.cells), len(pages), ', '.join('%s %d bytes' % (k, len(v)) for k, v in sorted(outputs.items()))))
        else:
            model, outputs = verify_all(recipe)
            print('Podunk field verified: %s' % ', '.join(sorted(outputs)))
    except (FieldError, GodotTextError, OSError, KeyError, subprocess.SubprocessError) as error:
        print('PODUNK FIELD ERROR:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
