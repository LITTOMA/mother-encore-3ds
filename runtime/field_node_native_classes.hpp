#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace encore::upstream::detail {
// Native structural opcodes only. Preserve all original ordinals; the two
// final CanvasItem subclasses are required by the complete native House tree.
inline constexpr std::array<std::string_view,27> field_native_classes={
  "Node","Node2D","Sprite","VisibilityNotifier2D","Position2D",
  "CollisionShape2D","AnimationPlayer","Area2D","Timer","KinematicBody2D",
  "VisibilityEnabler2D","TextureRect","HBoxContainer","Label",
  "AudioStreamPlayer","RayCast2D","AnimatedSprite","StaticBody2D",
  "CollisionPolygon2D","TileMap","YSort","Camera2D","AudioStreamPlayer2D",
  "Tween","ReferenceRect","Control","ColorRect"
};
inline bool field_native_class_schema(uint32_t count){return count==25||count==27;}
inline bool field_native_canvas(uint32_t opcode){
  switch(opcode){
    case 1:case 2:case 3:case 4:case 5:case 7:case 9:case 10:case 11:
    case 12:case 13:case 15:case 16:case 17:case 18:case 19:case 20:
    case 21:case 22:case 24:case 25:case 26:return true;
    default:return false;
  }
}
}
