#include "encore/music_bank_routes.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {namespace {
bool fail(std::string&e,const char*why){e=why;return false;}
bool db(float value){return std::isfinite(value)&&value>=-120&&value<=0;}
bool same_media(const AudioAsset&a,const AudioAsset&b){
    return a.source_path==b.source_path&&a.source_sha256==b.source_sha256&&a.sample_rate==b.sample_rate&&a.channels==b.channels&&a.flags==b.flags&&a.frames==b.frames&&a.loop_start==b.loop_start&&a.pcm_bytes==b.pcm_bytes&&a.pcm_crc==b.pcm_crc&&a.gain_db==b.gain_db;
}
}
const MusicBankProvider*MusicBankRoutes::provider(uint64_t id)const{for(const auto&p:providers_)if(p.identity==id)return &p;return nullptr;}
bool MusicBankRoutes::initialize(MusicRoutingPolicy policy,std::string&e){
    if(ready_||!policy.maximum_providers||policy.maximum_providers>64||!policy.maximum_voices||policy.maximum_voices>256||!db(policy.master_db)||!db(policy.silence_db)||policy.silence_db>-20)return fail(e,"Music routing policy/owner is invalid");
    policy_=policy;ready_=true;e.clear();return true;
}
bool MusicBankRoutes::add_provider(uint64_t id,const MusicRegionData&regions,const AudioBank&bank,std::string&e){
    if(!ready_||!id||id==MusicLoopChoice::no_player||id<=last_provider_||providers_.size()>=policy_.maximum_providers||!regions.valid()||!bank.count()||bank.count()!=regions.tracks().size()||bank.master_db()!=policy_.master_db||bank.silence_db()!=policy_.silence_db)return fail(e,"Music provider identity/capacity/metadata/tuning is invalid");
    if(!regions.matches(bank,e))return false;
    for(const auto&old:providers_)for(const auto&track:regions.tracks())for(const auto&known:old.regions.tracks())if(track.source_path==known.source_path){
        AudioAsset a,b;
        if(track.source_sha!=known.source_sha||track.import_sha!=known.import_sha||!bank.find(track.id,a)||!old.bank.find(known.id,b)||!same_media(a,b))return fail(e,"Music providers disagree on one source stream/import/PCM identity");
    }
    providers_.push_back({id,regions,bank});last_provider_=id;e.clear();return true;
}
bool MusicBankRoutes::remove_provider(uint64_t id,std::string&e){
    if(!ready_||!provider(id))return fail(e,"Music provider removal identity is unavailable");
    for(const auto&route:routes_)if(route.provider==id)return fail(e,"Music provider still owns live routed players");
    providers_.erase(std::remove_if(providers_.begin(),providers_.end(),[&](const auto&p){return p.identity==id;}),providers_.end());e.clear();return true;
}
bool MusicBankRoutes::reconcile(const MusicPlayers&players,const std::vector<MusicPlayerStart>&starts,std::string&e){
    if(!ready_||!players.ready()||!players.owner()||(owner_&&owner_!=players.owner())||starts.size()>policy_.maximum_voices)return fail(e,"Music routing requires prepared bounded owners/starts");
    for(size_t i=0;i<starts.size();++i){
        const auto&s=starts[i];const auto*node=players.find(s.player);
        if(!node||!node->playing||!s.generation||s.generation!=node->generation||!provider(s.provider))return fail(e,"Music start has no current checked player/provider");
        for(size_t j=0;j<i;++j)if(starts[j].player==s.player)return fail(e,"Music starts contain duplicate player identities");
    }
    std::vector<MusicPlaybackRoute>next;std::vector<bool>slots(policy_.maximum_voices,false),consumed(starts.size(),false);
    for(const auto&node:players.nodes())if(node.playing){
        if(next.size()>=policy_.maximum_voices)return fail(e,"Music routing voice capacity exhausted; old routes preserved");
        const MusicPlaybackRoute*old=nullptr;for(const auto&r:routes_)if(r.player==node.identity){old=&r;break;}
        size_t admission=starts.size();for(size_t i=0;i<starts.size();++i)if(starts[i].player==node.identity){admission=i;break;}
        MusicPlaybackRoute route;route.player=node.identity;route.generation=node.generation;route.gain_db=node.gain_db;route.stream_paused=node.stream_paused;route.slot=policy_.maximum_voices;
        if(old&&node.generation<old->generation)return fail(e,"Music routed playback generation regressed");
        if(old&&old->generation==node.generation){
            if(admission!=starts.size())return fail(e,"Music start redundantly admits an unchanged playback");
            route.provider=old->provider;route.track_id=old->track_id;
        }else{
            if(admission==starts.size())return fail(e,"Music playback start lacks an explicit checked bank binding");
            const auto*owner=provider(starts[admission].provider);bool found=false;
            for(const auto&track:owner->regions.tracks())if(track.source_path==node.stream){route.provider=owner->identity;route.track_id=track.id;found=true;break;}
            if(!found)return fail(e,"Music start source is absent from its checked provider");
            consumed[admission]=true;
        }
        const auto*owner=provider(route.provider);AudioAsset asset;
        if(!owner||!owner->bank.find(route.track_id,asset)||asset.source_path!=node.stream||(!asset.loops()&&!owner->regions.library())||!std::isfinite(route.gain_db)||route.gain_db<-120||route.gain_db>24)return fail(e,"Music route no longer matches its source player/bank");
        if(old){if(old->slot>=slots.size()||slots[old->slot])return fail(e,"Music routed slot ownership is invalid");route.slot=old->slot;slots[route.slot]=true;}
        next.push_back(route);
    }
    for(bool used:consumed)if(!used)return fail(e,"Music start was not consumed by a new playback generation");
    for(auto&route:next)if(route.slot==policy_.maximum_voices){auto free=std::find(slots.begin(),slots.end(),false);if(free==slots.end())return fail(e,"Music routed slots exhausted");route.slot=uint32_t(free-slots.begin());slots[route.slot]=true;}
    routes_=std::move(next);owner_=players.owner();e.clear();return true;
}
}
