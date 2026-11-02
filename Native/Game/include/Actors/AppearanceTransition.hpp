#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <span>

// Model palette effects: the targeting palette pulse and the death,
// disappearance and appearance transitions that fade a model's palettes. The
// model runtime owns the palettes, equipment and CLUT rows; these policies
// reach them only through the subjects below.
namespace VS::Actors {
inline constexpr int kActorPaletteColors = 160;
inline constexpr int kSmallPaletteColors = 16;
inline constexpr int kPalettePulsePeak = 16;
inline constexpr int kAppearanceFadeMaximum = 64;
inline constexpr std::size_t kRgbChannels = 3;

// Transition states (the actor's 4-bit action animation state).
inline constexpr int kAppearanceDeathFade = 2;
inline constexpr int kAppearanceDeathDarken = 3;
inline constexpr int kAppearanceDisappearance = 4;
inline constexpr int kAppearanceDelay = 6;
inline constexpr int kAppearanceDarken = 7;
inline constexpr int kAppearanceWhite = 9;
inline constexpr int kAppearanceRestore = 10;
inline constexpr int kAppearanceRestoreDone = 11;
inline constexpr int kAppearanceSlowDarken = 12;
inline constexpr int kAppearanceBlack = 14;

// The palette pulse's level and direction flags.
struct PalettePulse {
  std::uint8_t level = 0;
  std::uint8_t flags = 0;
};

// Steps the pulse two levels toward 16 or 0, twice at tick speed 4, and
// returns the untruncated level the colors use.
[[nodiscard]] int AdvancePalettePulse(PalettePulse& pulse, int tick_speed);
// One 15-bit color brightened toward blue by the pulse level; level 16 is the
// peak highlight.
[[nodiscard]] std::uint16_t PulsePaletteColor(std::uint16_t color, int level);
// Advances the pulse and writes the pulsed color of every non-zero `source`
// color to the same index of `destination`, which must be at least as long.
// Returns the level (func_800AB788).
int UpdatePalettePulse(PalettePulse& pulse, int tick_speed,
                       std::span<const std::uint16_t> source,
                       std::span<std::uint16_t> destination);

// Steps each RGB channel by `signed_step` toward `target`, wrapping as bytes
// before the clamp, and returns whether any channel was short of it.
bool StepRgbTowardTarget(std::span<std::uint8_t, kRgbChannels> current,
                         std::span<const std::uint8_t, kRgbChannels> target,
                         int signed_step);

enum class PaletteTransition : std::uint8_t {
  kRemoveRedGreen = 0,
  kDarken = 1,
  kWhite = 2,
  kBrightenGreenBlue = 3,
  kRestore = 4,
  kSlowDarken = 5,
  kBlack = 6,
};

// The model's transition color, fade level and the tint it returns to.
struct AppearanceColors {
  std::array<std::uint8_t, kRgbChannels> rgb{};
  // Frames spent in the appearance delay.
  std::uint8_t delay = 0;
  // Signed fade level, 0 to kAppearanceFadeMaximum.
  std::uint8_t fade_level = 0;
  // Signed fade level the restore returns to.
  std::uint8_t fade_target = 0;
  std::array<std::uint8_t, kRgbChannels> tint{};
};

struct AppearanceTransition {
  int state = 0;
  // Palette and fade step per update.
  int speed = 0;
  AppearanceColors colors;
};

// A model whose palettes the transition fades.
class AppearanceSubject {
 public:
  virtual ~AppearanceSubject() = default;
  // Applies `mode` to the model's (and its equipment's) transition palettes
  // and returns whether any color is still changing.
  [[nodiscard]] virtual bool ApplyPalettes(PaletteTransition mode,
                                           int speed) = 0;
  // Uploads the transition palettes; equipment records `state`.
  virtual void UploadPalettes(int state) = 0;
  // Equipment records the state the transition advanced to.
  virtual void PublishState(int state) = 0;
  // Drops a gimmick the model is carrying, if any.
  virtual void ReleaseCarriedGimmick() = 0;
};

// Advances one transition step: applies the state's palette mode, uploads the
// palettes and moves to the next state once nothing changes.
void AdvanceAppearanceTransition(AppearanceTransition& transition,
                                 AppearanceSubject& subject);

// An actor whose death transition also settles its fall.
class ActorAppearance : public AppearanceSubject {
 public:
  [[nodiscard]] virtual int TransitionState() const = 0;
  // The actor's transition after its death motion has settled; the speed is
  // the actor's step per update.
  [[nodiscard]] virtual AppearanceTransition LoadTransition() const = 0;
  virtual void StoreTransition(const AppearanceTransition& transition) = 0;
  [[nodiscard]] virtual int MovementMode() const = 0;
  [[nodiscard]] virtual bool IsPlayer() const = 0;
  // Pushes the actor off walls, halves its root motion and runs its fall.
  virtual void SettleDeathMotion() = 0;
  // Plays the falling death animation when the model has one.
  virtual void PlayFallingDeathAnimation() = 0;
};

// Settles a dying actor's motion before the delay state, then loads, advances
// and stores its transition (func_800AB9A4).
void UpdateActorAppearanceTransition(ActorAppearance& actor);
}  // namespace VS::Actors
