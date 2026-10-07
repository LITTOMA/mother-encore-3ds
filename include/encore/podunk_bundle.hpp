#pragma once
#include "encore/field_data.hpp"
#include <array>
#include <string>
#include <vector>
namespace encore::upstream {
// Software schema roles, not game filenames or game content.
enum class PodunkPackRole : uint32_t {
  BattleBackgroundResources = 1,
  CharacterLoad = 2,
  GlobalChildReady = 3,
  GlobalDataConstructor = 4,
  GlobalDataMembers = 5,
  GlobalItemDefinitions = 6,
  GlobalFlags = 7,
  GlobalLoad = 8,
  NativeRoot = 9,
  GlobalNodeConstructor = 10,
  PackedDirectory = 11,
  GlobalReady = 12,
  GlobalRegistry = 13,
  UiManager = 14,
  UiPreloads = 15,
  YamlCaches = 16,
  YamlFile = 17,
  HouseGlobalBridge = 18,
  UiContinuation = 19,
  PlayerChildScripts = 20,
  PlayerEffects = 21,
  PlayerFetcher = 22,
  PlayerGraphics = 23,
  PlayerInitialization = 24,
  PlayerMotion = 25,
  PlayerReady = 26,
  PlayerResources = 27,
  PlayerVisualScripts = 28,
  Butterfly = 29,
  CutsceneArea = 30,
  DeadBush = 31,
  DialogueAudio = 32,
  DialogueLifecycle = 33,
  DialogueRootScript = 34,
  DialogueUi = 35,
  DialogueVisual = 36,
  DoorNpc = 37,
  DroppedItem = 38,
  Emotes = 39,
  Enemy = 40,
  InteractDialog = 41,
  MelodyBackground = 42,
  MusicChanger = 43,
  Npc = 44,
  PlayerTransitions = 45,
  Present = 46,
  Programme = 47,
  Prompt = 48,
  SceneActions = 49,
  SpriteBridge = 50,
  SteppingSounds = 51,
  CharacterTint = 52,
  CameraArrows = 53,
  Birds = 54,
  CameraArea = 55,
  CanvasArt = 56,
  Dandelion = 57,
  Door = 58,
  Grass = 59,
  FieldItemDetails = 60,
  Geometry = 61,
  FieldItemDefinitions = 62,
  Map = 63,
  SceneLifecycle = 64,
  GameCamera = 65,
  Goods = 66,
  Inventory = 67,
  NodeTree = 68,
  OpenableDoor = 69,
  Payphone = 70,
  Sparkles = 71,
  NativeTimers = 72,
  VendingMachine = 73,
  DialogueNodeRecipe = 74,
  Shop = 75,
  HouseExitDoor = 76,
  HouseStatusEffects = 77,
  SceneNativeAudio = 78,
  SceneSignalCallbacks = 79,
  SceneVisibility = 80,
  SceneNpcWorld = 81,
  ScenePromptNative = 82,
  AudioServer = 83,
  SceneMaterials = 84,
  SceneClipNative = 85,
  SceneLeafNative = 86,
  GrassNative = 87,
  PlayerPreloadScenes = 88,
  NamedSfx = 89,
  DialogueActorResource = 90,
  HouseReentry = 91,
  HouseGeometry = 92,
  HouseNodeTree = 93,
  HouseReturnLadder = 94,
  HouseNpc = 95,
  HouseNpcWorld = 96,
  HouseNativeTimers = 97,
  HouseVisibility = 98,
  HouseSprites = 99,
};
enum class PodunkAssetKind : uint32_t {
  Texture = 1,
  Pcm = 2,
  GpuProgram = 3,
  Font = 4,
  AudioBank = 5,
  MusicBank = 6
};
struct PodunkBundleFile {
  uint32_t id = 0, kind = 0, size = 0, crc32 = 0;
  std::string path;
  std::array<uint8_t, 32> sha256{};
};
struct PodunkBundlePack : PodunkBundleFile {
  PodunkPackRole role{};
  // kind 1 is header identity; kind 2 is checked source context of a legacy
  // format.
  uint32_t format = 0, family = 0, capability = 0, rules = 0;
  FieldIdentity identity{};
  std::array<uint8_t, 32> ir_sha256{};
  std::vector<uint8_t> original_header;
};
struct PodunkScriptBinding {
  uint32_t source_role = 0, pack_role = 0;
};
// Immutable bundle ownership. Reading does not execute or admit any lifecycle.
class PodunkBundleData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  const FieldIdentity &identity() const { return identity_; }
  const std::string &source_scene() const { return scene_; }
  const std::vector<PodunkBundlePack> &packs() const { return packs_; }
  const std::vector<PodunkBundleFile> &assets() const { return assets_; }
  const std::vector<PodunkScriptBinding> &script_bindings() const {
    return scripts_;
  }
  const PodunkBundlePack *entry(PodunkPackRole) const;
  bool read(PodunkPackRole, const std::string &romfs_root,
            std::vector<uint8_t> &owner, std::string &) const;
  bool read_asset(uint32_t, const std::string &, std::vector<uint8_t> &,
                  std::string &) const;
  bool verify_files(const std::string &romfs_root, std::string &) const;
  std::vector<std::string> stage_paths() const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::string scene_;
  std::vector<PodunkBundlePack> packs_;
  std::vector<PodunkBundleFile> assets_;
  std::vector<PodunkScriptBinding> scripts_;
};
} // namespace encore::upstream
