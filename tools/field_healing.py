"""Closed Lifeup-alpha field semantics, admitted alongside the diary event."""
import re
from tools.extract_battle_entry import require,node

def extract(ex,skills,menus):
    doc=ex.yaml('Data/BattleSkills/lifeUpA.yaml')
    require(doc==dict(name='LIFEUP_NAME',level=0,description='LIFEUPA_DESC',article='LIFEUP_ART',skill_type='psi',action_type=1,
        use_cases=0,psi_category='Recovery',target_type=1,damage_or_heal=65,variance=5,priority=0,miss_chance=0,pp_cost=5,hp_cost=0,
        hit_effect='',pre_hit_effect='lifeup_alpha',screen_effect='lifeup',user_anim='psi',user_anim_colors=['90FF73','00FF6A','0C8A62'],
        use_sound='',hit_sound='',traits=['target_incapacitated']),'Unreviewed field healing definition')
    script=ex.text('Scripts/UI/PSIMenuUI.gd')
    for fact in ['_current_character.set_pp(_current_character.get_pp() - _skill.get("pp_cost", 0))',
        'var old_hp := target.get_hp() as int','var new_hp := old_hp + _calculate_heal_value(_skill, target)',
        'target.set_hp(new_hp)','val += _current_character.get_stat(Character.IQ) / 5',
        'val = floor(val + (randf() * variance) - variance/2.0)',
        'if new_hp >= target.get_stat(Character.MAXHP):','ACTION_RESULT_HP_MAX','ACTION_RESULT_HP_UP']:
        require(fact in script,'Field healing source changed: '+fact)
    sound=re.findall(r'"lifeup_a": load\("res://([^"\n]+)"\)',script)
    require(len(sound)==1,'Missing/ambiguous field healing sound');ex.data(sound[0]);ex.data(sound[0]+'.import')
    alpha=node(ex.text('Nodes/Ui/PSISkillLine.tscn'),'HBox/Alpha')['text']
    def translations(key):return {lang:menus[key][col] for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]}
    messages={name:translations(key) for name,key in [('max_message','ACTION_RESULT_HP_MAX'),('up_message','ACTION_RESULT_HP_UP')]}
    for values in messages.values():
        for value in values.values():require(not re.search(r'\{(?!target\}|value\})',value),'Unreviewed healing message context')
    return dict(source='lifeUpA',names={lang:skills[doc['name']][col]+' '+alpha for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]},
        descriptions={lang:skills[doc['description']][col] for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]},
        iq_divisor=int(re.search(r'get_stat\(Character.IQ\) / (\d+)',script)[1]),action=2,cost=doc['pp_cost'],power=doc['damage_or_heal'],variance=doc['variance'],sound=sound[0],**messages)
