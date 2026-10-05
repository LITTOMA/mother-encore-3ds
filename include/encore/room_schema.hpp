#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace encore::upstream {
// Disk schema constants and supported execution enums only. No game content.
constexpr uint32_t kRoomNoIndex=0xffffffffu;
constexpr uint16_t kRoomNoActor=0xffffu;
constexpr size_t kMaxRoomBytes=1024*1024;
constexpr uint32_t kRoomHeaderBytes=128, kRoomDirectoryEntryBytes=24;
constexpr uint16_t kRoomSectionCount=28;
enum class RoomSection : uint16_t {
    StringRef=1, StringBytes, Resource, Vertex, Polygon, BodyRule, Overlay,
    MapDraw, Clip, Key, DirectionFrame, ActorProfile, ActorInstance, CameraArea,
    Flag, InitialFlag, Condition, Trigger, Program, Command, Binding, Battle,
    Rule, Experience, Scene, AnimationBinding, MovementPath, MovementPathEntry
};
constexpr std::array<uint32_t,kRoomSectionCount> kRoomStrides{{
    8,1,56,8,32,16,32,24,20,8,2,80,40,24,16,8,8,32,20,48,32,32,16,4,80,8,24,24
}};
enum class RoomRuleKey : uint16_t {
    WalkSpeed=1, RunSpeed, MovementDivisor, ActorTurnRadians, ActorDefaultMoveSpeed,
    ActorJumpAscentRatio, ActorJumpDescentRatio, ActorShakeCyclesPerSecond,
    ActorShakeHalfStepSeconds, ActorDefaultTurnSeconds, CameraReturnSeconds,
    CameraSmallMagnitude, CameraShakeStepSeconds, CameraShakeWeight,
    CameraMinimumMagnitude, CameraZeroMagnitudeThreshold, CameraRecoverySeconds,
    CameraDefaultMoveSeconds, CameraDefaultShakeSeconds, ActorCollisionSafeMargin,
    RoomShakeWaitSeconds, RoomShakeWaitMarginSeconds, RoomShakeLengthSeconds,
    RoomShakeDirectionX, RoomShakeDirectionY, ActorLoopShakeSeconds, ActorJumpRepeatDelaySeconds
};
enum class RoomScalarType : uint16_t { F32=1, F64=2, U32=3 };
enum class RoomResourceKind : uint16_t { Texture=1, AudioRequestOnly=2, CheckedBattlePack=3,CheckedWorldEffectPack=4 };
enum class RoomBindingKind : uint16_t { PlayMusicRequest=1, DelayedUnsupportedBoundary=2, PeriodicCameraShake=3, StopRoomShaker=4,StopMusicResource=5,WorldEffectAppear=6,WorldEffectDisappear=7,StartPhoneRing=8,DeferredFlagBodyDeletion=9, BasementWhiteFade=10,StopMusicRegion=11,PlayMusicRegion=12 };
}
