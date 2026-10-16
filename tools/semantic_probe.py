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
EXTRA_FIELDS = ('world_programme world_dialogue story_revision_lo story_revision_hi story_flag_count '
                + ' '.join('story_flag_'+str(i)for i in range(6))+' '
                'cash bank earned_cash loot_ready loot_selected loot_granted battle_current_hp battle_target_hp '
                'dialogue_music_playing dialogue_music_asset dsp_dialogue_playing dsp_dialogue_sample_pos dsp_dialogue_wave_seq '
                'region_music_paused region_music_voices region_music_track dsp_region_playing dsp_region_sample_sum dsp_region_wave_sum '
                + ' '.join('inventory_'+str(i)+'_'+name for i in range(16)for name in ('definition','doses','equipped'))+' '
                'random_state_lo random_state_hi random_draws_lo random_draws_hi').split()
TAIL8 = struct.Struct('<82I128s')

def wire_size(header):
    if len(header)<16 or header[:8]!=MAGIC:raise ValueError('Unsupported semantic probe magic/header')
    version,size=struct.unpack_from('<2I',header,8)
    if (version,size)not in ((6,WIRE.size),(8,WIRE.size+TAIL8.size)):raise ValueError('Unsupported semantic probe identity/version/size')
    return size


def decode(data):
    if len(data) != wire_size(data):
        raise ValueError('Truncated or oversized semantic probe')
    magic, *values = WIRE.unpack_from(data)
    result = dict(zip(FIELDS, values[:len(FIELDS)]))
    if magic != MAGIC or result['byte_size'] != len(data):
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
    if result['version']==8:
        extra=TAIL8.unpack_from(data,WIRE.size);result.update(zip(EXTRA_FIELDS,extra[:-1]));raw=extra[-1]
        if b'\0'not in raw:raise ValueError('Unterminated battle observation error')
        result['battle_error']=raw.split(b'\0',1)[0].decode('utf8',errors='strict')
        if result['story_flag_count']>192 or result['inventory_count']>16 or result['region_music_voices']>16 or result['dsp_region_playing']>16:raise ValueError('Semantic observation capacity exceeded')
        for i in range(result['story_flag_count'],192):
            if result['story_flag_'+str(i//32)]&(1<<(i%32)):raise ValueError('Trailing observed story flag')
        for name in ('loot_ready','loot_selected','loot_granted','dialogue_music_playing','dsp_dialogue_playing','region_music_paused'):
            if result[name]not in (0,1):raise ValueError('Invalid extended observation boolean')
        for i in range(16):
            definition,doses,equipped=(result['inventory_'+str(i)+'_'+k]for k in ('definition','doses','equipped'))
            if i<result['inventory_count']:
                if not definition or not 1<=doses<=65535 or equipped not in (0,1):raise ValueError('Invalid observed item identity/doses/equipment')
            elif definition or doses or equipped:raise ValueError('Trailing observed inventory data')
    return result


def projection(state, fields):
    if not fields or len(set(fields)) != len(fields) or any(k not in state for k in fields):
        raise ValueError('Unknown or duplicated semantic projection field')
    return {k: state[k] for k in fields}
