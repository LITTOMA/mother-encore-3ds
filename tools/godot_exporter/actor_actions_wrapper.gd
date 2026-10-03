extends KinematicBody2D
signal finished_movement
signal finished_action
enum {MOVETO, STEP, TALKING, ROTATING, IDLE}
var animation_name = "Lamp"
var character_sprite
var _special_sprite
var speed: float = 64.0
var is_party_member = false
var _jumping = false
var talking = false
var mute = false
var _moonwalk = false
var _looping = false
var _blend_position = true
var _animating = false
var new_pos = Vector2.ZERO
var _direction = Vector2(0,1)
var velocity = Vector2.ZERO
var state = IDLE
var _idle_anim = "Idle"
var _talk_idle_anim = "Idle"
var _sprite_position = Vector2(0,9)
var _spinning = false
var _spin_time = 0.0
var _spin_speed = 0.2
var _spin_anticlockwise = false
var _replaced = null
var emotes
var action_count = 0
var movement_count = 0
func _ready():
    collision_layer = 0
    collision_mask = 0
    var shape = CollisionShape2D.new()
    var rectangle = RectangleShape2D.new()
    rectangle.extents = Vector2(8.5,6)
    shape.shape = rectangle
    shape.position = Vector2(-0.5,4)
    add_child(shape)
    character_sprite = load("res://character_sprite.gd").new()
    var player = AnimationPlayer.new()
    player.name = "AnimationPlayer"
    character_sprite.add_child(player)
    add_child(character_sprite)
    character_sprite.set_animation(animation_name)
    character_sprite.position = Vector2(0,9)
    character_sprite.offset = Vector2(0,-11)
    _special_sprite = Sprite.new()
    add_child(_special_sprite)
    var shadow = Node2D.new()
    shadow.name = "Shadow"
    shadow.hide()
    add_child(shadow)
    connect("finished_action",self,"record_action")
    connect("finished_movement",self,"record_movement")
func record_action(): action_count += 1
func record_movement(): movement_count += 1
func _replaced_exists(): return false
func set_spin(_enabled,_speed,_anticlockwise): assert(false)
func _set_anim_speed(_speed): assert(false)
func set_blending(_blend): assert(false)
