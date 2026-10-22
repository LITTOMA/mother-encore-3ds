extends "res://Character.gd"
class_name PartyMember
var _permanent_boosts={}
var _inventory=Inventory.new()
var inv setget , get_inventory
func get_character_type() -> int:return Type.PARTY_MEMBER
func get_nickname():return _name
func get_inventory() -> Inventory:
	return _inventory

func set_stat(stat: String, new_value: int):
	if !(is_unconscious() and stat == HP and new_value > 0):
		.set_stat(stat, new_value)
		if stat == HP:
			_refresh_status_from_hp()

func add_status(ailment: String):
	.add_status(ailment)
	_refresh_hp_from_status()

func remove_status(ailment: String):
	.remove_status(ailment)
	_refresh_hp_from_status()

func _refresh_hp_from_status():
	if is_unconscious() and _hp > 0:
		set_hp(0)
	elif !is_unconscious() and _hp == 0:
		set_hp(999)

func _refresh_status_from_hp():
	if _hp == 0 and !is_unconscious():

func apply_boosts(boosts: Dictionary, performed_actions := {}):
	for stat_name in boosts.keys():
		if boosts[stat_name] > 0 and stat_name in BOOSTABLE_STATS:
			_permanent_boosts[stat_name] = _permanent_boosts.get(stat_name, 0) + boosts[stat_name]
			performed_actions[Item.ItemActions.STAT_UP] = performed_actions.get(Item.ItemActions.STAT_UP, {})
			performed_actions[Item.ItemActions.STAT_UP][stat_name] = boosts[stat_name]
			match stat_name:
				MAXHP:
					set_hp(get_hp() + boosts[stat_name])
				MAXPP:
					set_pp(get_pp() + boosts[stat_name])
