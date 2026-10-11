"""Audio bindings for scenes and objects added after the reviewed opening.

tools/phone_linker_bindings.py (which aggregates the bank) is pinned by the
opening room provenance, so later bindings append through field_audio instead
of editing it. Each module keeps its own source/coverage admission.
"""


def bindings(root):
    from tools.present_audio import bindings as present_audio
    return present_audio(root)
