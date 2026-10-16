"""Engine_01 types/fields from code read this session. Citations are resolved by exact text search, not typed."""
import csv, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "resources" / "rood-reverseWin" / "src"
OUT = ROOT / "tools" / "naming" / "sym_out" / "Engine_01"
FILES = {
    "main.c": "SLUS_010.40/main.c", "main.h": "SLUS_010.40/main.h", "146C.c": "BATTLE/BATTLE.PRG/146C.c",
    "146C.h": "BATTLE/BATTLE.PRG/146C.h", "30DB0.c": "BATTLE/BATTLE.PRG/30DB0.c", "30DB0.h": "BATTLE/BATTLE.PRG/30DB0.h",
    "40564.c": "BATTLE/BATTLE.PRG/40564.c", "5BF94.c": "BATTLE/BATTLE.PRG/5BF94.c", "2842C.h": "BATTLE/BATTLE.PRG/2842C.h",
    "3A1A0.h": "BATTLE/BATTLE.PRG/3A1A0.h", "18.c": "BATTLE/INITBTL.PRG/18.c", "260.c": "MENU/MENU7.PRG/260.c",
    "22C.c": "TITLE/TITLE.PRG/22C.c", "238.c": "MENU/MENU9.PRG/238.c",
}
_cache = {}


def cite(f, text, after=0):
    lines = _cache.setdefault(f, (SRC / FILES[f]).read_text(encoding="utf-8", errors="replace").splitlines())
    for i in range(after, len(lines)):
        if text in lines[i]:
            return f"source:{f}:{i + 1}"
    sys.exit(f"citation text not found in {f}: {text!r}")


def cites(*pairs):
    return ";".join(cite(*p) if isinstance(p, tuple) else p for p in pairs)


members = {}
for r in csv.DictReader(open(ROOT / "Docs" / "ledgers" / "FIELD_LEDGER.csv", encoding="utf-8")):
    members.setdefault(r["type_id"], []).append(r)

types, fields = [], []


def T(tid, role, name, loc, purpose, conf, notes, flds):
    types.append([tid, role, name, loc, purpose, conf, notes])
    order = members[tid]
    if [o["name"] for o in order] != [f[0] for f in flds]:
        sys.exit(f"{tid}: member order mismatch {[o['name'] for o in order]} vs {[f[0] for f in flds]}")
    for o, (orig, fname, meaning, fconf, fnotes) in zip(order, flds):
        fields.append([o["id"], fname, meaning, fconf, fnotes])


DECL = "declaration-only;"
G = "Native\\Graphics\\src\\"
C = "Native\\Content\\src\\"
S = "Native\\Save\\src\\MemoryCard\\"

T("T00122", "Value", "State::GameTimeComponents", "Native\\Game\\include\\State\\GameTime.h",
  "Splits elapsed game time into frame second minute and hour bytes.", "high",
  "same-as:T00190;identical layout indexed through a different include path;source:main.h:166", [
      ("f", "frames", "Frames within the current second", "med", DECL + "source:main.h:167"),
      ("s", "seconds", "Seconds within the current minute", "high", cites(("260.c", "vs_main_gametime.t.s / 10"))),
      ("m", "minutes", "Minutes within the current hour", "high", cites(("260.c", "vs_main_gametime.t.m / 10"))),
      ("h", "hours", "Elapsed hours", "high", cites(("260.c", "vs_main_gametime.t.h == 100"))),
  ])

T("T00184", "State", "Content::DiscRequestCounts", C + "DiscQueue.h",
  "Counts disc requests being read and requests still waiting in the queue.", "high",
  cites(("main.c", "if (*(int*)&_cdQueueCount != 0)")) + ";both counters are tested together as one word", [
      ("processing", "reading", "Requests handed to the drive and not yet marked loaded", "high",
       cites(("main.c", "++_cdQueueCount.processing;"), ("main.c", "--_cdQueueCount.processing;"))),
      ("queued", "waiting", "Requests enqueued and not yet started", "high",
       cites(("main.c", "slot->priority = _cdQueueCount.queued++;"), ("main.c", "--_cdQueueCount.queued;"))),
  ])

T("T00197", "State", "Render::PaletteEntryFade", G + "Palette.h",
  "Holds one palette entry's current 5-bit color, mask bit and per-channel fade table rows.", "high",
  cites(("main.c", "colors[i].rindex = (r - colors[i].r) + 31;")), [
      ("r", "red", "Current 5-bit red", "high", cites(("main.c", "colors[i].r = r;"))),
      ("g", "green", "Current 5-bit green", "high", cites(("main.c", "colors[i].g = g;"))),
      ("b", "blue", "Current 5-bit blue", "high", cites(("main.c", "colors[i].b = b;"))),
      ("a", "maskBit", "Semi-transparency bit written to bit 15", "high", cites(("main.c", "(colors[i].a << 15)"))),
      ("rindex", "redStepRow", "Target minus current red plus 31; selects the fade step table row", "high",
       cites(("main.c", "colors[i].rindex = (r - colors[i].r) + 31;"), ("main.c", "_animOffset16[index[j].rindex]"))),
      ("gindex", "greenStepRow", "Target minus current green plus 31; selects the fade step table row", "high",
       cites(("main.c", "colors[i].gindex = (g - colors[i].g) + 31;"), ("main.c", "_animOffset16[index[j].gindex]"))),
      ("bindex", "blueStepRow", "Target minus current blue plus 31; selects the fade step table row", "high",
       cites(("main.c", "colors[i].bindex = (b - colors[i].b) + 31;"), ("main.c", "_animOffset16[index[j].bindex]"))),
  ])

T("T00204", "State", "State::SceneTimer", "Native\\Game\\src\\State\\SceneTimer.h",
  "Runs the script timer that counts up or down and mirrors its time into state flags 0xA0 to 0xA3.", "high",
  cites(("146C.c", "vs_battle_setStateFlag(0xA0, D_80050468.frames);")), [
      ("unk0", "mode", "0 stopped 1 counts up any other value counts down", "high",
       cites(("146C.c", "if (D_80050468.unk0 == 1) {"), ("146C.c", "D_80050468.unk0 = arg0;"))),
      ("unk1", "pausedMode", "Mode saved while the timer is paused and restored on resume", "high",
       cites(("146C.c", "D_80050468.unk1 = temp_v1;"), ("146C.c", "D_80050468.unk0 = D_80050468.unk1;"))),
      ("unk2", "resetScope", "Resets on room load (0) zone load (1) or scene transition (2)", "high",
       cites(("146C.c", "if (D_80050468.unk2 == 0) {"), ("146C.c", "if (D_80050468.unk2 == 1) {"),
             ("146C.c", "if (D_80050468.unk2 == 2) {"))),
      ("frames", "frames", "Frames part of the timer", "high", cites(("146C.c", "D_80050468.frames += vs_gametime_tickspeed;"))),
      ("seconds", "seconds", "Seconds part of the timer", "high", cites(("146C.c", "if (++D_80050468.seconds > 59) {"))),
      ("minutes", "minutes", "Minutes part of the timer", "high", cites(("146C.c", "if (++D_80050468.minutes > 59) {"))),
      ("hours", "hours", "Hours part of the timer", "high", cites(("146C.c", "if (--D_80050468.hours < 0) {"))),
  ])

T("T00205", "Value", "Render::ColorRgb8", G + "Palette.h",
  "Stores an 8-bit red green blue color with one padding byte.", "high",
  cites(("main.c", "*((int*)&_clutState.unk7F28[i].unk28) = 0x404040;")), [
      ("r", "red", "Red component", "high", cites(("main.c", "r = _shift_left(arg1[i].r);"))),
      ("g", "green", "Green component", "high", cites(("main.c", "g = _shift_left(arg1[i].g);"))),
      ("b", "blue", "Blue component", "high", cites(("main.c", "b = _shift_left(arg1[i].b);"))),
      ("unk3", "pad3", "Padding byte cleared by whole-word stores", "med",
       cites(("main.c", "*((int*)&_clutState.unk7F28[i].unk28) = 0x404040;")) + ";no member access;source:main.h:381"),
  ])

T("T00206", "State", "Render::ModelColorFade", G + "Palette.h",
  "Fades the room tint (slot 0) or one actor tint (slots 1-16) toward a target color with optional pulsing.", "high",
  cites(("main.c", "func_800A0768(i - 1, &sp18);"), ("main.c", "func_8008B430(&sp18, p[0].unk6[3] >> 2);")), [
      ("unk0", "progress", "Bytes: [0] active [1] frame [2] tick [3] half duration [4] transition mode", "high",
       cites(("main.c", "temp_t0->unk0[0] = 1;"), ("main.c", "++p[i].unk0[1];"), ("main.c", "if (p[i].unk0[4] == 9) {"))),
      ("unk6", "blendParams", "Shorts: [0..2] pulse target [3] tint strength [4] strength step per frame", "high",
       cites(("main.c", "t1->unk6[0] = arg3;"), ("main.c", "t1->unk6[4] = ((var_t4 - t1->unk6[3]) / (arg1 * 2));"))),
      ("unk10", "colorFixed", "Current red green blue in 16.16 then the per-frame step of each", "high",
       cites(("main.c", "temp_a3 = _clutState.unk7F28[arg1].unk10;"), ("main.c", "a2[0] += a2[3];"))),
      ("unk28", "baseColor", "Base color that relative transitions are computed from", "high",
       cites(("main.c", "_clutState.unk7F28[arg1].unk28 = *arg2;"), ("main.c", "var_a1 = (t1->unk28.r + arg3) << 0x10;"))),
  ])

T("T00207", "State", "State::AshleySnapshot", "Native\\Game\\src\\State\\Ashley.h",
  "Captures Ashley's statistics and limbs for saves and room transitions.", "high",
  cites(("260.c", "_rMemcpy(&savedata->unk6C8, &D_80060068, sizeof savedata->unk6C8);")), [
      ("unk0", "stats", "Ashley's statistics status and limb records", "high",
       cites(("260.c", "s5->stats.currentHP = D_80060068.unk0.currentHP;"))),
      ("unkDC", "reservedDC", "Unread words completing the 256-byte snapshot", "low", DECL + "source:main.h:235"),
  ])

T("T00222", "Value", "Field::DoorFlagIdWord", "Native\\Game\\src\\Field\\Doors.h",
  "Packs the door state-flag id copied into the saved entry door.", "med",
  cites(("main.c", "D_80061068.unk6_0 = D_800F18E8.unk0_0;")), [
      ("unk0_0", "requiredFlagId", "State flag id copied to the saved door's requiredFlagId", "med",
       cites(("main.c", "D_80061068.unk6_0 = D_800F18E8.unk0_0;"))),
      ("unk0_10", "reservedBits10", "Upper six bits; no access found", "low", DECL + "source:146C.h:730"),
  ])

T("T00228", "Value", "Render::FramebufferEffectTranslation", G + "FramebufferEffects.h",
  "Holds the screen translation applied to the captured framebuffer.", "high",
  cites(("146C.c", "translation.vx = D_800F1A68.unk0 + 160;")), [
      ("unk0", "x", "Horizontal offset added to the screen center", "high",
       cites(("146C.c", "translation.vx = D_800F1A68.unk0 + 160;"), ("146C.c", "sprite->x0 = D_800F1A68.unk0 + i * 64;"))),
      ("unk4", "y", "Vertical offset added to the screen center", "high",
       cites(("146C.c", "translation.vy = D_800F1A68.unk4 + 112;"), ("146C.c", "sprite->y0 = D_800F1A68.unk4;"))),
      ("unk8", "reserved8", "Cleared with the effect and never read", "low", cites(("146C.c", "D_800F1A68.unk8 = 0;"))),
      ("unkC", "reservedC", "No access found", "low", DECL + "source:146C.h:725"),
  ])

T("T00229", "Packet", "Render::LegacyFramebufferEffectPackets", G + "FramebufferEffects.h",
  "Holds the PS1 packets that redraw the captured screen as five 64-pixel strips per frame buffer.", "high",
  cites(("146C.c", "&D_800F1ABC->drawModes[(vs_main_frameBuf * 5) + i]);")) + ";replaced-by:native framebuffer effect pass", [
      ("poly", "rotatedStrips", "Textured quads used when the screen is rotated or scaled", "high",
       cites(("146C.c", "SetPolyFT4(&D_800F1ABC->poly[0]);"))),
      ("sprite", "flatStrips", "Sprites used when the screen is drawn unrotated", "high",
       cites(("146C.c", "SetSprt(&D_800F1ABC->sprite[0]);"))),
      ("drawModes", "drawModes", "Texture page draw modes for each strip", "high",
       cites(("146C.c", "SetDrawMode(&D_800F1ABC->drawModes[(vs_main_frameBuf * 5) + i], 0, 1,"))),
  ])

T("T00243", "State", "Actors::TextureScrollState", "Native\\Game\\src\\Actors\\TextureAnimation.h",
  "Tracks one scrolling texture window of an actor model.", "high",
  cites(("40564.c", "scroll = &actor->unk181C[i];")), [
      ("x", "x", "Current horizontal source position", "high", cites(("30DB0.c", "state->x = bounds->x;"), ("40564.c", "scrollX = scroll->x;"))),
      ("y", "y", "Current vertical source position", "high", cites(("30DB0.c", "state->y = bounds->y;"), ("40564.c", "scrollY = scroll->y;"))),
      ("dx", "stepX", "Horizontal step; negated when bouncing", "high", cites(("40564.c", "dx = scroll->dx;"), ("40564.c", "scroll->dx = -scroll->dx;"))),
      ("dy", "stepY", "Vertical step; negated when bouncing", "high", cites(("40564.c", "dy = scroll->dy;"), ("40564.c", "scroll->dy = -scroll->dy;"))),
      ("interval", "interval", "Ticks between steps", "high", cites(("40564.c", "if (scroll->timer <= scroll->interval) {"))),
      ("timer", "timer", "Ticks since the last step", "high", cites(("40564.c", "scroll->timer += vs_gametime_tickspeed / 2;"))),
      ("unk6", "pad6", "No access found", "low", DECL + "source:3A1A0.h:240"),
      ("unk7", "pad7", "No access found", "low", DECL + "source:3A1A0.h:241"),
  ])

T("T00250", "Def", "Actors::TextureScrollBounds", "Native\\Game\\src\\Actors\\TextureAnimation.h",
  "Defines the window, initial step, interval and edge behavior of one actor texture scroll.", "high",
  cites(("30DB0.c", "bounds = &actor->unk61C[i];")), [
      ("x", "x", "Window left edge", "high", cites(("40564.c", "minX = bounds->x;"))),
      ("y", "y", "Window top edge", "high", cites(("40564.c", "maxY = bounds->y + bounds->h;"))),
      ("w", "width", "Window width; zero disables the scroll", "high", cites(("40564.c", "if (bounds->w == 0) {"), ("40564.c", "maxX = minX + bounds->w;"))),
      ("h", "height", "Window height", "high", cites(("40564.c", "maxY = bounds->y + bounds->h;"))),
      ("unk4", "initialStepX", "Horizontal step copied into the scroll state", "high", cites(("30DB0.c", "state->dx = bounds->unk4;"))),
      ("unk5", "initialStepY", "Vertical step copied into the scroll state", "high", cites(("30DB0.c", "state->dy = bounds->unk5;"))),
      ("unk6_0", "bounces", "Reverses direction at an edge instead of wrapping to the start", "high",
       cites(("40564.c", "if (bounds->unk6_0) {"))),
      ("unk6_1", "reservedBits1", "No access found", "low", DECL + "source:3A1A0.h:229"),
      ("unk7", "interval", "Ticks between steps copied into the scroll state", "high", cites(("30DB0.c", "state->interval = bounds->unk7;"))),
  ])

T("T00291", "State", "Movie::Intro::MdecState", "Native\\Video\\src\\Intro\\Playback.h",
  "Double-buffers encoded frames, decoded slices and frame rectangles for intro movie decoding.", "high",
  cites(("22C.c", "arg0->encodedData[0] = _encodedDataBuf0;")) + ";replaced-by:native movie decoder", [
      ("encodedData", "encodedBuffers", "Two buffers holding compressed frames", "high",
       cites(("22C.c", "arg0->encodedData[0] = _encodedDataBuf0;"))),
      ("encodedDataIndex", "encodedBufferIndex", "Encoded buffer being decoded", "high",
       cites(("22C.c", "movie->encodedDataIndex = movie->encodedDataIndex == 0;"))),
      ("decodedData", "decodedBuffers", "Two buffers receiving decoded slices", "high",
       cites(("22C.c", "arg0->decodedData[0] = _decodedDataBuf0;"))),
      ("decodedDataIndex", "decodedBufferIndex", "Decoded slice buffer in use", "high", cites(("22C.c", "arg0->decodedDataIndex = 0;"))),
      ("frameBufs", "frameRects", "Destination rectangles of the two frame buffers", "high",
       cites(("22C.c", "movie->frameBufs[1].w = (int)(temp_v1 + (temp_v1 >> 31)) >> 1;"))),
      ("frameBufIndex", "frameRectIndex", "Frame buffer rectangle being filled", "high",
       cites(("22C.c", "movie->frameBufIndex = movie->frameBufIndex == 0;"))),
      ("renderTarget", "sliceRect", "Rectangle of the slice currently uploaded", "med",
       cites(("22C.c", "arg0->renderTarget.x = x0;"), ("22C.c", "movie->renderTarget.h = _movieHeight;"))),
      ("frameComplete", "frameComplete", "Set when the current frame has been fully uploaded", "high",
       cites(("22C.c", "arg0->frameComplete = 0;"), ("22C.c", "int i = movie->frameComplete;"))),
  ])

T("T00293", "OnDisc", "Content::SystemDatHeader", C + "SystemResources.h",
  "Lists the byte offsets of the images, palette and interface data inside SYSTEM.DAT.", "high",
  cites(("18.c", "vs_main_diskLoadFile(VS_SYSTEM_DAT_LBA, VS_SYSTEM_DAT_SIZE, buf);")), [
      ("unk0", "image0Offset", "First rectangle-headed image", "high", cites(("18.c", "p_hdr = (void*)buf + buf->unk0;"))),
      ("unk4", "image1Offset", "Second rectangle-headed image", "high", cites(("18.c", "p_hdr = (void*)buf + buf->unk4;"))),
      ("unk8", "paletteOffset", "Palette block loaded with vs_main_loadClut", "high", cites(("18.c", "p_hdr = (void*)buf + buf->unk8;"))),
      ("unkC", "image2Offset", "Third rectangle-headed image", "high", cites(("18.c", "p_hdr = (RECT*)((signed char*)buf + buf->unkC);"))),
      ("unk10", "image3Offset", "Fourth rectangle-headed image", "high", cites(("18.c", "p_hdr = (void*)buf + buf->unk10;"))),
      ("unk14", "image4Offset", "Fifth rectangle-headed image", "high", cites(("18.c", "p_hdr = (void*)buf + buf->unk14;"))),
      ("unk18", "fixedImageOffset", "Size-headed image loaded at VRAM 768 240", "high",
       cites(("18.c", "p_hdr = (void*)buf + buf->unk18;"), ("18.c", "setRECT(&rect, 0x300, 0xF0, p_hdr->x, p_hdr->y);"))),
      ("unk1C", "interfaceDataOffset", "Data passed to Menu::Framework::InitGameInterface", "high",
       cites(("18.c", "func_800CA9C0((void*)buf + buf->unk1C);"))),
  ])

T("T00307", "State", "Render::PaletteSlot", G + "Palette.h",
  "Holds one CLUT slot's lock, fade timer, per-entry fade data and base colors.", "high",
  cites(("main.c", "struct ColorInfo* colors = _clutState.slots[slot].colorInfo;")), [
      ("unk4", "reserved0", "First byte; no access found", "low", DECL + "source:main.c:35"),
      ("locked", "locked", "Prevents vs_main_loadClut from replacing the slot", "high",
       cites(("main.c", "if (_clutState.slots[slot].locked == 0) {"))),
      ("animState", "transition", "Fade timer of the slot", "high",
       cites(("main.c", "struct clutAnimState_t* state = &_clutState.slots[slot].animState;"))),
      ("colorInfo", "entries", "Per-entry fade data for 256 colors", "high",
       cites(("main.c", "struct ColorInfo* colors = _clutState.slots[slot].colorInfo;"))),
      ("clutBase", "baseColors", "Saved 15-bit colors that relative transforms start from", "high",
       cites(("main.c", "_clutState.slots[slot].clutBase[i] = sourceClut[i];"), ("main.c", "short baseEntry = _clutState.slots[slot].clutBase[i];"))),
  ])

T("T00308", "State", "Render::PaletteTransitionState", G + "Palette.h",
  "Holds every CLUT slot fade plus the room background and model tint fades.", "high",
  cites(("main.c", "if (_clutState.active != 0) {")), [
      ("uploadPending", "uploadPending", "Set when changed palette colors must be uploaded", "high",
       cites(("main.c", "_clutState.uploadPending = 1;"))),
      ("active", "enabled", "Gates every palette and tint transition request", "high", cites(("main.c", "if (_clutState.active != 0) {"))),
      ("slots", "slots", "Fourteen CLUT slots", "high", cites(("main.c", "_clutState.slots[i].locked = 0;"))),
      ("bgAnimActive", "backgroundFadeActive", "Background color fade in progress", "high", cites(("main.c", "_clutState.bgAnimActive = 1;"))),
      ("bgAnimFrame", "backgroundFadeFrame", "Frames elapsed in the background fade", "high", cites(("main.c", "_clutState.bgAnimFrame += 1;"))),
      ("bgAnimTick", "backgroundFadeTick", "Cleared when a background fade starts; never read", "med", cites(("main.c", "_clutState.bgAnimTick = 0;"))),
      ("bgAnimSpeed", "backgroundFadeSpeed", "Half the background fade length in frames", "high",
       cites(("main.c", "if (_clutState.bgAnimFrame >= (_clutState.bgAnimSpeed * 2)) {"))),
      ("bgTransitionMode", "backgroundFadeMode", "Transition mode; 9 and 10 alternate to pulse", "high",
       cites(("main.c", "if (_clutState.bgTransitionMode == 9) {"))),
      ("unk7E95", "pad7E95", "No access found", "low", DECL + "source:main.c:68"),
      ("bgTransitionTargetR", "backgroundPulseRed", "Red offset of the background pulse", "high", cites(("main.c", "_clutState.bgTransitionTargetR = arg2;"))),
      ("bgTransitionTargetG", "backgroundPulseGreen", "Green offset of the background pulse", "high", cites(("main.c", "_clutState.bgTransitionTargetG = arg3;"))),
      ("bgTransitionTargetB", "backgroundPulseBlue", "Blue offset of the background pulse", "high", cites(("main.c", "_clutState.bgTransitionTargetB = arg4;"))),
      ("bgColors", "backgroundColorsFixed", "Five background colors in 16.16 with per-frame steps", "high",
       cites(("main.c", "int (*temp_t6)[6] = _clutState.bgColors;"), ("main.c", "t1[i][0] += t1[i][3];"), ("main.c", "func_8008EB30(sp18);"))),
      ("unk7F14", "backgroundBaseColors", "Base background colors that relative transitions start from", "high",
       cites(("main.c", "_clutState.unk7F14[i] = arg1[i];"))),
      ("unk7F28", "modelColorFades", "Room tint fade then sixteen actor tint fades", "high",
       cites(("main.c", "D_8005DC80_t* p = _clutState.unk7F28;"))),
  ])

T("T00013", "Enum", "Content::DiscBufferMode", C + "DiscQueue.h",
  "Selects whether disc sectors go straight to the destination or through the sector ring buffer.", "high",
  cites(("main.c", "switch (vs_main_disk.bufferMode) {")), [
      ("cdBufferModeNone", "Direct", "Sectors are read straight into the destination", "high",
       cites(("main.c", "if (vs_main_disk.bufferMode == cdBufferModeNone) {"))),
      ("cdBufferModeBuffered", "Buffered", "Sectors go to the ring and are copied out each frame", "high",
       cites(("main.c", "if (vs_main_disk.bufferMode == cdBufferModeBuffered) {"))),
      ("cdBufferModeManual", "Manual", "Sectors go to the ring and the caller consumes them", "med",
       cites(("main.c", "case cdBufferModeManual:"))),
  ])

T("T00407", "State", "Render::PaletteTransitionTimer", G + "Palette.h",
  "Times one CLUT slot fade and stores its transition mode and pulse offsets.", "high",
  cites(("main.c", "struct clutAnimState_t* state = &_clutState.slots[slot].animState;")), [
      ("active", "active", "Fade in progress", "high", cites(("main.c", "state->active = 1;"))),
      ("frame", "frame", "Frames elapsed", "high", cites(("main.c", "state->frame = 0;"))),
      ("tick", "tick", "Sub-step counter", "high", cites(("main.c", "state->tick = 0;"))),
      ("speed", "speed", "Fade duration parameter", "high", cites(("main.c", "state->speed = duration;"))),
      ("transitionMode", "mode", "Color transform mode", "high", cites(("main.c", "p->transitionMode = mode;"))),
      ("transitionTargetR", "pulseRed", "Red offset of the transform", "high", cites(("main.c", "p->transitionTargetR = rOffset;"))),
      ("transitionTargetG", "pulseGreen", "Green offset of the transform", "high", cites(("main.c", "p->transitionTargetG = gOffsetU;"))),
      ("transitionTargetB", "pulseBlue", "Blue offset of the transform", "high", cites(("main.c", "p->transitionTargetB = bOffsetU;"))),
  ])

T("T00460", "OnDisc", "Content::SequenceBuffer", C + "ActorAssets.h",
  "Views a loaded SEQ animation file whose first two words are offsets rebased to addresses.", "med",
  cites(("30DB0.c", "vs_main_memcpy(arg0, arg1, 0x7800);")), [
      ("unk0", "firstSectionPointer", "First self-relative offset rebased to an address on load", "med",
       cites(("30DB0.c", "seq->unk0 = (char*)seq->unk0 + (int)seq;"))),
      ("unk4", "secondSectionPointer", "Second self-relative offset rebased to an address on load", "med",
       cites(("30DB0.c", "seq->unk4 = (char*)seq->unk4 + (int)seq;"))),
      ("unk8", "body", "Remaining sequence bytes", "med", DECL + "source:30DB0.h:8"),
  ])

T("T00499", "OnDisc", "Content::PFileTimelineTable", C + "PFile.h",
  "Lists the tracks of one P-file timeline section.", "high",
  cites(("5BF94.c", "func_800D55A4_t* entry = _getPFileBlock3section(arg0, arg1);")), [
      ("count", "trackCount", "Number of track entries", "high", cites(("5BF94.c", "arg0->unkD1C.unk30 = vs_main_allocHeapR(entry->count * 16 + 8);"))),
      ("unk4", "tracks", "Track entries", "high", cites(("5BF94.c", "return (char*)arg0 + entry->unk4[arg2].unk2;"))),
  ])

T("T00500", "OnDisc", "Content::PFileTimelineTrackEntry", C + "PFile.h",
  "Describes one P-file timeline track by packed header and data offset.", "high",
  cites(("5BF94.c", "p->unk8[i].unk4 = entry->unk4[i].unk0;")), [
      ("unk0", "header", "Packed track kind header copied into the playback state", "high",
       cites(("5BF94.c", "p->unk8[i].unk4 = entry->unk4[i].unk0;"))),
      ("unk2", "dataOffset", "Byte offset of the track data from the block start", "high",
       cites(("5BF94.c", "return (char*)arg0 + entry->unk4[arg2].unk2;"))),
  ])

T("T00542", "State", "Render::GimLayer", G + "Images.h",
  "Holds one of three GIM image layers: load phase, tile table, palette, placement and transition.", "high",
  cites(("5BF94.c", "var_s0 = vs_main_allocHeapR(sizeof *var_s0 * 3);")), [
      ("unk0_0", "drawEnabled", "Layer is drawn each frame", "high", cites(("5BF94.c", "gim->unk0_0 = 1;"), ("5BF94.c", "if (gim[j].unk0_0) {"))),
      ("unk0_1", "loading", "Set while the image file is loading", "high", cites(("5BF94.c", "var_s0->unk0_1 = 1;"), ("5BF94.c", "gim[j].unk0_1 = 0;"))),
      ("unk0_2", "textureMode", "Texture depth passed to getTPage", "high", cites(("5BF94.c", "tpage = getTPage(image->unk0_2, 1,"))),
      ("unk0_3", "drawRoutine", "Index of the transition draw routine", "high", cites(("5BF94.c", "D_800EC25C[gim->unk0_3](gim, clut << 16,"))),
      ("unk0_6", "loadPhase", "Load phase: waiting for disc unpacking freeing data ready", "high", cites(("5BF94.c", "switch (gim->unk0_6) {"))),
      ("unk0_9", "texturePageCount", "VRAM texture pages reserved for the tiles", "high", cites(("5BF94.c", "gim->unk0_9 = j;"))),
      ("unk0_12", "ownsPalette", "Layer uses the 256-color palette loaded with it", "high",
       cites(("5BF94.c", "current->unk0_12 = 1;"), ("5BF94.c", "if (gim->unk0_12) {"))),
      ("unk0_13", "reservedBits13", "No access found", "low", DECL + "source:5BF94.c:67"),
      ("unk2", "vramPage", "Base VRAM page used for tiles and palettes", "med",
       cites(("5BF94.c", "width = (gim->unk2 - 24) * 64;"), ("5BF94.c", "int clut = (gim->unk2 - 16) << 6;"))),
      ("unk3", "brightness", "Draw brightness; 128 draws without blending", "high",
       cites(("5BF94.c", "int alpha = image->unk3;"), ("5BF94.c", "if (temp_s0 == 0x80) {"))),
      ("columns", "columns", "Tile columns", "high", cites(("5BF94.c", "gim->columns = width;"))),
      ("rows", "rows", "Tile rows", "high", cites(("5BF94.c", "gim->rows = j;"))),
      ("id", "imageId", "GIM file index", "high", cites(("5BF94.c", "var_s0->id = id;"))),
      ("unk8", "offsetX", "Horizontal offset from the screen center", "high", cites(("5BF94.c", "int j = image->unk8 + 160;"))),
      ("unkA", "offsetY", "Vertical offset from the screen center", "high", cites(("5BF94.c", "j = image->unkA + 120;"))),
      ("unkC", "scaleAndOrder", "Scale in 1/4096 in bits 0-13; bit 14 selects the alternate ordering table", "high",
       cites(("5BF94.c", "int scale = image->unkC & 0x3FFF;"), ("5BF94.c", "gim->unkC & 0x4000 ? vs_scratch.unk4"))),
      ("unkE", "backdropColor", "15-bit color filling the screen around the image", "high", cites(("5BF94.c", "color = image->unkE;"))),
      ("data", "fileData", "Loaded GIM file", "high", cites(("5BF94.c", "count = gim->data[0];"))),
      ("cdQueueSlot", "discRequest", "Disc request loading the file", "high", cites(("5BF94.c", "if (gim->cdQueueSlot->state != 4) {"))),
      ("unk18", "scanlineWipeWidths", "Per-scanline wipe edge for the scanline-wipe routine", "high",
       cites(("5BF94.c", "u_short* p = gim->unk18;"), ("5BF94.c", "u_short* widths = image->unk18;"))),
      ("imageTable", "tileTable", "Header words then tile ids from the file", "high",
       cites(("5BF94.c", "gim->imageTable[i] = gim->data[i];"), ("5BF94.c", "u_short* tiles = &image->imageTable[4];"))),
      ("unk3F4", "reserved3F4", "No access found", "low", DECL + "source:5BF94.c:81"),
  ])

T("T00543", "OnDisc", "Render::LegacyImageSize", G + "Loading.h",
  "Stores the little-endian width and height that precede the NOW LOADING image pixels.", "high",
  cites(("main.c", "#define IMG_W(header) (header.wLo + (header.wHi << 8))")), [
      ("wLo", "widthLow", "Low byte of the width", "high", cites(("main.c", "#define IMG_W(header) (header.wLo + (header.wHi << 8))"))),
      ("wHi", "widthHigh", "High byte of the width", "high", cites(("main.c", "#define IMG_W(header) (header.wLo + (header.wHi << 8))"))),
      ("hLo", "heightLow", "Low byte of the height", "high", cites(("main.c", "#define IMG_H(header) (header.hLo + (header.hHi << 8))"))),
      ("hHi", "heightHigh", "High byte of the height", "high", cites(("main.c", "#define IMG_H(header) (header.hLo + (header.hHi << 8))"))),
  ])

T("T00547", "OnDisc", "Save::CardUiResources", S + "Resources.h",
  "Holds the memory-card menu resources loaded from SPMCIMG.BIN after three save images.", "high",
  cites(("260.c", "_mcData = (mcdata_t*)(_spmcimg + sizeof(savedata_t) * 3);")), [
      ("locationCluts", "locationPalettes", "Two palettes for the save-location artwork", "high",
       cites(("260.c", "locationClut = _mcData->locationCluts[1];"))),
      ("unk400", "fileIconPalettes", "Per-file 32-byte card icon palette copied into the save header", "med",
       cites(("260.c", "_rMemcpy(savedata3->fileInfo.unk60, _mcData->unk400[targetFile],")) + ";PS1 card header icon palette at 0x60"),
      ("unk4E0", "fileIconBitmaps", "Per-file 128-byte card icon bitmap copied after the header", "med",
       cites(("260.c", "_rMemcpy(savedata3->unk80, _mcData->unk4E0[targetFile], sizeof savedata->unk80);")) + ";PS1 card icon frame at 0x80"),
      ("unk860", "reserved860", "No access found", "low", DECL + "source:260.c:156"),
      ("unkBE0", "reservedBE0", "No access found", "low", DECL + "source:260.c:157"),
      ("unkFE0", "reservedFE0", "No access found", "low", DECL + "source:260.c:158"),
      ("textTable", "textTable", "Menu text table", "high", cites(("260.c", "_textTable = _mcData->textTable;"))),
      ("saveFileInfo", "fileHeaders", "Card header of each of the five save files", "high", cites(("260.c", "_saveFileInfo = _mcData->saveFileInfo;"))),
      ("_dirEntBuf", "directoryEntry", "Card directory entry buffer", "high", cites(("260.c", "_dirEntBuf = &_mcData->_dirEntBuf;"))),
  ])

T("T00025", "Enum", "Save::CardReadinessResult", S + "GameFiles.h",
  "Reports the progress of bringing a memory card to a usable state.", "high",
  cites(("260.c", "return memcardEventPending;")) + ";replaced-by:Save asynchronous completion", [
      ("memcardEventPending", "Pending", "Still working", "high", cites(("260.c", "return memcardEventPending;"))),
      ("memcardEventIoEnd", "Ready", "Card responded and is usable", "med", DECL + "source:260.c:40"),
      ("memcardEventTimeout", "Timeout", "Card did not respond", "high", cites(("260.c", "return memcardEventTimeout;"))),
      ("memcardEventUnformatted", "Unformatted", "Card is not formatted", "med", DECL + "source:260.c:42"),
  ])

T("T00091", "Enum", "Save::CardEventSignal", S + "GameFiles.h",
  "Identifies the first signalled memory-card event in a group.", "high",
  cites(("260.c", "if (TestEvent(_memcardEventDescriptors[i + type]) == 1) {")) + ";replaced-by:Save asynchronous completion", [
      ("memcardInternalEventIoEnd", "IoEnd", "Operation completed", "high", DECL + "source:260.c:29"),
      ("memcardInternalEventError", "Error", "Operation failed", "high", DECL + "source:260.c:30"),
      ("memcardInternalEventTimeout", "Timeout", "Card did not respond", "high", DECL + "source:260.c:31"),
      ("memcardInternalEventUnformatted", "Unformatted", "New or unformatted card", "high", DECL + "source:260.c:32"),
      ("memcardInternalEventNone", "None", "No event signalled", "high", cites(("260.c", "return i;")) + ";source:260.c:33"),
  ])

CONFLICT = "layout-conflict: channels are used as int[2][6] (48 bytes) but only 40 remain; an element likely starts at +8 with motor levels stored separately"
T("T00581", "State", "Input::VibrationPatternSlot", "Native\\Input\\src\\Vibration.h",
  "Plays one bytecoded vibration pattern on the two controller motors.", "med",
  cites(("main.c", "int (*a1)[6] = (int (*)[6])_padAct[j].unk10;")) + ";" + CONFLICT, [
      ("unk0", "motorLevels", "Summed motor levels; only slot 0's copy is used", "med",
       cites(("main.c", "_padAct[0].unk0[i] += a1[i][1];")) + ";" + CONFLICT),
      ("unk8", "active", "Pattern is playing", "high", cites(("main.c", "if (_padAct[i].unk8 == 0) {"))),
      ("unkA", "ownerTag", "Tag used to stop patterns started by one owner", "high", cites(("main.c", "if (_padAct[i].unkA == arg1) {"))),
      ("unkC", "pattern", "Pattern bytecode", "high", cites(("main.c", "temp_t1 = _padAct[j].unkC;"))),
      ("unk10", "channels", "Per motor: running level step target cursor frames-left", "med",
       cites(("main.c", "int (*a1)[6] = (int (*)[6])_padAct[j].unk10;")) + ";" + CONFLICT),
  ])

T("T00583", "Value", "Core::PercentSuffixText", "Native\\Core\\src\\Text\\Format.h",
  "Holds the encoded percent sign appended to percentages.", "high",
  cites(("238.c", "static percent_t percent = { \"%\\0\" };")), [
      ("data", "text", "Encoded percent sign and terminator", "high", cites(("238.c", "static percent_t percent = { \"%\\0\" };")) + ";source:238.c:2373"),
  ])

T("T00584", "State", "Input::ControllerPortState", "Native\\Input\\src\\Controllers.h",
  "Holds one controller port's type, stick positions, actuator levels and connection state.", "high",
  cites(("main.c", "portInfo_t* port = &_portInfo[portID >> 4];")), [
      ("mode", "controllerType", "Controller type from PadInfoMode; 5 and 7 report sticks", "high",
       cites(("main.c", "port->mode = PadInfoMode(portID, InfoModeCurID, 0);"), ("main.c", "if ((port->mode == 5) || (port->mode == 7)) {"))),
      ("exId", "extendedType", "Extended controller id; nonzero supports mode lock", "high",
       cites(("main.c", "port->exId = PadInfoMode(portID, InfoModeCurExID, 0);"))),
      ("states", "buttonBits", "Low byte of the last inverted button state", "med", cites(("main.c", "port->states = btnStates;"))),
      ("rStickX", "rightStickX", "Right stick X", "high", cites(("main.c", "port->rStickX = padBuf[6];"))),
      ("rStickY", "rightStickY", "Right stick Y", "high", cites(("main.c", "port->rStickY = padBuf[7];"))),
      ("lStickX", "leftStickX", "Left stick X", "high", cites(("main.c", "port->lStickX = padBuf[4];"))),
      ("lStickY", "leftStickY", "Left stick Y", "high", cites(("main.c", "port->lStickY = padBuf[5];"))),
      ("lock", "modeLock", "Mode lock passed to PadSetMainMode", "high", cites(("main.c", "port->lock = 2;"))),
      ("actData", "actuatorLevels", "Motor levels sent with PadSetAct", "high", cites(("main.c", "PadSetAct(portID, port->actData, sizeof port->actData);"))),
      ("connected", "connected", "Port finished connection setup", "high", cites(("main.c", "port->connected = 1;"))),
  ])

SAVE_FIELDS = [
    ("fileInfo", "cardHeader", "Card file header with title and icon palette"),
    ("unk80", "iconBitmap", "Card icon bitmap"),
    ("unk100", "reserved100", "Zero block included in checksum block 1"),
    ("unk180", "protectedHeader", "Cipher key slot header summary and block checksums"),
    ("stateFlags", "stateFlags", "Game state flags"),
    ("actionsLearned", "learnedActions", "Learned action flags"),
    ("mapStatus", "mapStatus", "Visited room and area flags"),
    ("settings", "settings", "Player settings"),
    ("unk6C8", "ashleySnapshot", "Ashley's statistics snapshot"),
    ("inventory", "inventory", "Inventory"),
    ("unk16C8", "inventoryIndices", "Inventory ordering indices"),
    ("unk1778", "entryDoor", "Door the player entered the current room through"),
    ("scoreData", "scoreData", "Score and record statistics"),
    ("unk1898", "roomStateSlot", "Current room state slot"),
    ("unk189C", "recentRoomEnemyStates", "Recent room enemy states"),
    ("artsStatus", "artsStatus", "Break art progress"),
    ("containerData", "containerData", "Container contents"),
    ("unk59E0", "reserved59E0", "Zero padding to the end of the 23 blocks"),
]
save_cites = {
    "fileInfo": cites(("260.c", "((char*)&savedata->fileInfo.key)[0] = 0x53;")),
    "unk80": cites(("260.c", "_rMemcpy(savedata3->unk80, _mcData->unk4E0[targetFile], sizeof savedata->unk80);")),
    "unk100": cites(("260.c", "p = data->unk100;")),
    "unk180": cites(("260.c", "savedata_unk180_t* s5 = &savedata->unk180;")),
    "stateFlags": cites(("260.c", "_rMemcpy(savedata->stateFlags, &vs_main_stateFlags, sizeof savedata->stateFlags);")),
    "actionsLearned": cites(("260.c", "_rMemcpy(savedata->actionsLearned, vs_main_actionsLearned,")),
    "mapStatus": cites(("260.c", "_rMemcpy(savedata->mapStatus, &vs_main_mapStatus, sizeof savedata->mapStatus);")),
    "settings": cites(("260.c", "_rMemcpy(&savedata->settings, &vs_main_settings, sizeof savedata->settings);")),
    "unk6C8": cites(("260.c", "_rMemcpy(&savedata->unk6C8, &D_80060068, sizeof savedata->unk6C8);")),
    "inventory": cites(("260.c", "_rMemcpy(&savedata->inventory, &vs_main_inventory, sizeof savedata->inventory);")),
    "unk16C8": cites(("260.c", "_rMemcpy(&savedata->unk16C8, &vs_main_inventoryIndices, sizeof savedata->unk16C8);")),
    "unk1778": cites(("260.c", "_rMemcpy(&savedata->unk1778, &D_80061068, sizeof savedata->unk1778);")),
    "scoreData": cites(("260.c", "_rMemcpy(&savedata->scoreData, &vs_main_scoredata, sizeof savedata->scoreData);")),
    "unk1898": cites(("260.c", "savedata->unk1898 = D_80060064;")),
    "unk189C": cites(("260.c", "_rMemcpy(savedata->unk189C, D_80061078, sizeof savedata->unk189C);")),
    "artsStatus": cites(("260.c", "_rMemcpy(&savedata->artsStatus, &vs_main_artsStatus, sizeof savedata->artsStatus);")),
    "containerData": cites(("260.c", "_rMemcpy(&savedata->containerData, &savedata2->containerData,")),
    "unk59E0": cites(("260.c", "memset(savedata, 0, sizeof *savedata);")) + ";no member access;source:260.c:107",
}
SAVE_PURPOSE = "Lays out the 23-block save file: card header, protected header and game state."
T("T00598", "OnDisc", "Save::GameSaveImage", S + "GameFiles.h", SAVE_PURPOSE, "high",
  cites(("260.c", "static void _packageGameSaveData(int targetFile)")), [
      (o, n, m, "low" if n.startswith("reserved") else "high", save_cites[o]) for o, n, m in SAVE_FIELDS
  ])
title_orig = [m["name"] for m in members["T00599"]]
T("T00599", "OnDisc", "Save::GameSaveImage", S + "GameFiles.h", SAVE_PURPOSE, "high",
  "same-as:T00598;identical offsets in the Title overlay copy;source:22C.c:282", [
      (o, n, m, "low" if n.startswith("reserved") else "med", f"{DECL}source:22C.c:{264 + i};same layout as T00598")
      for i, (o, (_, n, m)) in enumerate(zip(title_orig, SAVE_FIELDS))
  ])

T("T00600", "OnDisc", "Save::SaveCipherHeader", S + "GameFiles.h",
  "Stores the key that encodes the protected save region and the slot header that starts it.", "high",
  cites(("260.c", "_decode(unk180->key, &unk180->base.slotState,")), [
      ("key", "cipherKey", "Key for the save byte stream cipher", "high", cites(("260.c", "s5->unk180.key = vs_main_settings.key;"))),
      ("base", "slotHeader", "Slot state generation and format version", "high", cites(("260.c", "s5->unk180.base.slotState = vs_main_settings.slotState;"))),
  ])

T("T00601", "OnDisc", "Save::SaveProtectedHeader", S + "GameFiles.h",
  "Holds the cipher header, summary statistics and per-block checksums of a save.", "high",
  cites(("260.c", "savedata_unk180_t* s5 = &savedata->unk180;")), [
      ("unk180", "cipherHeader", "Cipher key and slot header", "high", cites(("260.c", "s5->unk180.key = vs_main_settings.key;"))),
      ("stats", "summary", "Summary statistics shown in the file menu", "high", cites(("260.c", "s5->stats.gameTime.t = vs_main_gametime.t;"))),
      ("checksums", "blockChecksums", "XOR checksum of each 256-byte block", "high", cites(("260.c", "s5->checksums[i] = var_a0;"))),
  ])

T("T00659", "State", "Content::ActorAssetRequest", C + "ActorAssets.h",
  "Requests loading of one actor asset: model, weapon, sequence or effect data.", "high",
  cites(("30DB0.c", "D_800F4538[object->index] = object->unk4;")), [
      ("dataType", "assetKind", "Kind of asset being requested", "high", cites(("146C.c", "objData.dataType = 6;"))),
      ("index", "targetSlot", "Actor or weapon slot receiving the asset", "high", cites(("30DB0.c", "D_800F4538[object->index] = object->unk4;"))),
      ("modelId", "modelId", "Model identifier", "high", cites(("30DB0.c", "modelId = object->modelId;"))),
      ("unk4", "destination", "Storage that receives the asset", "high",
       cites(("30DB0.c", "D_800F4538[object->index] = object->unk4;"), ("30DB0.c", "vs_battle_wepModels[objectData->index] = objectData->unk4;"))),
      ("dataAddr", "dataAddress", "Address of the asset data or a small table index", "high", cites(("146C.c", "object.dataAddr = zud->model + (u_long)zud;"))),
      ("unkC", "placement", "Placement word copied into the new actor", "high",
       cites(("30DB0.c", "*(vs_battle_objectData_flags*)&actor->unk0.currentTileX = object->unkC;"))),
      ("actorId", "subKind", "Per-kind selector such as the sequence kind; 0xFF for none", "med",
       cites(("30DB0.c", "} else if (object->actorId == 3) {"), ("30DB0.c", "if (object->actorId == 0xFF) {"))),
      ("unk11", "textureSlot", "Palette or texture slot used by the asset", "med",
       cites(("30DB0.c", "model->clutSlotOffset = objectData->unk11 + (objectData->actorId << 1) + 16;"),
             ("30DB0.c", "setRECT(&rect, (objdata->unk11 & 0xF) << 6, 256, 64, h);"))),
      ("material", "material", "Weapon or armor material", "high", cites(("146C.c", "object.material = actor->equippedWeaponCategory;"))),
      ("variant", "variant", "Asset variant", "high", cites(("146C.c", "objData.variant = arg2->unk0_8 >> 4;"))),
  ])

T("T00667", "Packet", "Render::LegacyTransitionWipePacket", G + "ScreenTransition.h",
  "Holds the PS1 quads covering the wiped screen area and drawing the wipe edge.", "high",
  cites(("146C.c", "vs_battle_screenTransitionWipePolyF4[vs_main_frameBuf].covered.x0 =")) + ";replaced-by:native transition pass", [
      ("covered", "coveredQuad", "Quad over the covered part of the screen", "high",
       cites(("146C.c", "vs_battle_screenTransitionWipePolyF4[vs_main_frameBuf].covered.x0 ="))),
      ("edge", "edgeQuad", "Quad drawing the wipe edge", "high",
       cites(("146C.c", "vs_battle_screenTransitionWipePolyF4[vs_main_frameBuf].edge.x0 ="))),
  ])

T("T00670", "State", "Render::TextureSlot", G + "Textures.h",
  "Tracks one actor or weapon texture slot's page, palettes and user count.", "high",
  cites(("30DB0.c", "++D_800F46A8[actor->unk5BB + i].refCount;")), [
      ("tpage", "texturePage", "Texture page word", "high", cites(("30DB0.c", "D_800F46A8[actor->unk5BB].tpage |= 0x80;"))),
      ("clut0", "palette", "Default palette", "high", cites(("30DB0.c", "actor->unk6E8 = D_800F46A8[object->index].clut0;"))),
      ("clut1", "alternatePalette", "Alternate palette", "high", cites(("30DB0.c", "actor->unk6E8 = D_800F46A8[1].clut1;"))),
      ("refCount", "referenceCount", "Number of users; zero frees the slot", "high", cites(("30DB0.c", "if (D_800F46A8[i].refCount == 0) {"))),
  ])

T("T00692", "State", "Core::HeapBlockHeader", "Native\\Core\\src\\Memory\\Heap.h",
  "Links one free or allocated heap block and records its size.", "high",
  cites(("main.c", "if (var_a1->blockSz >= blockSz) {")) + ";replaced-by:native allocator", [
      ("prev", "prev", "Previous block", "high", cites(("main.c", "var_a1->next->prev = var_a1->prev;"))),
      ("next", "next", "Next block", "high", cites(("main.c", "var_v1->next = var_a1->next;"))),
      ("blockSz", "blockSize", "Block size", "high", cites(("main.c", "if (var_a1->blockSz == blockSz) {"))),
      ("unkC", "reservedC", "No access found", "low", DECL + "source:main.h:141"),
  ])

T("T00694", "State", "Content::DiscDrive", C + "DiscQueue.h",
  "Holds the disc drive read state: target, progress counters, ring buffer indices and XA settings.", "high",
  cites(("main.c", "static int _diskInitRead(int sector, u_int bytes, void* vram)")), [
      ("state", "state", "Drive state", "high", cites(("main.c", "vs_main_disk.state = diskReadReady;"))),
      ("unk1", "xaModeMarker", "Set to 0 when XA streaming starts and 128 when it stops; never read", "low",
       cites(("main.c", "vs_main_disk.unk1 = 0;"), ("main.c", "vs_main_disk.unk1 = 128;"))),
      ("unk2", "reserved2", "Cleared on reset; never read", "low", cites(("main.c", "vs_main_disk.unk2 = 0;"))),
      ("unk3", "failureCount", "Failed command count", "high", cites(("main.c", "vs_main_disk.unk3 = 0;"), ("main.c", "++vs_main_disk.unk3;"))),
      ("unk4", "sectorsReceived", "Sectors delivered by the drive; ends the read at sectorCount", "high",
       cites(("main.c", "if (++vs_main_disk.unk4 >= vs_main_disk.sectorCount) {"))),
      ("sectorCount", "sectorCount", "Sectors to read", "high", cites(("main.c", "vs_main_disk.sectorCount = bytes / 2048;"))),
      ("cdSector", "startSector", "First sector of the read", "high", cites(("main.c", "vs_main_disk.cdSector = sector;"))),
      ("byteCount", "byteCount", "Bytes to read", "high", cites(("main.c", "vs_main_disk.byteCount = bytes;"))),
      ("vram", "destination", "Destination buffer", "high", cites(("main.c", "vs_main_disk.vram = vram;"))),
      ("pcm", "xaFilter", "XA file and channel filter", "high", cites(("main.c", "vs_main_disk.pcm.file = 1;"))),
      ("cdLoc", "position", "Drive position", "high", cites(("main.c", "DsIntToPos(sector, &vs_main_disk.cdLoc);"))),
      ("commandId", "commandId", "Id of the last issued command", "high", cites(("main.c", "if (vs_main_disk.commandId == 0) {"))),
      ("unk24", "reserved24", "No access found", "low", DECL + "source:main.c:108"),
      ("unk28", "reserved28", "No access found", "low", DECL + "source:main.c:109"),
      ("bufferMode", "bufferMode", "Direct or ring-buffered transfer", "high", cites(("main.c", "switch (vs_main_disk.bufferMode) {"))),
      ("sectorBufIndex", "destinationSector", "Next destination sector to fill", "high",
       cites(("main.c", "DsGetSector(vs_main_disk.vram + vs_main_disk.sectorBufIndex * 2048, 512);"))),
      ("unk34", "bufferedSectors", "Sectors in the ring not yet copied out", "high",
       cites(("main.c", "++vs_main_disk.unk34;"), ("main.c", "--vs_main_disk.unk34;"))),
      ("unk38", "ringSectorsWritten", "Sectors written to the ring; bounds the copy loop", "high",
       cites(("main.c", "if (vs_main_disk.sectorBufIndex >= vs_main_disk.unk38) {"))),
      ("unk3C", "ringWriteIndex", "Ring entry receiving the next sector", "high",
       cites(("main.c", "DsGetSector(_cdReadBuffer + vs_main_disk.unk3C * 128, 512);"))),
      ("ringBufIndex", "ringReadIndex", "Ring entry copied out next", "high",
       cites(("main.c", "_cdReadBuffer + (vs_main_disk.ringBufIndex * 128), 2048);"))),
      ("framesSinceLastRead", "framesSinceLastRead", "Frames since the queue last started a read", "high",
       cites(("main.c", "++vs_main_disk.framesSinceLastRead;"))),
  ])

problems = []
byid = {m["id"]: m for ms in members.values() for m in ms}
for fid, fname, meaning, conf, notes in fields:
    orig = byid[fid]["name"]
    ok = False
    for m in re.finditer(r"source:([^:;]+):(\d+)", notes):
        text = (SRC / FILES[m.group(1)]).read_text(encoding="utf-8", errors="replace").splitlines()[int(m.group(2)) - 1]
        if re.search(r"(?<!\w)" + re.escape(orig) + r"(?!\w)", text):
            ok = True
    if not ok:
        problems.append(f"{fid} {orig}: no cited line mentions the member")
if problems:
    sys.exit("\n".join(problems))

with open(OUT / "types.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["id", "role", "final_name", "final_location", "purpose", "confidence", "notes"])
    w.writerows(types)
with open(OUT / "fields.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["id", "final_name", "meaning", "confidence", "notes"])
    w.writerows(fields)
print(len(types), "types", len(fields), "fields")
