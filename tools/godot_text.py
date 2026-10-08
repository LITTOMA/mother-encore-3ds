#!/usr/bin/env python3
"""Strict reader for Godot 3.6 text scenes/resources (format=2).

Offline build input only. Every Variant constructor, section tag and property
syntax that is not listed here is rejected; values are never evaluated as code.
Instancing flattens PackedScene children with the parent scene's overrides, so
callers see effective node properties without a Godot runtime.
"""
from __future__ import annotations
import math
import re
from dataclasses import dataclass, field


class GodotTextError(ValueError):
    pass


@dataclass(frozen=True)
class Vec2:
    x: float
    y: float


@dataclass(frozen=True)
class Rect2:
    x: float
    y: float
    w: float
    h: float


@dataclass(frozen=True)
class Transform2D:
    xx: float
    xy: float
    yx: float
    yy: float
    ox: float
    oy: float

    def apply(self, p: Vec2) -> Vec2:
        return Vec2(self.xx * p.x + self.yx * p.y + self.ox, self.xy * p.x + self.yy * p.y + self.oy)


@dataclass(frozen=True)
class Color:
    r: float
    g: float
    b: float
    a: float


@dataclass(frozen=True)
class ExtRef:
    id: int


@dataclass(frozen=True)
class SubRef:
    id: int


@dataclass(frozen=True)
class NodePathValue:
    path: str


_NUMBER = re.compile(r'-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?')
_IDENT = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
_KEY = re.compile(r'[A-Za-z0-9_][A-Za-z0-9_/:.\-]*')
_FIXED = {'Vector2': 2, 'Rect2': 4, 'Transform2D': 6, 'Color': 4}
_POOLS = {'PoolIntArray', 'PoolRealArray', 'PoolByteArray', 'PoolStringArray', 'PoolVector2Array', 'PoolColorArray'}


class _Reader:
    def __init__(self, text: str, pos: int = 0):
        self.text = text
        self.pos = pos

    def fail(self, message: str):
        line = self.text.count('\n', 0, self.pos) + 1
        raise GodotTextError(f'{message} at line {line}')

    def skip(self):
        text, pos = self.text, self.pos
        while pos < len(text) and text[pos] in ' \t\r\n':
            pos += 1
        self.pos = pos

    def peek(self) -> str:
        self.skip()
        return self.text[self.pos] if self.pos < len(self.text) else ''

    def expect(self, char: str):
        if self.peek() != char:
            self.fail(f'Expected {char!r}')
        self.pos += 1

    def string(self) -> str:
        self.expect('"')
        out, text, pos = [], self.text, self.pos
        while True:
            if pos >= len(text):
                self.fail('Unterminated string')
            c = text[pos]
            if c == '"':
                self.pos = pos + 1
                return ''.join(out)
            if c == '\\':
                pos += 1
                if pos >= len(text):
                    self.fail('Unterminated escape')
                e = text[pos]
                if e == 'u':
                    digits = text[pos + 1:pos + 5]
                    if not re.fullmatch(r'[0-9a-fA-F]{4}', digits):
                        self.fail('Bad unicode escape')
                    out.append(chr(int(digits, 16)))
                    pos += 5
                    continue
                mapped = {'"': '"', '\\': '\\', 'n': '\n', 't': '\t', 'r': '\r', '/': '/', "'": "'", 'b': '\b', 'f': '\f'}.get(e)
                if mapped is None:
                    self.fail('Unreviewed string escape')
                out.append(mapped)
            else:
                out.append(c)
            pos += 1

    def number(self):
        self.skip()
        m = _NUMBER.match(self.text, self.pos)
        if not m:
            self.fail('Expected number')
        self.pos = m.end()
        raw = m.group(0)
        if re.fullmatch(r'-?\d+', raw):
            return int(raw)
        value = float(raw)
        if not math.isfinite(value):
            self.fail('Nonfinite number')
        return value

    def numbers(self, count=None):
        self.expect('(')
        values = []
        if self.peek() != ')':
            while True:
                values.append(self.number())
                if self.peek() == ',':
                    self.pos += 1
                    continue
                break
        self.expect(')')
        if count is not None and len(values) != count:
            self.fail('Wrong constructor arity')
        return values

    def value(self):
        c = self.peek()
        if c == '"':
            return self.string()
        if c == '[':
            self.pos += 1
            items = []
            if self.peek() != ']':
                while True:
                    items.append(self.value())
                    if self.peek() == ',':
                        self.pos += 1
                        if self.peek() == ']':
                            break
                        continue
                    break
            self.expect(']')
            return items
        if c == '{':
            self.pos += 1
            result = {}
            if self.peek() != '}':
                while True:
                    key = self.value()
                    if not isinstance(key, (str, int)):
                        self.fail('Unreviewed dictionary key')
                    self.expect(':')
                    if key in result:
                        self.fail('Duplicate dictionary key')
                    result[key] = self.value()
                    if self.peek() == ',':
                        self.pos += 1
                        if self.peek() == '}':
                            break
                        continue
                    break
            self.expect('}')
            return result
        if c == '-' or c == '.' or c.isdigit():
            return self.number()
        m = _IDENT.match(self.text, self.pos)
        if not m:
            self.fail('Unreadable value')
        name = m.group(0)
        self.pos = m.end()
        if name in ('true', 'false'):
            return name == 'true'
        if name == 'null':
            return None
        if name in _FIXED:
            v = self.numbers(_FIXED[name])
            return {'Vector2': Vec2, 'Rect2': Rect2, 'Transform2D': Transform2D, 'Color': Color}[name](*[float(x) for x in v])
        if name in ('ExtResource', 'SubResource'):
            v = self.numbers(1)
            if not isinstance(v[0], int) or v[0] < 0:
                self.fail('Invalid resource reference')
            return ExtRef(v[0]) if name == 'ExtResource' else SubRef(v[0])
        if name == 'NodePath':
            self.expect('(')
            path = self.string()
            self.expect(')')
            return NodePathValue(path)
        if name in _POOLS:
            if name == 'PoolStringArray':
                self.expect('(')
                items = []
                if self.peek() != ')':
                    while True:
                        items.append(self.string())
                        if self.peek() == ',':
                            self.pos += 1
                            continue
                        break
                self.expect(')')
                return items
            raw = self.numbers()
            if name in ('PoolIntArray', 'PoolByteArray'):
                if any(not isinstance(x, int) for x in raw):
                    self.fail('Non-integer pool element')
                return raw
            if name == 'PoolRealArray':
                return [float(x) for x in raw]
            if name == 'PoolVector2Array':
                if len(raw) % 2:
                    self.fail('Odd PoolVector2Array')
                return [Vec2(float(raw[i]), float(raw[i + 1])) for i in range(0, len(raw), 2)]
            if len(raw) % 4:
                self.fail('Bad PoolColorArray')
            return [Color(*[float(x) for x in raw[i:i + 4]]) for i in range(0, len(raw), 4)]
        self.fail('Unreviewed Variant constructor ' + name)


@dataclass
class Section:
    tag: str
    attrs: dict
    props: dict = field(default_factory=dict)
    order: list = field(default_factory=list)


def _section_attrs(reader: _Reader, end: int) -> dict:
    attrs = {}
    while True:
        reader.skip()
        if reader.pos >= end:
            return attrs
        m = _IDENT.match(reader.text, reader.pos)
        if not m:
            reader.fail('Bad section attribute')
        key = m.group(0)
        reader.pos = m.end()
        reader.expect('=')
        if key in attrs:
            reader.fail('Duplicate section attribute')
        attrs[key] = reader.value()


def parse(text: str) -> list:
    """Return the ordered sections of one text resource; header comes first."""
    if '\r' in text:
        raise GodotTextError('Carriage returns are not accepted in pinned sources')
    reader = _Reader(text)
    sections = []
    current = None
    while True:
        reader.skip()
        if reader.pos >= len(text):
            break
        if text[reader.pos] == '[':
            end = text.find(']\n', reader.pos)
            if end < 0:
                end = len(text) - 1 if text.endswith(']') else -1
            if end < 0:
                reader.fail('Unterminated section header')
            reader.pos += 1
            m = _IDENT.match(text, reader.pos)
            if not m:
                reader.fail('Missing section tag')
            reader.pos = m.end()
            attrs = _section_attrs(reader, end)
            if reader.pos != end:
                reader.fail('Section header overrun')
            reader.pos = end + 1
            current = Section(m.group(0), attrs)
            sections.append(current)
            continue
        if current is None:
            reader.fail('Property outside section')
        if text[reader.pos] == '"':
            key = reader.string()
            if not key or '\n' in key:
                reader.fail('Bad quoted property key')
        else:
            m = _KEY.match(text, reader.pos)
            if not m:
                reader.fail('Bad property key')
            key = m.group(0)
            reader.pos = m.end()
        reader.expect('=')
        if key in current.props:
            reader.fail('Duplicate property ' + key)
        current.props[key] = reader.value()
        current.order.append(key)
        nl = text.find('\n', reader.pos)
        tail = text[reader.pos:nl if nl >= 0 else len(text)]
        if tail.strip():
            reader.fail('Trailing tokens after property')
    if not sections or sections[0].tag not in ('gd_scene', 'gd_resource'):
        raise GodotTextError('Missing gd_scene/gd_resource header')
    if sections[0].attrs.get('format') != 2:
        raise GodotTextError('Only text format 2 is reviewed')
    return sections


@dataclass
class Node:
    path: str
    name: str
    type: str | None
    script: str | None
    scene: str | None
    props: dict
    groups: list
    owner_scene: str
    # Document that resolves ExtRef/SubRef values of non-overridden props.
    resolve: object = None
    # Per-property Document of the instancing scene that overrode the value.
    overrides: dict = field(default_factory=dict)


class Document:
    """One parsed resource with resolvable local references."""

    def __init__(self, source: str, text: str):
        self.source = source
        try:
            self.sections = parse(text)
        except GodotTextError as error:
            raise GodotTextError(f'{source}: {error}') from None
        self.header = self.sections[0]
        self.ext = {}
        self.sub = {}
        self.nodes = []
        self.connections = []
        self.editable = []
        self.resource = None
        for s in self.sections[1:]:
            if s.tag == 'ext_resource':
                rid, path, kind = s.attrs.get('id'), s.attrs.get('path'), s.attrs.get('type')
                if not isinstance(rid, int) or not isinstance(path, str) or not path.startswith('res://') or not isinstance(kind, str) or s.props:
                    raise GodotTextError('Malformed ext_resource in ' + source)
                if rid in self.ext:
                    raise GodotTextError('Duplicate ext_resource id in ' + source)
                self.ext[rid] = (path[6:], kind)
            elif s.tag == 'sub_resource':
                rid, kind = s.attrs.get('id'), s.attrs.get('type')
                if not isinstance(rid, int) or not isinstance(kind, str):
                    raise GodotTextError('Malformed sub_resource in ' + source)
                if rid in self.sub:
                    raise GodotTextError('Duplicate sub_resource id in ' + source)
                self.sub[rid] = (kind, s.props)
            elif s.tag == 'node':
                self.nodes.append(s)
            elif s.tag == 'connection':
                self.connections.append(s.attrs)
            elif s.tag == 'editable':
                # Permission marker only: child overrides are merged by path either way.
                if set(s.attrs) != {'path'} or not isinstance(s.attrs['path'], str) or s.props:
                    raise GodotTextError('Malformed editable section in ' + source)
                self.editable.append(s.attrs['path'])
            elif s.tag == 'resource':
                if self.resource is not None or self.header.tag != 'gd_resource':
                    raise GodotTextError('Unexpected resource section in ' + source)
                self.resource = s.props
            else:
                raise GodotTextError('Unreviewed section tag ' + s.tag + ' in ' + source)

    def ext_path(self, ref: ExtRef, kind: str | None = None) -> str:
        if not isinstance(ref, ExtRef) or ref.id not in self.ext:
            raise GodotTextError('Unresolved ExtResource in ' + self.source)
        path, actual = self.ext[ref.id]
        if kind is not None and actual != kind:
            raise GodotTextError(f'ExtResource {ref.id} is {actual}, expected {kind}')
        return path

    def sub_resource(self, ref: SubRef, kind: str | None = None) -> dict:
        if not isinstance(ref, SubRef) or ref.id not in self.sub:
            raise GodotTextError('Unresolved SubResource in ' + self.source)
        actual, props = self.sub[ref.id]
        if kind is not None and actual != kind:
            raise GodotTextError(f'SubResource {ref.id} is {actual}, expected {kind}')
        return props

    def script_identity(self, ref) -> str:
        """External script path, or '<scene>#GDScript<id>' for an embedded script."""
        if isinstance(ref, SubRef):
            self.sub_resource(ref, 'GDScript')
            return f'{self.source}#GDScript{ref.id}'
        return self.ext_path(ref, 'Script')

    def sub_type(self, ref: SubRef) -> str:
        if not isinstance(ref, SubRef) or ref.id not in self.sub:
            raise GodotTextError('Unresolved SubResource in ' + self.source)
        return self.sub[ref.id][0]


def node_path(attrs: dict) -> str:
    name, parent = attrs.get('name'), attrs.get('parent')
    if not isinstance(name, str) or not name or '/' in name:
        raise GodotTextError('Bad node name')
    if parent is None:
        return '.'
    if not isinstance(parent, str):
        raise GodotTextError('Bad node parent')
    return name if parent == '.' else parent + '/' + name


_NODE_ATTRS = {'name', 'type', 'parent', 'instance', 'index', 'groups', 'owner', 'instance_placeholder'}


def instantiate(load, source: str, depth: int = 0) -> list:
    """Flatten one PackedScene into effective nodes (root path '.').

    ``load(path)`` returns a Document for a res-relative path. Instanced scenes
    are expanded with their own resolver; overrides from the instancing scene
    replace properties key by key. Inherited root scenes are expanded the same way.
    """
    if depth > 16:
        raise GodotTextError('Instancing depth exceeded at ' + source)
    doc = load(source)
    if doc.header.tag != 'gd_scene':
        raise GodotTextError('Not a scene: ' + source)
    nodes: dict = {}
    order: list = []
    for s in doc.nodes:
        unknown = set(s.attrs) - _NODE_ATTRS
        if unknown or 'owner' in s.attrs or 'instance_placeholder' in s.attrs:
            raise GodotTextError('Unreviewed node attributes in ' + source)
        path = node_path(s.attrs)
        groups = s.attrs.get('groups', [])
        if not isinstance(groups, list) or any(not isinstance(g, str) for g in groups):
            raise GodotTextError('Bad node groups in ' + source)
        instance = s.attrs.get('instance')
        if instance is not None:
            sub_scene = doc.ext_path(instance, 'PackedScene')
            if 'type' in s.attrs:
                raise GodotTextError('Typed instance node in ' + source)
            if path in nodes:
                raise GodotTextError('Duplicate node path ' + path + ' in ' + source)
            children = instantiate(load, sub_scene, depth + 1)
            prefix = '' if path == '.' else path + '/'
            for child in children:
                child_path = path if child.path == '.' else prefix + child.path
                if child_path in nodes:
                    raise GodotTextError('Instanced child collides with ' + child_path)
                copy = Node(child_path, child.name if child.path != '.' else s.attrs['name'], child.type, child.script,
                            child.scene if child.path != '.' else sub_scene, dict(child.props), list(child.groups),
                            child.owner_scene, child.resolve, dict(child.overrides))
                if child.path == '.':
                    copy.groups = copy.groups + groups
                nodes[child_path] = copy
                order.append(child_path)
            root = nodes[path]
            for key, value in s.props.items():
                root.props[key] = value
                root.overrides[key] = doc
            continue
        if path in nodes:
            existing = nodes[path]
            if 'type' in s.attrs:
                raise GodotTextError('Typed override of existing node ' + path + ' in ' + source)
            for key, value in s.props.items():
                existing.props[key] = value
                existing.overrides[key] = doc
            existing.groups = existing.groups + groups
            continue
        kind = s.attrs.get('type')
        if not isinstance(kind, str):
            raise GodotTextError('Untyped new node ' + path + ' in ' + source)
        if path != '.':
            parent = path.rsplit('/', 1)[0] if '/' in path else '.'
            if parent not in nodes:
                raise GodotTextError('Node parent missing for ' + path + ' in ' + source)
        script = None
        if s.props.get('script') is not None:
            script = doc.script_identity(s.props['script'])
        node = Node(path, s.attrs['name'], kind, script, None, dict(s.props), list(groups), source, doc)
        nodes[path] = node
        order.append(path)
    result = [nodes[p] for p in order]
    for n in result:
        if 'script' in n.props:
            owner = n.overrides.get('script', n.resolve)
            n.script = owner.script_identity(n.props['script']) if n.props['script'] is not None else None
    return result


def parse_project_value(text: str, section: str, key: str):
    """Value of one key in a project.godot section, parsed as a Variant."""
    if '\r' in text:
        raise GodotTextError('Carriage returns are not accepted in pinned sources')
    header = re.search(r'^\[' + re.escape(section) + r'\]\n', text, re.M)
    if not header:
        raise GodotTextError('Missing project section ' + section)
    end = re.search(r'^\[', text[header.end():], re.M)
    body_end = header.end() + end.start() if end else len(text)
    found = [m for m in re.finditer(r'^' + re.escape(key) + r'=', text[:body_end], re.M) if m.start() >= header.end()]
    if len(found) != 1:
        raise GodotTextError('Missing/ambiguous project key ' + key)
    reader = _Reader(text, found[0].end())
    value = reader.value()
    if reader.pos > body_end:
        raise GodotTextError('Project value overruns its section')
    return value


def owner_of(node: Node, key: str) -> Document:
    """Document whose ext/sub tables resolve node.props[key]."""
    return node.overrides.get(key, node.resolve)
