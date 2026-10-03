# Wrapper for exact-source _move/_movement; excluded interfaces are explicit
# in compatibility/reviews/movement-v0410.json. Never claim full Player behavior.
extends KinematicBody2D
signal moved
enum TPModes { MANUAL = -1 }
const SPEED_WALKING = 64
const SPEED_RUNNING = 96
class OpeningMember:
    extends Reference
    func has_field_skill(name):
        assert(name in ["teleport", "relay"])
        return false
class AnimationRecorder:
    extends Reference
    var body
    func travel(name): body.animation = name
class DustRecorder:
    extends Node
    func create_dust(): pass
var _input_vector = Vector2.ZERO
var _direction = Vector2(0,1)
var _velocity = Vector2.ZERO
var _knockback = Vector2.ZERO
var _speed = 64.0
var _tap_run = false
var _walk = false
var _crouch = false
var _running = false
var _substantial_movement = false
var _paused = false
var _climbing = false
var _spinning = false
var _idle = false
var _tp_crouch_timer_done = false
var can_interact = false
var _party_member = OpeningMember.new()
var _anim_tree = Node.new()
var _anim_state = AnimationRecorder.new()
var _anim_player = AnimationPlayer.new()
var eventRayCaster = RayCast2D.new()
var _timer = Timer.new()
var _tp_crouch_timer = Timer.new()
var animation = "Idle"
var moved_count = 0
func _ready():
    _anim_state.body = self
    add_child(eventRayCaster)
    add_child(_timer)
    add_child(_tp_crouch_timer)
    add_child(_anim_tree)
    add_child(_anim_player)
    var blink = Timer.new()
    blink.name = "BlinkTime"
    add_child(blink)
    var dust = DustRecorder.new()
    dust.name = "DustCreator"
    add_child(dust)
    connect("moved", self, "record_moved")
func record_moved(): moved_count += 1
func is_climbing(): return false
func _set_running(value): _running = value
func _update_party_positions(_old): pass
func set_anim_state(value): animation = value
func blend_position(_value): pass
func start_teleport(_mode): assert(false)
func swap_spin(_direction): assert(false)
func use_telepathy(): assert(false)
func interact_with(): assert(false)
