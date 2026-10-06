// Manual source admission cases only. Not registered or executed by default.
#include "podunk_dialogue_host.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
std::vector<uint8_t> bytes(const char *path) {
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
uint32_t word(const std::vector<uint8_t> &b, size_t i) {
  return uint32_t(b[i]) | uint32_t(b[i + 1]) << 8 | uint32_t(b[i + 2]) << 16 |
         uint32_t(b[i + 3]) << 24;
}
} // namespace
int main() {
  const auto source = bytes("romfs/data/dialogue.encnoderecipe");
  assert(source.size() > 128);
  FieldIdentity identity;
  identity.scene_id = word(source, 36);
  std::copy_n(source.begin() + 40, 20, identity.upstream_commit.begin());
  std::copy_n(source.begin() + 60, 32, identity.source_sha256.begin());
  FieldNodeRecipeData recipe;
  FieldDialogueUiData ui;
  FieldDialogueVisualData visual;
  FieldDialogueAudioData audio;
  FieldNativeTimerData timers;
  std::string error;
  assert(recipe.load(source.data(), source.size(), identity, error));
  assert(ui.load_file("romfs/data/podunk-dialogue-ui.encdui", identity, error));
  assert(visual.load_file("romfs/data/podunk-dialogue-visual.encdvisual",
                          identity, error));
  assert(audio.load_file("romfs/data/podunk-dialogue-audio.encdaudio", identity,
                         error));
  assert(timers.load_file("romfs/data/podunk.enctimers", error));
  // UI retains all 47 source descriptors to prove the 27 pending foreign
  // bodies. Membership alone must not grant those bodies a second UI owner.
  size_t owned = 0, foreign = 0;
  for (const auto &n : recipe.records()) {
    const auto *u = ui.node(n.id);
    assert(u);
    const bool ours = u->kind != FieldDialogueUiKind::Pending;
    unsigned coverage = unsigned(ours) +
                        unsigned(visual.node(n.id) != nullptr) +
                        unsigned(audio.node(n.id) != nullptr) +
                        unsigned(timers.record(identity, n.id) != nullptr);
    assert(coverage == 1);
    owned += ours;
    foreign += !ours;
  }
  assert(owned == 20 && foreign == 27 && recipe.records().size() == 47);
  // The genuine packs are intentionally not enough to manufacture an actual
  // RootScript/Canvas/AudioServer. A new host rejects every public runtime
  // entry until those real source objects have been provided and admitted.
  encore::ctr::PodunkDialogueHost host;
  assert(!host.admit_factory(error));
  FieldNodeBinding binding;
  assert(!host.bind(1, recipe.records()[0], binding, error));
  assert(!host.dispatch(1, binding, FieldTreePhase::ReadyNative, error));
  assert(!host.admit_ready(1, 1, error));
  FieldDeferredMessage message;
  message.object = 1;
  message.member = "unknown";
  assert(!host.deferred(message, error));
  assert(!host.ui(1) && !host.visual(1) && !host.audio(1));
}
