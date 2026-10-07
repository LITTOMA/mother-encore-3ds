#pragma once
#include "encore/audio_server.hpp"
#include "encore/field_object_signals.hpp"
#include "podunk_player_native_media.hpp"
namespace encore::ctr {
// Same AudioServer ObjectDB owner for Player and Podunk source voices. The
// audio pump is explicitly called by the existing shared backend owner.
class PodunkAudioServer final : public upstream::FieldGlobalNativeObject {
public:
  static bool create(const upstream::AudioServerData &,
                     upstream::FieldGlobalRegistry &,
                     upstream::FieldObjectSignals &, AudioPlayer &,
                     PodunkAudioServer *&, std::string &);
  upstream::FieldGlobalExternalBinding binding() const override;
  const char *native_class() const override { return "AudioServer"; }
  const upstream::FieldGlobalRegistry *registry() const override {
    return registry_;
  }
  bool checked_source_hash(std::string_view,
                           std::array<uint8_t, 32> &) const override;
  bool alive() const override;
  PodunkPlayerMediaHost media_host();
  bool pump(std::string &);
  bool signal_declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                          std::string &) const;
  bool handles_layout(const upstream::FieldDeferredMessage &) const;
  bool layout_callback(const upstream::FieldDeferredMessage &, std::string &);
  bool set_bus_volume(uint32_t, float, std::string &);
  int get_bus_index(std::string_view) const;
  const upstream::AudioServerRuntime &body() const { return body_; }

private:
  struct Key {
    PodunkAudioMixFunction function = nullptr;
    PodunkAudioVoiceCallback *userdata = nullptr;
    bool operator<(const Key &b) const {
      return function == b.function
                 ? std::less<PodunkAudioVoiceCallback *>{}(userdata, b.userdata)
                 : std::less<PodunkAudioMixFunction>{}(function, b.function);
    }
  };
  bool add(upstream::FieldObjectId, PodunkNativeAudioCallback, std::string &);
  bool remove(upstream::FieldObjectId, std::string &);
  bool connect_layout(upstream::FieldObjectId, std::function<bool()>,
                      std::string &);
  bool disconnect_layout(upstream::FieldObjectId, std::string &);
  bool live(std::string &) const;
  bool callback_valid(upstream::FieldObjectId, PodunkNativeAudioCallback,
                      std::string &) const;
  const upstream::AudioServerData *data_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  AudioPlayer *audio_ = nullptr;
  upstream::FieldGlobalExternalBinding binding_{};
  upstream::AudioServerRuntime body_;
  std::map<Key, upstream::FieldObjectId> callbacks_;
  std::map<upstream::FieldObjectId, Key> by_object_;
  std::map<upstream::FieldObjectId, std::function<bool()>> layout_;
  bool pumping_ = false;
};
} // namespace encore::ctr
