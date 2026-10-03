#!/usr/bin/env python3
"""Validate lossless Godot scene data and enforce the gameplay approval boundary."""
from __future__ import annotations
import math
import re

ARRAYS = {'Array', 'PoolByteArray', 'PoolIntArray', 'PoolRealArray', 'PoolStringArray',
          'PoolVector2Array', 'PoolVector3Array', 'PoolColorArray'}


def validate(document: dict) -> None:
    if document.get('schema') != 1 or document.get('coverage') != 'native_instantiated_data_with_raw_scene_states':
        raise ValueError('Unsupported scene data schema/coverage')
    if document.get('native_compatible') is not False:
        raise ValueError('Data extraction cannot approve gameplay compatibility')
    engine = document.get('godot', {})
    if [engine.get(k) for k in ('major', 'minor', 'patch', 'status')] != [3, 6, 2, 'stable']:
        raise ValueError('Scene data requires the pinned Godot 3.6.2 stable engine')
    resources = document.get('resources')
    nodes = document.get('nodes')
    states = document.get('scene_states')
    if not isinstance(resources, list) or not isinstance(nodes, list) or not isinstance(states, list) or not nodes or not states:
        raise ValueError('Incomplete scene data')
    if any(not isinstance(r, dict) or type(r.get('id')) != int for r in resources) or [r.get('id') for r in resources] != list(range(len(resources))):
        raise ValueError('Invalid local resource graph IDs')
    paths = [n.get('path') for n in nodes]
    if paths[0] != '.' or len(set(paths)) != len(paths) or any(not isinstance(p, str) or not p for p in paths):
        raise ValueError('Missing/duplicate native node identity')
    path_set = set(paths)
    for path in paths:
        if path != '.':
            if any(part in ('', '.', '..') for part in path.split('/')):
                raise ValueError('Noncanonical native node path')
            parent = path.rsplit('/', 1)[0] if '/' in path else '.'
            if parent not in path_set:
                raise ValueError('Native node has no parent: ' + path)

    def value(item):
        if item is None or type(item) in (str, bool):
            return
        if type(item) in (int, float):
            if not math.isfinite(item):
                raise ValueError('Non-finite scene value')
            return
        if not isinstance(item, dict) or 'type' not in item:
            raise ValueError('Unknown scene Variant encoding')
        kind = item['type']
        keys = set(item)
        if kind == 'int64':
            text = item.get('value')
            if keys != {'type', 'value'} or not isinstance(text, str) or not re.fullmatch(r'0|-?[1-9][0-9]*', text):
                raise ValueError('Noncanonical int64 value')
            if not -(2**63) <= int(text) < 2**63:
                raise ValueError('Out-of-range int64 value')
        elif kind in ('Vector2', 'Vector3', 'Color'):
            components = {'Vector2': {'x', 'y'}, 'Vector3': {'x', 'y', 'z'}, 'Color': {'r', 'g', 'b', 'a'}}[kind]
            if keys != components | {'type'} or any(type(item[k]) not in (int, float) or not math.isfinite(item[k]) for k in components):
                raise ValueError('Invalid numeric components: ' + kind)
        elif kind in ('Rect2', 'Transform2D'):
            components = {'Rect2': {'position', 'size'}, 'Transform2D': {'x', 'y', 'origin'}}[kind]
            if keys != components | {'type'}:
                raise ValueError('Invalid compound Variant fields')
            for key in components:
                if not isinstance(item[key], dict) or item[key].get('type') != 'Vector2':
                    raise ValueError('Compound Variant requires Vector2')
                value(item[key])
        elif kind in ('NodePath', 'NodeReference'):
            key = 'value' if kind == 'NodePath' else 'path'
            if keys != {'type', key} or not isinstance(item[key], str):
                raise ValueError('Invalid node reference')
            if kind == 'NodeReference' and item[key] not in path_set:
                raise ValueError('Unresolved native node reference')
        elif kind == 'ResourceReference':
            if keys != {'type', 'id'} or type(item['id']) != int or not 0 <= item['id'] < len(resources):
                raise ValueError('Unresolved resource graph reference')
        elif kind in ARRAYS:
            if keys != {'type', 'value'} or not isinstance(item['value'], list):
                raise ValueError('Invalid typed array')
            for child in item['value']:
                value(child)
                required = {'PoolIntArray': 'int64', 'PoolVector2Array': 'Vector2', 'PoolVector3Array': 'Vector3', 'PoolColorArray': 'Color'}.get(kind)
                if required and (not isinstance(child, dict) or child.get('type') != required):
                    raise ValueError('Wrong typed array element')
                if kind == 'PoolByteArray' and (not isinstance(child, dict) or child.get('type') != 'int64' or not 0 <= int(child['value']) <= 255):
                    raise ValueError('Invalid byte array element')
                if kind == 'PoolStringArray' and not isinstance(child, str):
                    raise ValueError('Invalid string array element')
                if kind == 'PoolRealArray' and type(child) not in (int, float):
                    raise ValueError('Invalid real array element')
        elif kind == 'Dictionary':
            if keys != {'type', 'pairs'} or not isinstance(item['pairs'], list):
                raise ValueError('Invalid dictionary encoding')
            for pair in item['pairs']:
                if not isinstance(pair, list) or len(pair) != 2:
                    raise ValueError('Invalid dictionary entry')
                value(pair[0])
                value(pair[1])
        else:
            raise ValueError('Unsupported scene Variant: ' + str(kind))

    def props(properties):
        if not isinstance(properties, dict):
            raise ValueError('Missing node/resource properties')
        for entry in properties.values():
            value(entry)

    for node in nodes:
        if not isinstance(node.get('class'), str) or not node['class']:
            raise ValueError('Unknown native node class')
        props(node.get('properties'))
        if 'world_transform' in node:
            value(node['world_transform'])
        if node['class'] in ('StaticBody2D', 'KinematicBody2D', 'RigidBody2D', 'Area2D', 'CollisionObject2D'):
            owners = node.get('physics_shape_owners')
            if not isinstance(owners, list):
                raise ValueError('Missing native collision shape owners')
            for owner in owners:
                if set(owner) != {'owner', 'transform', 'cached_transform_before_enter_tree', 'disabled', 'one_way', 'one_way_margin', 'shapes'}:
                    raise ValueError('Unknown collision owner fields')
                if type(owner['disabled']) != bool or type(owner['one_way']) != bool:
                    raise ValueError('Invalid collision flags')
                if type(owner['one_way_margin']) not in (int, float) or not math.isfinite(owner['one_way_margin']) or owner['one_way_margin'] < 0:
                    raise ValueError('Invalid one-way collision margin')
                if not isinstance(owner['transform'], dict) or owner['transform'].get('type') != 'Transform2D' or not isinstance(owner['shapes'], list):
                    raise ValueError('Invalid collision shape/transform data')
                value(owner['owner'])
                value(owner['transform'])
                value(owner['cached_transform_before_enter_tree'])
                for shape in owner['shapes']:
                    if not isinstance(shape, dict) or shape.get('type') != 'ResourceReference':
                        raise ValueError('Collision shape must reference a resource')
                    value(shape)
        if node['class'] == 'TileMap':
            cells = node.get('cells')
            if not isinstance(cells, list):
                raise ValueError('Missing resolved TileMap cells')
            for cell in cells:
                if set(cell) != {'position', 'tile', 'autotile', 'flip_x', 'flip_y', 'transpose', 'local_origin'}:
                    raise ValueError('Unknown TileMap cell fields')
                if type(cell['tile']) != int or cell['tile'] < 0 or any(type(cell[k]) != bool for k in ('flip_x', 'flip_y', 'transpose')):
                    raise ValueError('Invalid TileMap cell identity/flags')
                for key in ('position', 'autotile', 'local_origin'):
                    value(cell[key])
    for resource in resources:
        if not isinstance(resource.get('class'), str) or not isinstance(resource.get('path'), str):
            raise ValueError('Invalid resource source identity')
        if 'properties' in resource:
            props(resource['properties'])
        if 'size' in resource:
            value(resource['size'])
    for state in states:
        if not isinstance(state.get('source'), str) or not isinstance(state.get('nodes'), list) or not isinstance(state.get('connections'), list):
            raise ValueError('Incomplete raw SceneState')
        for node in state['nodes']:
            props(node.get('properties'))
            value(node.get('instance'))
            value(node.get('groups'))
        for connection in state['connections']:
            value(connection.get('binds'))


def require_gameplay(document: dict) -> None:
    validate(document)
    raise ValueError('Scene data is not playable content: script defaults, behavior, signals, factories and method tracks require audited adapters')
