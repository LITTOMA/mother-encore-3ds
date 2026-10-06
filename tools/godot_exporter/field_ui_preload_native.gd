# Original source resource-only export extension. No native/Script Ready.
# Environment.sky_orientation is exact Godot Basis, never converted to angles.
extends "res://scene_data.gd"
func tag(value):
    if typeof(value) == TYPE_BASIS:
        return {"type": "Basis", "x": tag(value.x), "y": tag(value.y), "z": tag(value.z)}
    return .tag(value)
