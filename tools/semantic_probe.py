"""Decode the explicitly versioned CTR observation ABI, never native C++ objects."""
import math
import struct

MAGIC = b'ENCSEM01'
FIELDS = ('version byte_size sequence frame width height title_phase title_option '
          'naming_phase naming_field naming_selected intro_phase menu_phase menu_command '
          'menu_slot menu_candidate field_scene field_phase house_phase world_stage '
          'paused crouch session_ready battle_active level hp pp inventory_count key_count '
          'audio_available music_playing music_asset dsp_music_playing dsp_music_sample_pos dsp_music_wave_seq psi_learned intro_scene intro_door overflow battle_entry_phase battle_round_phase battle_outcome_phase enemy_hp door_unlocked_mask world_phrase mick_phase choice_phase save_phase present_opened_mask object_flag_count experience dialogue_active dialogue_finished dialogue_stopped dialogue_visible').split()
POSITIONS = ('player_x', 'player_y', 'direction_x', 'direction_y', 'mick_x', 'mick_y')
ERRORS = ('house_error', 'field_error', 'menu_error', 'audio_error')
WIRE = struct.Struct('<8s55I6f128s128s128s128s')


def decode(data):
    if len(data) != WIRE.size:
        raise ValueError('Truncated or oversized semantic probe')
    magic, *values = WIRE.unpack(data)
    result = dict(zip(FIELDS, values[:len(FIELDS)]))
    if magic != MAGIC or result['version'] != 6 or result['byte_size'] != WIRE.size:
        raise ValueError('Unsupported semantic probe identity/version/size')
    if result['sequence'] % 2:
        raise ValueError('Semantic probe producer is mid-commit')
    if result['overflow']:
        raise ValueError('Semantic probe text overflow')
    result.update(zip(POSITIONS, values[len(FIELDS):len(FIELDS)+len(POSITIONS)]))
    if not all(math.isfinite(result[k]) for k in POSITIONS):
        raise ValueError('Nonfinite semantic probe position')
    for name, raw in zip(ERRORS, values[-4:]):
        if b'\0' not in raw:
            raise ValueError('Unterminated semantic probe text')
        result[name] = raw.split(b'\0', 1)[0].decode('utf-8', errors='strict')
    for name in ('paused', 'crouch', 'session_ready', 'battle_active', 'audio_available', 'music_playing', 'dsp_music_playing', 'psi_learned', 'dialogue_active', 'dialogue_finished', 'dialogue_stopped'):
        if result[name] not in (0, 1):
            raise ValueError('Invalid semantic probe boolean')
    return result


def projection(state, fields):
    if not fields or len(set(fields)) != len(fields) or any(k not in state for k in fields):
        raise ValueError('Unknown or duplicated semantic projection field')
    return {k: state[k] for k in fields}
