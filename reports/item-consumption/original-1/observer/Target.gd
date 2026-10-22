extends Node
signal show_dialogbox(dialog,character,value,stat,item)
var _messages_stack={}
func consume_item(source, item: Item, action_details: Dictionary, target: PartyMember) -> void:
	var item_data: Dictionary = item.get_data()
	var actions_performed := {}
	var fail_reason: String = action_details.get("textfail", "ACTION_RESULT_FAIL_ANY")
	if item_data.get("can_consume", globaldata.characters).has(target.get_name()):
		if target.is_in_state_to_receive_item(item):
			actions_performed = source.inv.consume_item(item, target)
		else:
			fail_reason = target.get_item_inability_message(item)
	
	var success := false
	
	if actions_performed.size() == 0:
		emit_signal("show_dialogbox", fail_reason, target, 0, "", item_data)
	else:
		for key in actions_performed:
			var message = "ACTION_RESULT_%s" % Item.ItemActions.keys()[key]
			var cur_action = actions_performed[key]
			match key:
				Item.ItemActions.HP_UP, Item.ItemActions.PP_UP,\
				Item.ItemActions.HP_MAX, Item.ItemActions.PP_MAX:
					success = true
					if (_messages_stack.has(target)):
						_messages_stack[target].append(TextTools.format_text_with_context(message, target, {}, cur_action))
					else:
						_messages_stack[target] = [TextTools.format_text_with_context(message, target, {}, cur_action)]
				Item.ItemActions.STAT_UP:
					success = true
					var stats = cur_action
					for stat in stats:
						emit_signal("show_dialogbox", message, target, stats[stat], stat)
				Item.ItemActions.HEAL:
					success = true
					if cur_action.size() > 1:
						emit_signal("show_dialogbox", "ACTION_RESULT_HEAL_ALL", target)
					else:
						emit_signal("show_dialogbox", Status.get_status_message(cur_action[0], "heal_overworld"), target)
				Item.ItemActions.HEAL_FAIL:
					if cur_action.size() > 1:
						emit_signal("show_dialogbox", "ACTION_RESULT_HEAL_NONE", target)
					else:
						emit_signal("show_dialogbox", Status.get_status_message(cur_action[0], "heal_overworld_fail"), target)
	if success:
		audioManager.play_sfx(load("res://Audio/Sound effects/EB/eat.wav"), "menu")
