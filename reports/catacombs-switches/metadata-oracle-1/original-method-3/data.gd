extends Node
var flags={}
var object_flags={}
func _init_flags():
	var flag_names := [
		#===GLOBAL===
		#Melodies
		"doll_melody",
		"canary_melody",
		"monkey_melody",
		"piano_melody",
		"cactus_melody",
		"dragon_melody",
		"eve_melody",
		"grave_melody",
		
		#Abilities
		"switch_leader",
		"bat",
		"energy_beam",
		"eagle_feather",
		"encore_tutorial",
		"used_encore",
		
		#Misc
		"earned_cash",
		"good_morning",
		"saved",
		
		#Debug
		"npc_disappear_1",
		"npc_dialog_1",
		"npc_dialog_2",
		
		#Exploration
		"visited_podunk",
		"visited_magicant",
		"visited_merrysville",
		"visited_twinkle_elementary",
		"visited_reindeer",
		"visited_spookane",
		"visited_snowman",
		"visited_yucca",
		"visited_youngtown",
		"visited_ellay",
		"visited_mtitoi",
		
		#Pippi Banter
		"pippi_reunion",
		"pippi_met_lloyd",
		"pippi_met_ana",
		"pippi_met_teddy",
		"pippi_rocket",
		
		#Paty Members Visit Magicant
		"lloyd_magicant",
		"ana_magicant",
		"teddy_magicant",
		
		#===ACT 1===
		#Ninten's House
		"poltergeist",
		"mimmie_door_opened",
		"doll_attack",
		"pillow_attack",
		"minnie_leave",
		"minnie_door",
		"doll_defeated",
		"phone_ring",
		"talked_to_dad",
		"mimmie_ask_key",
		"carol_ask_key",
		"mick_scratch",
		"mick_telepathy",
		"ninten_basement_door",
		"got_juice",
		"got_diary",
		"got_dog_treats",
		"got_asthma_spray",
		"gave_treats",
		
		#Podunk
		"beat_dog_bridge",
		"pippis_mom_call",
		"pippis_mom_help",
		
		#Podunk Cemetery
		"disguised_zombie_defeated",
		"cem_roots_grown",
		"got_shovel",
		"returned_shovel",
		
		#Podunk Catacombs
		"hidden_entrance_opened",
		"pippi_fight",
		"pippi_fought",
		"pippi_rescued",
		"courage_badge_received",
		"church_door_opened",
		"church_exited",
		
		#Return to podunk
		"canary_found",
		"wally_attack",
		"wally_fought", # duplicated but ANYWAYS we shouldn't touch either of these cuz of backwards compatibility with older saves
		"wally_defeat",
		"pippi_reunite",
		"alfred_song",
		"leave_canary_village",
		"pippi_delivered",
		
		#Zoo
		"zoo_key_stolen",
		"starman_jr_fought",
		"zoo_freed",
		"monkey_apologize",
		"guard_moved",
		"ninten_family_goodbye",
		
		#===ACT 2===
		
		#Magicant
		"got_magic_candy",
		"got_magic_ribbon",
		"lent_cash_card",
		"got_big_bag",
		"met_happiness_guy",
		"met_guy_who_talks_about_how_you_cant_part_if_you_havent_met",
		"got_nicknamed",
		"slept_at_spoon_guys_place",
		"guard_riddle_solved",
		"fountain_scouts_met",
		"marys_story_learned",
		"enteredwell",
		"first_flying_man",
		"flying_man_in_party",
		"flying_man_1_joined",
		"flying_man_2_joined",
		"flying_man_3_joined",
		"flying_man_4_joined",
		"flying_man_5_joined",

		"onyx_hook_found",
		"fish_attack",
		"fish_defeat",
		
		"forgotten_man_1",
		"forgotten_man_2",
		"forgotten_man_3",
		"forgotten_man_4",
		"forgotten_man_5",
		
		#Merrysville 1
		"roof_door_checked",
		"janitor_cutscene_finished",
		"lloyd_roof_1",
		
		#Sweet's Little Factory
		"rat_king_1",
		"rat_king_2",
		"sweet_factory_key_1",
		"got_br_wings",
		"got_br_head",
		"br_progress_1",
		"br_progress_1_arrived",
		"br_wings_attached",
		"br_progress_2",
		"br_progress_2_arrived",
		"br_head_attached",
		"br_progress_3",
		"br_progress_3_arrived",
		"br_finishing_touch",
		"rat_king_attack",
		"rat_king_defeat",
		"rat_king_disappear",
		"bottle_rocket_found",
		"sweet_factory_bomb_chain",
		"sweet_factory_exit_open",
		
		#Merrysville 2
		"lloyd_befriended",
		"lab_destroyed",
		"missile_explosion",
		"missile_damage_seen",
		"weird_teacher_appear",
		"weird_teacher_alt_dialogue",
		"weird_teacher_real_rocket_launched",
		"got_factory_pass",
		
		#Duncan's Factory
		"guard_dog_beaten",
		"drdistorto_1",
		"drdistorto_2",
		"drdistorto_3",
		"drdistorto_4",
		"computer_left_hacked",
		"computer_right_hacked",
		"computers_all_hacked",
		"franklin_badge_found",
		"drdistorto_attack",
		"drdistorto_defeat",
		"rocket_launched",
		"drdistorto_disappear",
		#Merrysville 3
		"lloyd_kitchen",
		"fixed_merrysville_road_bridge",
		"lloyd_mom_goodbye",
		"train_to_reindeer",

		#===ACT 3===

		#Reindeer
		"met_arms_dealer",
		"received_swindler_money",
		"donated_to_charity",
		"found_ana_hat",

		#Magicant
		"magicant_second_visit",
		"wally2_attack",
		"wally2_defeat",
		
		#Snowman
		"lloyd_asleep",
		"ana_befriended",
		"nancy_attack",
		"nancy_defeat",
		
		#===ACT 4===
		
		#Spookane
		"magicant_third_visit",
		
		#Yucca Desert
		"giantrobot1_attack",
		"giantrobot1_defeat",
		"magicant_fourth_visit",
		
		#Youngtown #Swamp #Ellay #MtItoi
		"magicant_fifth_visit",
		"magicant_sixth_visit"
	]
	
	for flag in flag_names:
		flags[flag] = false

func set_flag(flag_name: String, value: bool, emit_signal := true):
	if flags.has(flag_name):
		flags[flag_name] = value
		if emit_signal: global.emit_signal("flags_updated")

func set_object_flag(flag_name: String, value: bool, emit_signal := true):
	object_flags[flag_name] = value
	if emit_signal: global.emit_signal("flags_updated")

########################### DATA GETTERS ###########################

# AILMENTS
