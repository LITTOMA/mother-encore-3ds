extends "res://Character.gd"
class_name PartyMember
var _permanent_boosts={}
var _inventory=Inventory.new()
var inv setget , get_inventory
func get_character_type() -> int:return Type.PARTY_MEMBER
func get_nickname():return _name
func get_inventory() -> Inventory:
	return _inventory

# Returns learned skills; can filter by type (basic, skill, psi)

func set_stat(stat: String, new_value: int):
	if !(is_unconscious() and stat == HP and new_value > 0):
		.set_stat(stat, new_value)
		if stat == HP:
			_refresh_status_from_hp()

# Override

func add_status(ailment: String):
	.add_status(ailment)
	_refresh_hp_from_status()

# Override

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
#		add_status(Status.AILMENT_UNCONSCIOUS)
		pass

#####################################################################
################### ITEMS/INVENTORY RELATED STUFF ###################
#####################################################################

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

# Boosts = permanent stats boosting items (like capsules) and equipment
# Not to be confused with mods (stats changes during a fight)
