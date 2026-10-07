#pragma once
#include "encore/field_global_registry.hpp"
namespace encore::ctr {
// Stored inside the actual stable native voice body. The function/data pair
// follows the native AudioServer Set key, rather than ordering by ObjectID.
struct PodunkAudioVoiceCallback {
  upstream::FieldObjectId object = 0;
  upstream::FieldGlobalRegistry *registry = nullptr;
  void *owner = nullptr;
  bool (*mix_owner)(void *, upstream::FieldObjectId, std::string &) = nullptr;
};
using PodunkAudioMixFunction = bool (*)(PodunkAudioVoiceCallback *,
                                        std::string &);
inline bool podunk_audio_stream_player_mix(PodunkAudioVoiceCallback *v,
                                           std::string &e) {
  if (!v || !v->object || !v->registry || !v->owner || !v->mix_owner) {
    e = "AudioStreamPlayer actual native callback body absent";
    return false;
  }
  return v->mix_owner(v->owner, v->object, e);
}
inline bool podunk_audio_stream_player_2d_mix(PodunkAudioVoiceCallback *v,
                                              std::string &e) {
  if (!v || !v->object || !v->registry || !v->owner || !v->mix_owner) {
    e = "AudioStreamPlayer2D actual native callback body absent";
    return false;
  }
  return v->mix_owner(v->owner, v->object, e);
}
struct PodunkNativeAudioCallback {
  PodunkAudioMixFunction function = nullptr;
  PodunkAudioVoiceCallback *userdata = nullptr;
};
} // namespace encore::ctr
