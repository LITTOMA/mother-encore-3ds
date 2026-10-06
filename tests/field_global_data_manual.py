"""Manual IR negatives only; not automatically registered or executed."""
from copy import deepcopy
from tools.field_global_data import load, validate

def manual_cases():
    original = load()
    assert original['policy']['saved_uid_zero']
    for mutate in (
        lambda d: d.update(schema=2),
        lambda d: d['policy'].update(entire_ready=True),
        lambda d: d['policy'].update(character_native='Reference'),
        lambda d: d['load_roles'].reverse(),
        lambda d: d['declarations'][0].update(native='Reference'),
        lambda d: d['declarations'].pop(),
        lambda d: d['pending'].clear(),
        lambda d: d['character']['exp_levels'].reverse(),
        lambda d: d['character']['skills'].append('unreviewedSkill'),
        lambda d: d['character'].update(affinities=[['invalid', float('nan')]]),
        lambda d: d.update(unknown=True),
    ):
        changed = deepcopy(original); mutate(changed)
        try: validate(changed)
        except ValueError: continue
        raise AssertionError('Unsupported globaldata scope accepted')
