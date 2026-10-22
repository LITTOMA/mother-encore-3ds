"""Audio bindings for scenes and objects added after the reviewed opening.

tools/phone_linker_bindings.py (which aggregates the bank) is pinned by the
opening room provenance, so later bindings append through field_audio instead
of editing it. Each module keeps its own source/coverage admission.
"""


def bindings(root):
    from tools.present_audio import bindings as present_audio
    from tools.gameplay_audio import bindings as gameplay_audio
    from tools.footstep_audio import bindings as footstep_audio
    from tools.field_prop_audio import bindings as prop_audio
    from tools.item_consume_audio import bindings as consume_audio
    return present_audio(root) + gameplay_audio(root) + footstep_audio(root) + prop_audio(root) + consume_audio(root)
