// Standalone worker linkage: doubles external UI/audio/projection boundaries,
// never LimbControllers.cpp or its production Live adapters. The coordinator
// links StatusLimbControllersLiveTests.cpp against production dependencies.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <span>

#include "Actors/SkeletonLive.hpp"
#include "Field/MapLive.hpp"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPacketsLive.hpp"
extern "C" {
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Battle/TargetingState.h"
#include "Core/Diagnostics.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/TextBox.h"

Battle_Actor* Battle_actors[42] { };
u_int Battle_limbStatusColors[10] { };
u_short Battle_statusIconTexOffsets[32] { };
u_short Battle_upperStatusIconFlags[10] { };
char Battle_hudHidden = 0;
char Battle_hudBattleMode = 0;
char Battle_lowerScreenUiState = 0;
char Battle_shortcutHudRequested = 0;
char* Battle_limbNames[20] { };
u_short Battle_statusStrings[1712] { };
Menu_StringContext Battle_stringContext { };
u_short Menu_Status_statusStrings[1760] { };
char* Menu_Status_limbStatuses[6] { };
int Menu_Status_selectedActor = 0;
u_char Menu_Status_selectedElement = 0;
int Menu_Status_xPos = 0;
int Menu_Status_yPos = 0;
int Menu_Status_fadeScreen = 0;
int Menu_Status_drawBackgroundFirst = 0;
char Menu_Framework_cursorColor = 0;
char Menu_Framework_equipmentSubtype = 0;
char* Menu_Framework_stringBuf = nullptr;
Menu_Framework_MenuMemory Menu_Framework_menuMemory { };
char Menu_Framework_rowAnimationSteps[36] { };
int16_t Menu_Framework_equipmentStats[kMenuEquipmentStatCount] { };
int16_t Menu_Framework_strIntAgi[kMenuPrimaryStatCount] { };
int16_t Menu_Framework_comparisonStatDeltas[kMenuComparisonStatCount] { };
int16_t Menu_Framework_comparisonStatTotals[kMenuComparisonStatCount] { };
int16_t Menu_Framework_comparisonAccessoryStats[kMenuComparisonStatCount] { };
Menu_TextBoxState Menu_textBoxes[8] { };
Input_StickPosition Input_stickPosBuf { };
Input_PressedButtonsView Input_buttonsPressed { };
int Input_buttonsState = 0;
int Input_buttonRepeat = 0;
void Core_Warnf(const char* channel, const char* format, ...)
{
    std::fprintf(stderr, "[%s] ", channel);
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::fputc('\n', stderr);
}
void Core_Errorf(const char*, const char*, ...)
{
    std::fputs("unexpected HUD input refusal\n", stderr);
}
}
namespace VS::Menu {
void PlayChangeSound() { }
}
namespace VS::Menu::Framework {
void SetLiveActorComparisonStats(int actor)
{
    Menu_Framework_comparisonStatDeltas[3] = static_cast<std::int16_t>(actor);
}
void SetLiveInformationMessageText(std::span<const std::uint8_t>) { }
int DrawLiveCursor(u_int step, int) { return static_cast<int>(step + 1); }
}
namespace VS::Actors {
int ProjectLiveActorBone(int, int, int*, int) { return -1; }
}
namespace VS::Field {
int SetLiveMinimapVisible(int) { std::abort(); }
}
namespace VS::Render {
u_long* LiveFrameOrderingEntry(LiveOrderingBase, int) { return nullptr; }
std::uint32_t LivePacketQueue::ChainLink() const
{
    return chain_ == nullptr ? 0 : *chain_;
}
bool LivePacketQueue::CanEmit(std::size_t) const { return false; }
bool LivePacketQueue::Emit(std::span<const std::uint32_t>) { return false; }
void DrawLiveCompactTextColor(const char*, int, int, u_long*) { }
void DrawLiveCompactText(const char*, int, void*) { }
}
