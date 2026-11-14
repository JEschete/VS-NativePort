#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_MENU_MAGICMENUS_H
#define VS_OWNED_MENU_MAGICMENUS_H
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/Sphere.h"
#include "Content/StringTables.h"
#include "Core/Math/ScalarMath.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Field/RoomStateLive.hpp"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Magic.h"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPrimitives.h"
#include "State/StateFlags.h"

int Menu_Magic_UpdateWarlockMenu(u_int initShortcutInvoked);

int Menu_Magic_UpdateShamanMenu(u_int initShortcutInvoked);

int Menu_Magic_UpdateSorcererMenu(u_int initShortcutInvoked);

int Menu_Magic_UpdateEnchanterMenu(u_int initShortcutInvoked);

#endif  // VS_OWNED_MENU_MAGICMENUS_H
