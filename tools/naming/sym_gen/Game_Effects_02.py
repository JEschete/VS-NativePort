"""Game_Effects_02 globals and types from code read directly. Citations are resolved by exact text search (common.Gen).

Plugin globals: each plugin source binds a template macro to its own global (`#define VS_RING_COLORS D_...`) and then
includes the template header, whose use of the macro fixes the global's meaning; a few plugins index a ring color
table inline (`factorBytes = ((u_char*)D_...) + (column & 0x1C);`, the rings.h pattern). The generator finds each
global's binding in its own overlay directory (plugin overlays share addresses, so a name can recur in several) and
names it Effects::PlgNNN::<template member>, numbering repeats within one plugin. A template extern row and the
plugin's own definition of the same object (same overlay) are one object: the extern row is a view of the definition.
"""
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import ROOT, Gen  # noqa: E402

g = Gen("Game_Effects_02")
c, G = g.c, g.G
L, M, H = "low", "med", "high"
E = "Native\\Game\\src\\Effects\\"
GAP = ";inventory size is an address gap, not a capacity"
SRC = ROOT / "resources" / "rood-reverseWin" / "src"
with (ROOT / "tools" / "naming" / "sym_batches" / "Game_Effects_02" / "globals.csv").open(encoding="utf-8") as stream:
    ROWS = list(csv.DictReader(stream))

# macro: (member, disposition, purpose, template header, template line)
MACROS = {
    "RING_COLORS": ("ringColorFactors", "catalog", "Gives the ring template's color factors for each transparency mode.",
                    "EFFECT/rings.h", "factorBytes = (u_char*)VS_RING_COLORS + (column & 0x1C);"),
    "TILED_RING_COLORS": ("tiledRingColorFactors", "catalog", "Gives the tiled ring template's color factors for each transparency mode.",
                          "EFFECT/tiledRings.h", "factorBytes = &VS_TILED_RING_COLORS[column];"),
    "CLOSED_RING_COLORS": ("closedRingColorFactors", "catalog", "Gives the closed ring template's color factors for each transparency mode.",
                           "EFFECT/closedRings.h", "factorBytes = &VS_CLOSED_RING_COLORS[column];"),
    "SURFACE_VERTICES": ("surfaceVertices", "catalog", "Gives the surface template's 5 by 16 unit vertices, scaled by radius and height.",
                         "EFFECT/surfaces.h", "+ ((VS_SURFACE_VERTICES[row][column].vx * radiusValue) >> 12);"),
    "MIRROR_SURFACE_VERTICES": ("mirrorSurfaceVertices", "catalog",
                                "Gives the mirrored surface template's 5 by 16 unit vertices; the lower half is mirrored in height.",
                                "EFFECT/mirroredSurfaces.h", "+ ((VS_MIRROR_SURFACE_VERTICES[4 - row][column].vx * radiusValue)"),
    "MESH10_VERTICES": ("mesh10Vertices", "catalog", "Gives the mesh template's unit vertices (pad selects the vertex color).",
                        "EFFECT/meshes10.h", "(VS_MESH10_VERTICES[index].vx * scratch->unkA8.vx) >> 12;"),
    "MESH10_FACES": ("mesh10Faces", "catalog", "Gives the mesh template's triangles: three vertex indices and three texture offsets.",
                     "EFFECT/meshes10.h", "*(int*)&prim->poly.x0 = screen[VS_MESH10_FACES[index][0]];"),
    "PARTICLE_CORNER_STATE": ("particleEmissionCounter", "state",
                              "Steps the box corner (0-7) or ring position the particle template emits from next.",
                              "EFFECT/particleSetup.h", "VS_PARTICLE_CORNER_STATE = temp_a1 & 7;"),
    "TRAIL_CORNER_STATE": ("trailEmissionCounter", "state",
                           "Steps the box corner (0-7) or ring position the trail template emits from next.",
                           None, "VS_TRAIL_CORNER_STATE = temp_a1 & 7;"),
}
INLINE_RING = "factorBytes = ((u_char*){name}) + (column & 0x1C);"


def plugin_bindings(overlay: str, name: str):
    """(file, line, macro, include) for each binding of `name` in the overlay's sources, in source order."""
    out = []
    for p in sorted((SRC / overlay).glob("*.c")):
        rel = p.relative_to(SRC).as_posix()
        lines = p.read_text(encoding="utf-8", errors="replace").splitlines()
        for i, ln in enumerate(lines):
            m = re.match(r"\s*#define VS_(\w+) " + re.escape(name) + r"\s*$", ln)
            if m:
                inc = next((re.search(r'"src/(EFFECT/\w+\.h)"', x).group(1) for x in lines[i + 1:]
                            if x.startswith("#include") and "src/EFFECT/" in x), None)
                out.append((rel, ln.strip(), m.group(1), inc))
            elif INLINE_RING.format(name=name) in ln:
                out.append((rel, ln.strip(), "RING_COLORS", None))
    return out


PLUGIN_ROWS = [r for r in ROWS if r["overlay"].startswith("EFFECT/PLG")]
canonical = {}
for r in PLUGIN_ROWS:
    key = (r["overlay"], r["name"])
    if r["segment"] == "c-defined":
        canonical[key] = r["id"]
counts: dict[tuple, int] = {}
unresolved = []
for r in PLUGIN_ROWS:
    key = (r["overlay"], r["name"])
    plg = re.search(r"PLG(\d+)", r["overlay"]).group(1)
    binds = plugin_bindings(r["overlay"], r["name"])
    if not binds:
        unresolved.append(r["id"])
        continue
    rel, line, macro, inc = binds[0]
    member, disp, purpose, header, hline = MACROS[macro]
    if macro == "TRAIL_CORNER_STATE":
        header = inc
    if key in canonical and canonical[key] != r["id"]:
        G(r["id"], "drop", f"Effects::Plg{plg}::{member}Extern", f"{E}Plugins\\Plg{plg}.c",
          f"Is the template's extern declaration of {canonical[key]}.", H,
          f"view-of:{canonical[key]};replaced-by:{canonical[key]};" + c((rel, line)))
        continue
    n = counts[(plg, member)] = counts.get((plg, member), 0) + 1
    final = f"Effects::Plg{plg}::{member}" + (str(n) if n > 1 else "")
    cites = [(rel, line)] + ([(header, hline)] if header else [])
    note = ";bound by the plugin source" if line.startswith("#define") else ";indexed inline like the rings.h template"
    G(r["id"], disp, final, f"{E}Plugins\\Plg{plg}.c", purpose, H if header or not line.startswith("#define") else M,
      c(*cites) + note)

# ---------------------------------------------------------------- effect runtime (BATTLE)
RT, CAM, PT, LD, SCR = E + "Runtime.c", E + "Camera.c", E + "Particles.c", E + "Loading.c", E + "ScreenEffects.c"
TRL, WX = E + "Trails.c", "Native\\Game\\src\\Field\\Weather.c"
G("G00159", "state", "Effects::screenEffectOverlay", SCR, "Points to the screen effect overlay state, or the defaults while none is loaded.", H,
  c(("5BF94.c", "D_800EB9B4 = (D_800EB9B4_t*)D_800F4ED8;"), ("5BF94.c", "D_800EB9B4_t* p = D_800EB9B4;")) + ";points to Effects::ScreenEffectOverlay")
G("G00512", "state", "Effects::screenEffectDefaults", SCR, "Holds the screen effect state used when no overlay is loaded (warp 0x1000).", M,
  c(("5BF94.c", "D_800F4ED8[2] = 0x1000;"), ("5BF94.c", "D_800EB9B4 = (D_800EB9B4_t*)D_800F4ED8;")))
G("G00219", "state", "Effects::effectBufferIndex", RT, "Alternates each frame between the two effect render buffers.", M,
  c(("5BF94.c", "D_800EC2E4 = 1 - D_800EC2E4;")))
G("G00220", "catalog", "Effects::trailAdditiveColors", TRL, "Gives each bone trail color for additive trails (scaled by eighths).", M,
  c(("5BF94.c", "p->r0 = p->r1 = (D_800EC2E8[t->config.color].r * head) / 8;")) + GAP)
G("G00221", "catalog", "Effects::trailColors", TRL, "Gives each bone trail color, scaled by head and tail fade.", M,
  c(("5BF94.c", "p->r0 = p->r1 = D_800EC308[t->config.color].r * head - 128;")) + GAP)
G("G00222", "catalog", "Effects::scriptRenderers", RT, "Gives the renderer an effect script attaches by index.", M,
  c(("5BF94.c", "func_800CF694(arg0, D_800EC324[temp_s2], temp_s0);")) + GAP)
G("G00223", "state", "Effects::emissionCounter", PT,
  "Steps the box corner (0-7) or ring position the next particle is emitted from.", H,
  c(("5BF94.c", "D_800EC328 = (D_800EC328 + 1) & 7;"), ("5BF94.c", "D_800EC328 %= scratch->unk188;")))
G("G00224", "catalog", "Effects::actionVariantValues", RT, "Gives the script register value for each action variant below 4.", M,
  c(("5BF94.c", "arg0->unk10[3] = D_800EC32C[D_800F522C];")) + GAP)
G("G00225", "catalog", "Effects::pairedParticleColorPresets", PT, "Gives two particles' red, green and blue curves for each target color preset.", M,
  c(("5BF94.c", "dst->rCurve = D_800EC330[arg0->unkD1C.unk18.unk2][i][0];")) + GAP)
G("G00226", "catalog", "Effects::particleColorPresetTable", PT,
  "Gives five particles' definition index and red, green and blue curves for each target color preset.", M,
  c(("5BF94.c", "&D_800F569C->block5Data->unk4[D_800EC368[arg0->unkD1C.unk18.unk2][i][3]];")) + GAP)
G("G00227", "catalog", "Effects::scriptOpcodeHandlers", RT, "Dispatches effect script opcodes.", H,
  c(("5BF94.c", "miscIndex = D_800EC3F4[func_800D5170(arg0)](arg0);")) + GAP)
G("G00228", "state", "Effects::lastReactionSoundVariant", RT, "Keeps the last random reaction sound so the next one differs.", H,
  c(("5BF94.c", "} while (var == D_800EC4B8);")))
G("G00229", "state", "Effects::emptyPackage", RT, "Is the empty effect package bound when effect resources are initialised.", L,
  c(("6F014.c", "func_800D8060(&D_800EC4BC);")))
G("G00239", "state", "Effects::tintedActor", RT, "Remembers the actor tinted red during an action, to restore its color.", H,
  c(("146C.c", "D_800F185C = arg0;"), ("146C.c", "func_8007B29C(8, 4, D_800F185C, 0, 0, 0);")))
G("G00339", "state", "Field::weatherParameters", WX, "Holds the weather parameters: the drift vector in entries 0-2 and room values from 4 up.", M,
  c(("146C.c", "particle->position.vx += D_800F1D98[0];"), ("2842C.c", "D_800F1D98[4] = room->unk42;")))
G("G00349", "state", "Field::weatherParticleCount", WX, "Counts the weather particles drawn, set from the weather intensity.", H,
  c(("2842C.c", "D_800F1DC0 = vs_battle_clamp(D_800F1DC4 * 8, 4, D_800F1DC2);"), ("146C.c", "for (i = 0; i < D_800F1DC0; ++i, ++particle) {")))
G("G00350", "state", "Field::weatherParticleCapacity", WX, "Gives the most weather particles allocated.", H,
  c(("2842C.c", "D_800F1DC0 = vs_battle_clamp(D_800F1DC4 * 8, 4, D_800F1DC2);")))
G("G00353", "state", "Field::screenWeatherFade", WX, "Fades screen-space weather in and out by 8 per frame up to 96.", M,
  c(("146C.c", "D_800F1DC7 -= 8;"), ("146C.c", "fade = D_800F1DC7 - 32;")))
G("G00356", "state", "Field::roomWeatherKind", WX, "Gives the current room's weather kind; 0 for none.", H,
  c(("2842C.c", "switch (D_800F1DCA) {"), ("2842C.c", "D_800F1DCA = arg0;")))
G("G00360", "state", "Field::weatherParticles", WX, "Points to the 0x800-byte weather particle buffer.", H,
  c(("146C.c", "D_800F1DD4 = vs_main_allocHeap(0x800);")) + ";points to Field::WeatherParticle")
G("G00524", "state", "Effects::menuReloadSlot", RT, "Holds the read that reloads the main menu after a primary effect.", M,
  c(("5BF94.c", "D_800F5218 = vs_main_allocateCdQueueSlot(&sp38);"), ("5BF94.c", "if (D_800F5218->state != 4) {")))
G("G00525", "state", "Effects::pendingTracks", RT, "Holds one bit per primary effect track not yet started.", H,
  c(("5BF94.c", "D_800F521C |= 1 << i;"), ("5BF94.c", "D_800F521C &= ~(1 << arg0);")))
G("G00526", "state", "Effects::trackCount", RT, "Gives the number of primary effect tracks in the package.", H,
  c(("5BF94.c", "D_800F5224 = D_800F569C->block9Data->unk14;")))
G("G00527", "state", "Effects::startedTrackCount", RT, "Counts the primary effect tracks started; the effect ends once all have.", H,
  c(("5BF94.c", "++D_800F5228;"), ("5BF94.c", "if (D_800F5224 <= D_800F5228) {")))
G("G00528", "state", "Effects::actionVariant", RT, "Gives the action variant of the current effect; 0 for none.", M,
  c(("5BF94.c", "D_800F522C = arg1;"), ("5BF94.c", "arg0->unk10[3] = D_800EC32C[D_800F522C];")))
G("G00529", "state", "Effects::actionRequest", RT, "Holds the action effect request being presented.", H,
  c(("5BF94.c", "if ((D_800F5230.effectId != 0) && (vs_battle_pFileLoadContext.lba == 0)) {")) + ";type Effects::ActionEffectRequest")
G("G00532", "state", "Effects::effectOrigin", RT, "Gives the effect origin added to spawn positions for emission origin 1.", M,
  c(("5BF94.c", "vs_battle_addVecToSvec(&scratch->unk98, D_800F5310, &scratch->unk98);")))
G("G00533", "state", "Effects::primaryToken", RT, "Holds the token of the current primary effect.", H,
  c(("5BF94.c", "D_800F5318 = func_800CF49C();"), ("5BF94.c", "func_800CF514(D_800F5318);")))
G("G00534", "state", "Effects::primaryPhase", RT,
  "Steps the primary effect: 6 load the purge overlay, 2/8/3 load package, 4/5 run tracks, 9 reload the menu, 0 idle.", H,
  c(("5BF94.c", "switch (D_800F531C) {"), ("5BF94.c", "void func_800CF478(int arg0) { D_800F531C = arg0; }")))
G("G00535", "state", "Effects::tokenBitmaps", RT, "Holds four 32-bit maps of allocated effect tokens.", H,
  c(("5BF94.c", "D_800F5320[i] = temp_a1 | v;"), ("5BF94.c", "ptr[var_v1] &= ~(1 << (arg0 - (var_v1 << 5)));")))
G("G00536", "state", "Effects::curveSamples", RT, "Holds the current sample of each package curve; entry 0 is zero.", H,
  c(("5BF94.c", "D_800F5330[0] = 0;"), ("5BF94.c", "vs_battle_lerpVector(arg0->unk20, D_800F5330[arg0->unk4], &arg2->unk10);")))
G("G00537", "state", "Effects::unusedPrimaryWord", RT, "Cleared with the primary effect state; not otherwise used.", L,
  c(("5BF94.c", "D_800F53B4 = 0;")))
G("G00538", "state", "Effects::primaryInstances", RT, "Points to the first primary effect instance, in priority order.", H,
  c(("5BF94.c", "D_800F53B8 = newNode;")) + ";points to Effects::EffectInstance")
G("G00539", "state", "Effects::currentInstance", RT, "Points to the effect instance being updated.", H,
  c(("5BF94.c", "D_800F53BC = var_s0;"), ("EFFECT/closedRings.h", "D_800F53B8_t* battle = D_800F53BC;")))
G("G00540", "state", "Effects::secondaryRequest", RT, "Holds the request of the secondary effects being spawned.", H,
  c(("5BF94.c", "D_800F53C0 = *arg0;")))
G("G00541", "state", "Effects::primaryLoaded", RT, "Set while a primary effect package is loaded and running.", H,
  c(("5BF94.c", "D_800F54A8 = 1;"), ("5BF94.c", "int func_800CEF64(void) { return D_800F54A8 != 0; }")))
G("G00542", "state", "Effects::actionVariantArgument", RT, "Keeps the second action variant argument; not read by decompiled code.", L,
  c(("5BF94.c", "D_800F54A9 = arg2;")))
G("G00543", "state", "Effects::secondaryInstances", RT, "Points to the first secondary effect instance, in priority order.", H,
  c(("5BF94.c", "D_800F54B0 = temp_v0;")) + ";points to Effects::EffectInstance")
G("G00544", "state", "Effects::cameraShakeChannels", CAM, "Holds the camera shake channels.", H,
  c(("5BF94.c", "D_800F54B8_t* p = &D_800F54B8[i];")) + ";element type Effects::CameraShakeChannel")
G("G00545", "state", "Effects::cameraMotionFrame", CAM, "Counts frames of the effect camera motion.", M,
  c(("5BF94.c", "func_800D1884(&start, &end, D_800F55A0, D_800F54D0, &result);"), ("5BF94.c", "D_800F54D0 = 0;")))
G("G00549", "state", "Effects::cameraMotionDuration", CAM, "Gives the length of the effect camera motion.", M,
  c(("5BF94.c", "D_800F55A0 = arg2;"), ("5BF94.c", "func_800D1884(&start, &end, D_800F55A0, D_800F54D0, &result);")))
G("G00546", "state", "Effects::effectCamera", CAM, "Holds the camera the effects drive.", H,
  c(("5BF94.c", "func_800D1B18(&D_800F54D8);"), ("5BF94.c", "D_800F54D8.unk20 = v;")) + ";type Effects::EffectCamera")
G("G00547", "state", "Effects::enabledShakeChannels", CAM, "Holds one bit per camera shake channel that is enabled.", H,
  c(("5BF94.c", "void func_800D1DE4(int arg0) { D_800F5518 |= arg0; }"), ("5BF94.c", "if (!((D_800F5518 >> i) & 1)) {")))
G("G00548", "state", "Effects::cameraMotionEnds", CAM, "Holds the start and end poses of the effect camera motion.", H,
  c(("5BF94.c", "setVector(&start, D_800F5520[0].position.vx >> 12, D_800F5520[0].position.vy >> 12,")))
G("G00550", "state", "Effects::savedActionCamera", CAM, "Keeps the camera captured when an action effect begins, to restore it.", H,
  c(("5BF94.c", "func_800D1B18(&D_800F55A8);"), ("5BF94.c", "D_800F5520[1] = D_800F55A8;")))
G("G00551", "state", "Effects::cameraTransitionMode", CAM, "Selects how the effect camera moves (linear, cosine, hold...).", M,
  c(("5BF94.c", "void func_800D1E20(int arg0) { D_800F55E8 = arg0; }"), ("5BF94.c", "switch (D_800F55E8) {")))
G("G00552", "state", "Effects::particleCapacity", PT, "Gives the number of particles in the pool.", H,
  c(("5BF94.c", "D_800F55F0 = count;"), ("5BF94.c", "count = D_800F55F0 - D_800F55F8;")))
G("G00553", "state", "Effects::freeParticles", PT, "Points to the first free particle.", H,
  c(("5BF94.c", "D_800F55F4 = node->next;")) + ";points to Effects::Particle")
G("G00554", "state", "Effects::usedParticleCount", PT, "Counts the particles in use.", H,
  c(("5BF94.c", "++D_800F55F8;"), ("5BF94.c", "--D_800F55F8;")))
G("G00555", "state", "Effects::particlePool", PT, "Points to the particle pool.", H,
  c(("5BF94.c", "D_800F55FC = vs_main_allocHeapR(sizeof *D_800F55FC * count);")))
G("G00556", "state", "Effects::particlePoolReferences", PT, "Counts the users of the particle pool; the pool is freed at zero.", H,
  c(("5BF94.c", "++D_800F5600;"), ("5BF94.c", "--D_800F5600;")))
G("G00557", "state", "Effects::scriptSelector", RT, "Keeps the selector byte read by the effect script; not otherwise read.", L,
  c(("5BF94.c", "D_800F5610 = func_800D5170(arg0);")))
G("G00558", "state", "Effects::particleDrag", PT, "Gives the drag applied to particles.", H,
  c(("5BF94.c", "void func_800D5294(int* arg0) { D_800F5618 = *arg0; }"), ("5BF94.c", "int drag = D_800F5618;")))
G("G00559", "state", "Effects::particleGravity", PT, "Gives the gravity applied to particles.", H,
  c(("5BF94.c", "void func_800D5260(D_800F5620_t* arg0) { D_800F5620 = *arg0; }")) + ";type Effects::ParticleGravity")
G("G00560", "state", "Effects::textureLoadIndex", LD, "Indexes the effect texture being loaded.", H,
  c(("6F014.c", "++D_800F5630;"), ("6F014.c", "int slot = D_800F5638.slots[D_800F5630];")))
G("G00561", "state", "Effects::textureLoads", LD, "Lists the effect textures to load: count, files, slots and CLUT flags.", H,
  c(("6F014.c", "if ((D_800F5680 == 0) && (D_800F5638.unk0 != 0)) {")) + ";type Effects::EffectLoadState")
G("G00562", "state", "Effects::packageBuffer", LD, "Points to the loaded effect package file.", H,
  c(("6F014.c", "vs_battle_pfileBuf = vs_main_allocHeapR(vs_battle_pFileLoadContext.size);")) + ";fork name pfileBuf")
G("G00563", "state", "Effects::textureLoadState", LD, "Holds the effect texture loading state; loading starts while zero.", M,
  c(("6F014.c", "void func_800D8038(int arg0) { D_800F5680 = arg0; }"), ("6F014.c", "if ((D_800F5680 == 0) && (D_800F5638.unk0 != 0)) {")))
G("G00564", "state", "Effects::purgeOverlayLoaded", LD, "Set while the effect purge overlay is loaded.", M,
  c(("6F014.c", "D_800F5684 = 1;"), ("5BF94.c", "if (D_800F5684 != 0) {")))
G("G00565", "state", "Effects::packageFile", LD, "Gives the effect package file to load.", H,
  c(("6F014.c", "return _loadfile(vs_battle_pFileLoadContext.lba + VS_E000_P_LBA,")) + ";fork name pFileLoadContext")
G("G00566", "state", "Effects::resourceReadSlot", LD, "Holds the read of an effect resource.", H,
  c(("6F014.c", "D_800F568C = vs_main_allocateCdQueueSlot(&cdFile);")))
G("G00567", "state", "Effects::pluginLoadState", LD, "Holds the effect plugin loading state.", H,
  c(("6F014.c", "void vs_battle_setPlgLoadState(int arg0) { _plgLoadState = arg0; }")) + ";fork name kept")
G("G00568", "state", "Effects::pluginFile", LD, "Gives the effect plugin file to load.", H,
  c(("6F014.c", "_plgLoadContext.lba = lba;")) + ";fork name kept")
G("G00569", "state", "Effects::savedPackage", RT, "Keeps the selected effect package while another is used.", H,
  c(("6F014.c", "void func_800D78B8(void) { D_800F5698 = D_800F569C; }")))
G("G00570", "state", "Effects::package", RT, "Points to the selected effect package.", H,
  c(("6F014.c", "D_800F569C = &D_800F56C8;")) + ";points to Effects::EffectPackage")
G("G00571", "state", "Effects::textureLoadBuffer", LD, "Points to the buffer effect textures are read into.", H,
  c(("6F014.c", "D_800F56A0 = vs_main_allocHeapR(VS_E000_0_FBT_SIZE);")))
G("G00572", "state", "Effects::pluginEntryPointCount", LD, "Gives the number of renderer entry points of the loaded plugin.", M,
  c(("6F014.c", "_plgEntryPointCount = entryPointCount;")) + ";fork name kept")
G("G00574", "state", "Effects::effectRenderers", RT, "Gives the renderer bound to each renderer index by the plugin.", H,
  c(("6F014.c", "vs_battle_effectRenderers[index] = func;")) + ";fork name kept")
G("G00575", "state", "Effects::actionPackage", RT, "Holds the action effect package.", M,
  c(("6F014.c", "D_800F569C = &D_800F56C8;"), ("EFFECT/EFFPURGE.BIN/0.c", "D_800F56C8.curveBlock = NULL;")))
G("G00576", "state", "Effects::actionRequestPointer", RT, "Points to the action request; not read by decompiled code.", L,
  c(("6F014.c", "D_800F5798 = &D_800F5230;")))
G("G00577", "state", "Effects::secondaryPackage", RT, "Holds the secondary effect package, whose request is the secondary request.", M,
  c(("6F014.c", "D_800F57A0.unkD0 = &D_800F53C0;"), ("6F014.c", "D_800F569C = &D_800F57A0;")))
G("G00578", "state", "Effects::loadedTextureSlots", LD, "Holds one bit per texture slot (24-31) holding an effect texture, released with the package.", M,
  c(("6F014.c", "D_800F5874 |= temp_s1 << slot;"), ("6F014.c", "D_800F5874 &= ~bit;")))

# ---------------------------------------------------------------- EFFPURGE tables
PG = E + "PurgeTables.c"
G("G00622", "catalog", "Effects::effectUsesTextures", PG, "Marks the effects that load texture files.", M,
  c(("EFFECT/EFFPURGE.BIN/0.c", "int new_var = D_800FAE4C[D_800F5230.effectId];")))
G("G00623", "catalog", "Effects::effectEntryPointIndices", PG, "Gives each effect's first renderer entry point; the next entry ends it.", H,
  c(("EFFECT/EFFPURGE.BIN/0.c", "int firstindex = _effectEntryPointIndices[D_800F5230.effectId];")) + ";fork name kept")
G("G00624", "catalog", "Effects::effectEntryPoints", PG, "Lists every effect renderer entry point.", H,
  c(("EFFECT/EFFPURGE.BIN/0.c", "vs_battle_setEffectExec(_effectEntryPoints[firstindex + i], i);")) + ";fork name kept")
G("G00625", "catalog", "Effects::fbcFiles", PG, "Gives each effect's CLUT file.", H,
  c(("EFFECT/EFFPURGE.BIN/0.c", "vs_battle_configureEffectFbLoad(_fbcFiles[D_800F5230.effectId].lba,")) + ";fork name kept")
G("G00626", "catalog", "Effects::fbcSlots", PG, "Gives the texture slot of each effect's CLUT file.", H,
  c(("EFFECT/EFFPURGE.BIN/0.c", "a2 = &_fbcSlots[D_800F5230.effectId];")) + ";fork name kept")
G("G00627", "catalog", "Effects::fbtCounts", PG, "Gives the number of texture files of each effect.", H,
  c(("EFFECT/EFFPURGE.BIN/0.c", "texCount = _fbtCounts[D_800F5230.effectId];")) + ";fork name kept")
G("G00628", "catalog", "Effects::fbtFiles", PG, "Lists the texture files of all effects in effect order.", H,
  c(("EFFECT/EFFPURGE.BIN/0.c", "vs_battle_configureEffectFbLoad(_fbtFiles[var_s2 + i].lba,")) + ";fork name kept")
G("G00629", "catalog", "Effects::fbtSlots", PG, "Gives the texture slot of each texture file.", H,
  c(("EFFECT/EFFPURGE.BIN/0.c", "_fbtFiles[var_s2 + i].size, _fbtSlots[var_s2 + i], 0, i + 1);")) + ";fork name kept")
G("G00630", "catalog", "Effects::packageFiles", PG, "Gives each effect's package file.", H,
  c(("EFFECT/EFFPURGE.BIN/0.c", "vs_battle_pFileLoadContext = _pFiles[D_800F5230.effectId];")) + ";fork name pFiles")
G("G00631", "catalog", "Effects::pluginFiles", PG, "Gives each effect's plugin file.", H,
  c(("EFFECT/EFFPURGE.BIN/0.c", "_plgFiles[D_800F5230.effectId].lba, _plgFiles[D_800F5230.effectId].size, count);")) + ";fork name plgFiles")

# ---------------------------------------------------------------- main executable
G("G01957", "state", "Render::nearClip", "Native\\Engine\\src\\Render\\Render.c", "Gives the camera near clip distance.", H,
  c(("146C.c", "vs_main_nearClip = arg0;"), ("5BF94.c", "arg0->nearClip = vs_main_nearClip;")) + ";fork name kept")
G("G02018", "state", "Core::overlaySlots", "Native\\Engine\\src\\Core\\Overlays.c", "Gives the addresses overlays are loaded at, by slot.", H,
  c(("6F014.c", "return _loadfile(VS_EFFPURGE_BIN_LBA, VS_EFFPURGE_BIN_SIZE, vs_overlay_slots[3]);")) + ";replaced by native module loading")

# ---------------------------------------------------------------- template types
PRIM, TPL = "Native\\Engine\\src\\Render\\LegacyPackets.h", E + "Templates\\TemplateState.h"


def TT(tid, role, name, loc, purpose, conf, overrides=None, recv=None):
    names = {m["name"] for m in g.members[tid]}
    assert set(overrides or {}) <= names, (tid, set(overrides) - names)
    g.T(tid, role, name, loc, purpose, conf, "declaration-only;" + g.decl(tid, g.members[tid][0]["name"]),
        g.generic_specs(tid, overrides), recv=recv)


TT("T00729", "Packet", "Render::TiledRingPacket", PRIM, "Is one tiled ring template packet with its draw mode word.", M)
TT("T00731", "Packet", "Render::TiledRingTexCoord", PRIM, "Gives one tiled ring texture coordinate.", M)
TT("T00735", "Packet", "Render::TransposedRingPacket", PRIM, "Is one transposed ring template packet with its draw mode word.", M)
TT("T00737", "Packet", "Render::TransposedRingTexCoord", PRIM, "Gives one transposed ring texture coordinate.", M)
TT("T00730", "State", "Effects::TiledRingState", TPL, "Holds one tiled ring effect template's state.", M, recv=r"\b(?:state|effect|arg0|arg3)")
TT("T00736", "State", "Effects::TransposedRingState", TPL, "Holds one transposed ring effect template's state.", M, recv=r"\b(?:state|effect|arg0|arg3)")
TT("T00732", "State", "Effects::Trail9", TPL, "Holds one nine-sample trail: spline points, samples, progress and speed.", M,
   recv=r"(?:trails\[[^\]]*\]|temp_s0)")
TT("T00733", "Packet", "Render::Trail9Packet", PRIM, "Is one trail template gouraud packet with its draw mode word.", M, {
    "unk0": ("tag0", "Byte of the ordering-table tag, written when the packet is added", L, "declaration-only;" + g.decl("T00733", "unk0")),
    "unk1": ("tag1", "Byte of the ordering-table tag", L, "declaration-only;" + g.decl("T00733", "unk1")),
    "unk2": ("tag2", "Byte of the ordering-table tag", L, "declaration-only;" + g.decl("T00733", "unk2")),
    "unk3": ("wordCount", "Packet length in words", M, c(("EFFECT/trails9.h", "temp_a3->unk3 = 0xE;"))),
    "unk4": ("poly", "Gouraud textured quad", M, c(("EFFECT/trails9.h", "sp10.unk4.x0 = spF0[0].vx + temp_s0;"))),
    "unk38": ("drawMode", "Draw mode command word (0xE1)", M, c(("EFFECT/trails9.h", "temp_a3->unk38 = 0xE1000200;"))),
})
TT("T00734", "State", "Effects::Trail9State", TPL, "Holds the trail template's state: lifetime, definition, trails, age and texture animation.", M, {
    "unk0": ("lifetime", "Lifetime (low byte of the spawn argument), as in the other template states", M,
             c(("EFFECT/trails9.h", "temp_v0->unk0 = arg2;"))),
    "unk1": ("effect", "Particle definition (high byte of the spawn argument)", M,
             c(("EFFECT/trails9.h", "temp_s3 = &D_800F569C->block5Data->unk4[temp_v0->unk1];"))),
    "unkC": ("texture", "Texture animation state", M, c(("EFFECT/trails9.h", "func_800D6D24(&arg3->unkC);"))),
    "trailcount": ("trailCount", "Number of trails", M, c(("EFFECT/trails9.h", "for (i = 0; i < arg3->trailcount; ++i) {"))),
}, recv=r"\b(?:arg3|temp_v0|temp_s0)")

if __name__ == "__main__":
    if unresolved:
        print("unresolved plugin globals:", unresolved)
    g.write()
