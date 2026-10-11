#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace encore::upstream {
// Execution roles only. Source paths and their binding live in ENCCUE01.
enum class GameplayCue:uint32_t { MenuOpen=0, MenuClose=1, Confirm=2, Cursor=3, Back=4, Error=5, EnemyTurn=6, EnemyDefeat=7, PlayerDefeat=8, MortalDamage=9, Smash=10, Hurt=11, HurtCritical=12, Dodge=13, Miss=14, Swing=15, Attack=16, Hit=17, LearnedPsi=18, BossVictory=19, VictoryLoop=20, LevelUp=21, LevelUpLayer=22, Victory=23, Count };
class GameplayAudioData {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 const std::string& path(GameplayCue cue)const;
private:std::vector<std::string>paths_;
};
}
