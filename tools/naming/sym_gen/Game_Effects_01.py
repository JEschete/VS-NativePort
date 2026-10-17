"""Game_Effects_01 types and fields from code read directly. Citations are resolved by exact text search (common.Gen).

Every type goes through common.Gen.generic_specs (used members keep a meaningful fork name or become opaque<Off>;
unread members are reserved<Off>); overrides name the members whose meaning was read:
- the effect instance, spawn request, timeline and package from InitEffectInstance, SpawnSecondaryEffects and the
  timeline tick functions (5BF94.c);
- the particle definition and the emitter scratch from EmitParticleEvent (5BF94.c:6940-7500), where each definition
  curve byte selects the curve sample for one range and the scratch receives the result;
- the particle node from upstream's motion group (func_800D52A4_t) and the fork's motion and attraction views
  (effectMotionState, effectAttractionView), which read the node by offset.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen  # noqa: E402

g = Gen("Game_Effects_01")
c = g.c
L, M, H = "low", "med", "high"
E = "Native\\Game\\src\\Effects\\"
ANY = r"[\w\]\)]+"


def T(tid, role, name, loc, purpose, conf, notes, overrides=None, recv=ANY):
    overrides = overrides or {}
    names = {m["name"] for m in g.members.get(tid, [])}
    bad = set(overrides) - names
    if bad:
        sys.exit(f"{tid}: overrides name members that do not exist: {sorted(bad)}")
    g.T(tid, role, name, loc, purpose, conf, notes, g.generic_specs(tid, overrides), recv=recv)


def D(tid, member):
    return "declaration-only;" + g.decl(tid, member)


# ---------------------------------------------------------------- effect instances and scripts
RT = E + "Runtime.h"
T("T00262", "State", "Effects::EffectInstance", RT,
  "Runs one effect script: priority list links, parent, script cursor and registers, token, child count, frame, "
  "particles, 16 transforms and the effect context.", H,
  c(("5BF94.c", "arg1->unk9 = arg0->unk10;"), ("5BF94.c", "u_char func_800D5170(D_800F53B8_t* arg0) { return arg0->unkC[arg0->unkA++]; }")), {
      "previous": ("parent", "Parent instance that spawned this one (fork name previous)", H,
                   c(("5BF94.c", "--arg0->previous->unk14_11;"))),
      "unk8": ("particleCount", "Particles the instance owns", H, c(("5BF94.c", "++arg0->unk8;"))),
      "unk9": ("priority", "Priority; instances are kept in ascending order", H,
               c(("5BF94.c", "if ((currentNode == NULL) || (arg0->unk10 < currentNode->unk9)) {"))),
      "unkA": ("scriptOffset", "Offset of the next script byte", H,
               c(("5BF94.c", "u_char func_800D5170(D_800F53B8_t* arg0) { return arg0->unkC[arg0->unkA++]; }"))),
      "unkC": ("script", "Effect script bytecode", H,
               c(("5BF94.c", "u_char func_800D5170(D_800F53B8_t* arg0) { return arg0->unkC[arg0->unkA++]; }"))),
      "unk10": ("registers", "Four script registers", H, c(("5BF94.c", "arg0->unk10[func_800D5170(arg0)] = func_800D5198(arg0);"))),
      "unk14_0": ("token", "Effect token released when the instance is destroyed", M, c(("5BF94.c", "func_800CF514(arg0->unk14_0);"))),
      "unk14_8": ("busyBits", "Bits OR'ed into the busy result of the secondary effect update (2, or 4 for effect 54)", M,
                  c(("5BF94.c", "var_s3 |= var_s1->unk14_8;"))),
      "unk14_11": ("childCount", "Child instances still running", H, c(("5BF94.c", "++arg0->unk14_11;"))),
      "unk14_16": ("frame", "Frame of the effect, used to sample its curves", H, c(("5BF94.c", "++var_s1->unk14_16;"))),
      "unk18": ("particles", "First particle the instance owns", H, c(("5BF94.c", "temp_a1 = arg0->unk18;"))),
      "unk1C": ("transforms", "Sixteen transform slots", H, c(("5BF94.c", "matrix = &arg0->unk1C[scratch->unk168].unk38;"))),
      "unkD1C": ("context", "Targets, timeline, renderer, effect id and phase", H, c(("5BF94.c", "arg1->unkD1C.unk0 = *arg0->unk0;"))),
  }, recv=r"\b(?:arg0|arg1|var_s0|var_s1|newNode|currentNode|child)")
T("T00263", "Value", "Effects::EffectSpawnRequest", RT,
  "Describes an effect instance to create: its four target slots, priority, script and start offset, parent and token.", H,
  c(("5BF94.c", "spawn.unk14 = D_800F569C->block8Data;"), ("5BF94.c", "arg1->unkD1C.unk0 = *arg0->unk0;")), {
      "unk0": ("source", "Copied to the context source", H, c(("5BF94.c", "arg1->unkD1C.unk0 = *arg0->unk0;"))),
      "unk4": ("emitFrom", "Copied to the context emitFrom slot", H, c(("5BF94.c", "arg1->unkD1C.unkC = *arg0->unk4;"))),
      "unk8": ("target", "Copied to the context target", H, c(("5BF94.c", "arg1->unkD1C.unk18 = *arg0->unk8;"))),
      "unkC": ("emitTo", "Copied to the context emitTo slot", H, c(("5BF94.c", "arg1->unkD1C.unk24 = *arg0->unkC;"))),
      "unk10": ("priority", "Instance priority", H, c(("5BF94.c", "arg1->unk9 = arg0->unk10;"))),
      "unk14": ("script", "Script bytecode", H, c(("5BF94.c", "arg1->unkC = arg0->unk14;"))),
      "unk18": ("scriptOffset", "Start offset in the script", H, c(("5BF94.c", "arg1->unkA = arg0->unk18;"))),
      "unk1C": ("parent", "Parent instance, or null", H, c(("5BF94.c", "arg1->previous = arg0->unk1C;"))),
      "unk20": ("token", "Effect token", H, c(("5BF94.c", "arg1->unk14_0 = arg0->unk20;"))),
  }, recv=r"\b(?:arg0|spawn|sp10)")
T("T00265", "State", "Effects::EffectTimeline", RT,
  "Holds a loaded effect timeline: track count, flags, current frame, active-track bits and the tracks.", H,
  c(("5BF94.c", "p->unk0 = entry->count;"), ("5BF94.c", "if ((arg0->unkD1C.unk30->unk4 >> var_s1) & 1) {")), {
      "unk0": ("trackCount", "Number of tracks", H, c(("5BF94.c", "p->unk0 = entry->count;"))),
      "unk1": ("flags", "Bit 0 holds the frame; bit 1 is set when the timeline has tracks", M,
               c(("5BF94.c", "void func_800D6538(D_800F53B8_t* arg0) { arg0->unkD1C.unk30->unk1 |= 1; }"))),
      "unk2": ("frame", "Current timeline frame", H, c(("5BF94.c", "int cur = arg0->unkD1C.unk30->unk2;"))),
      "unk4": ("activeTracks", "One bit per track still running; zero when the timeline is finished", H,
               c(("5BF94.c", "if ((arg0->unkD1C.unk30->unk4 >> var_s1) & 1) {"))),
      "unk8": ("tracks", "Timeline tracks", H, c(("5BF94.c", "p->unk8[i].unk0 = func_800D5564(D_800F569C->block3Data, arg1, i);"))),
  }, recv=r"\b(?:p|arg0->unkD1C\.unk30)")
T("T00501", "State", "Effects::TimelineTrack", RT,
  "Holds one timeline track: its keys, kind and key count, current key, timer and parameter.", M,
  c(("5BF94.c", "if (arg0->unk6 + 1 >= arg0->unk4.unk0_8) {")), {
      "unk0": ("keys", "Key data of the track", H, c(("5BF94.c", "func_800D57FC_t* base = (func_800D57FC_t*)arg0->unk0;"))),
      "unk4": ("header", "Track kind and key count", H, c(("5BF94.c", "if (arg0->unk6 + 1 >= arg0->unk4.unk0_8) {"))),
      "unk6": ("keyIndex", "Current key; -1 before the first", H, c(("5BF94.c", "arg0->unkC = base[arg0->unk6].unk0_20;"))),
      "unk8": ("curveFrame", "Frame passed to the curve sampler while particles are emitted", L,
               c(("5BF94.c", "func_800D1104(arg1->unk8);"))),
      "unkA": ("keyTimer", "Frames left in the current key", M, c(("5BF94.c", "--arg1->unkA;"))),
      "unkC": ("keyParameter", "Parameter of the current key", L, c(("5BF94.c", "arg0->unkC = base[arg0->unk6].unk0_20;"))),
  }, recv=r"\b(?:arg0|arg1|p->unk8\[i\])")
T("T00502", "Value", "Effects::TimelineTrackHeader", RT, "Gives a timeline track's kind and key count.", M,
  c(("5BF94.c", "switch (arg0->unk4.unk0_0) {")), {
      "unk0_0": ("kind", "Track kind (3 particle emission, ...)", M, c(("5BF94.c", "switch (arg0->unk4.unk0_0) {"))),
      "unk0_8": ("keyCount", "Number of keys", M, c(("5BF94.c", "if (arg0->unk6 + 1 >= arg0->unk4.unk0_8) {"))),
  })
T("T00503", "Value", "Effects::ParticleTrackKey", RT, "Gives one particle emission key: frame, duration and the event arguments.", M,
  c(("5BF94.c", "int key = e[arg1->unk6].unk0_0;")), {
      "unk0_0": ("frame", "Frame at which the key starts", M, c(("5BF94.c", "int key = e[arg1->unk6].unk0_0;"))),
      "unk0_8": ("duration", "Frames the key lasts", M, c(("5BF94.c", "arg0->unkA = base[arg0->unk6].unk0_8;"))),
  }, recv=r"(?:e|base)\[[^\]]*\]")
T("T00504", "Value", "Effects::ShakeTrackKey", RT, "Gives one camera shake key word.", L, D("T00504", "unk0_4"), {
      "unk0_4": ("curve", "Curve passed to the shake start", L,
                 c(("5BF94.c", "func_800D12D8((((func_800D57FC_t2*)&arg0->unk0[arg0->unk6 * 2 + 1])->unk0_4));"))),
  })
T("T00505", "Value", "Effects::ChildTrackKey", RT, "Gives one child-effect key.", L, D("T00505", g.members["T00505"][0]["name"]))
T("T00506", "State", "Effects::ChildTrackView", RT, "Views a timeline track while child effects are spawned.", L,
  D("T00506", g.members["T00506"][0]["name"]))
T("T00507", "Value", "Effects::RendererTrackKey", RT, "Gives one renderer key: frame, renderer selection and argument.", M,
  c(("5BF94.c", "if (temp_a3->unk0_0 == arg0->unkD1C.unk30->unk2) {")), {
      "unk0_0": ("frame", "Frame at which the key applies", M, c(("5BF94.c", "if (temp_a3->unk0_0 == arg0->unkD1C.unk30->unk2) {"))),
  }, recv=r"\btemp_a3")
T("T00509", "Value", "Effects::ColorTrackKey", RT, "Gives one actor or scene color key: frame, duration, color and mode.", M,
  c(("5BF94.c", "if (temp_s0->unk0 == arg0->unkD1C.unk30->unk2) {")), {
      "unk0": ("frame", "Frame at which the key applies", M, c(("5BF94.c", "if (temp_s0->unk0 == arg0->unkD1C.unk30->unk2) {"))),
      "unk2": ("duration", "Duration passed to the color fade", L, c(("5BF94.c", "func_8007B29C(temp_s0->unk7, temp_s0->unk2,"))),
      "unk4": ("red", "Red, biased by 0x80", M, c(("5BF94.c", "arg0->unkD1C.unkC.value.actor.actor, temp_s0->unk4 - 0x80,"))),
      "unk5": ("green", "Green, biased by 0x80", M, c(("5BF94.c", "temp_s0->unk5 - 0x80, temp_s0->unk6 - 0x80);"))),
      "unk6": ("blue", "Blue, biased by 0x80", M, c(("5BF94.c", "temp_s0->unk5 - 0x80, temp_s0->unk6 - 0x80);"))),
      "unk7": ("mode", "Fade mode passed to the color fade", L, c(("5BF94.c", "func_8007B29C(temp_s0->unk7, temp_s0->unk2,"))),
  }, recv=r"\b(?:temp_s0|entry|temp_s2)")
T("T00510", "State", "Effects::TransformControlView", RT, "Views a transform slot's attachment and motion bits.", L,
  D("T00510", "unk22_0"), {
      "unk22_0": ("attachmentMode", "Attachment bits cleared to invalidate attachments", L,
                  c(("5BF94.c", "arg0->unk22_0 = 0;"))),
      "unk22_2": ("motionResumed", "Set to resume the transform's motion", L, c(("5BF94.c", "arg0->unk22_2 = 1;"))),
  }, recv=r"\barg0")
T("T00511", "Value", "Effects::ActionControlKey", RT, "Gives one action-control key: frame word and command.", L,
  c(("5BF94.c", "if ((temp_a2->unk0 & 0x1FF) == arg0->unkD1C.unk30->unk2) {")), {
      "unk0": ("frame", "Frame in the low nine bits", M, c(("5BF94.c", "if ((temp_a2->unk0 & 0x1FF) == arg0->unkD1C.unk30->unk2) {"))),
  }, recv=r"\btemp_a2")
T("T00512", "Value", "Effects::ShakeChannelKey", RT, "Gives one camera shake key: frame and the shake channel parameters.", M,
  c(("5BF94.c", "int key = e[arg1->unk6].unk0_0;")), {
      "unk0_0": ("frame", "Frame at which the key applies", M, c(("5BF94.c", "int key = e[arg1->unk6].unk0_0;"))),
  }, recv=r"\b(?:e|e\[[^\]]*\])")

# ---------------------------------------------------------------- effect package
T("T00270", "State", "Effects::EffectPackage", RT,
  "Points to the loaded effect package blocks: particle definitions, curves, programs, textures, timelines, sounds "
  "and the current action request.", H, c(("5BF94.c", "spawn.unk14 = D_800F569C->block8Data;")), {
      "unkCC": ("auxiliarySoundHandle", "Handle returned when the auxiliary sound bank is bound; sound keys play only while set", L,
                c(("6F014.c", "D_800F569C->unkCC = func_80046608((u_int)arg0);"))),
      "unkD0": ("request", "The action effect request being presented", H, c(("5BF94.c", "if (arg1->index < D_800F569C->unkD0->count) {"))),
  }, recv=r"\b(?:D_800F569C|D_800F57A0|D_800F56C8)")
PFILE = {
    "T00580": {"data": ("blocks", "Package blocks following the header", H, c(("6F014.c", "_parsePfileBlock1((pFileBlock1*)arg0->data);")))},
    "T00573": {"unk0": ("animationCount", "Number of texture animations", H, c(("6F014.c", "D_800F569C->block2Count = block->unk0;")))},
    "T00575": {"unk4": ("definitions", "Particle definitions", H,
                        c(("5BF94.c", "func_800FA098_arg0* def = &D_800F569C->block5Data->unk4[event];")))},
}
for _tid, _name, _purpose in (
        ("T00571", "Effects::PackageBlock1", "Heads package block 1 (texture tables)."),
        ("T00572", "Effects::PackageBlock11", "Heads package block 11 (segmented curves and vibration)."),
        ("T00573", "Effects::PackageBlock2", "Heads package block 2 (particle texture animations)."),
        ("T00574", "Effects::PackageBlock3", "Heads package block 3 (timelines)."),
        ("T00575", "Effects::PackageBlock5", "Heads package block 5 (particle definitions)."),
        ("T00577", "Effects::CurveBlock", "Heads the curve block: curve count and sizes."),
        ("T00578", "Effects::PackageTexture", "Describes one package texture."),
        ("T00579", "Effects::PackageTextureInfo", "Selects one package texture."),
        ("T00580", "Effects::PackageFile", "Heads an effect package file.")):
    T(_tid, "OnDisc", _name, E + "Package.h", _purpose, M, D(_tid, g.members[_tid][0]["name"]), PFILE.get(_tid))
T("T00576", "OnDisc", "Effects::PackageBlock9", E + "Package.h",
  "Gives package block 9: gravity range and drag range with their curves, frame count, transform count and timeline ids.", M,
  c(("6F014.c", "D_800F569C->block9Data->unk0, D_800F5330[D_800F569C->block9Data->unk12], &sp10);")), {
      "unk0": ("gravityRange", "Gravity range sampled by gravityCurve", M,
               c(("6F014.c", "D_800F569C->block9Data->unk0, D_800F5330[D_800F569C->block9Data->unk12], &sp10);"))),
      "unkC": ("dragRange", "Drag range sampled by dragCurve", M,
               c(("6F014.c", "D_800F569C->block9Data->unkC, D_800F5330[D_800F569C->block9Data->unk13]);"))),
      "unk12": ("gravityCurve", "Curve selecting the gravity", M,
                c(("6F014.c", "D_800F569C->block9Data->unk0, D_800F5330[D_800F569C->block9Data->unk12], &sp10);"))),
      "unk13": ("dragCurve", "Curve selecting the drag", M,
                c(("6F014.c", "D_800F569C->block9Data->unkC, D_800F5330[D_800F569C->block9Data->unk13]);"))),
      "unk14": ("frameCount", "Copied to the effect frame limit when primary effects update", L,
                c(("5BF94.c", "D_800F5224 = D_800F569C->block9Data->unk14;"))),
      "unk15": ("transformCount", "Number of transforms the package animates", M,
                c(("5BF94.c", "for (i = 0; i < D_800F569C->block9Data->unk15; ++i) {"))),
      "unk16": ("timelineIds", "Timeline ids loaded by script register", M, c(("5BF94.c", "int id = D_800F569C->block9Data->unk16[index];"))),
  }, recv=r"block9Data")
T("T00315", "State", "Effects::FileLoadContext", E + "Loading.h", "Holds one effect file load in progress.", M,
  D("T00315", g.members["T00315"][0]["name"]))

# ---------------------------------------------------------------- transforms
TR = E + "Transforms.h"
VIA = ";read through effectDefinitionView (T00418) at the same offset"
CLEARED = "Cleared when the instance is initialised; not otherwise read"
T("T00493", "State", "Effects::TransformMotionView", TR, "Views a transform slot while its motion and attachment are resolved.", M,
  D("T00493", "rotation"), {
      "flags": ("flags", "Bit 0 attached to an actor bone, bit 1 target bone resolved, bit 2 motion limited by the definition", M,
                c(("5BF94.c", "state->flags |= 2;"))),
      "unk8": ("reserved8", CLEARED, L, c(("upstream:5BF94.c", "arg1->unk1C[i].unk8 = 0;"))),
      "unkC": ("reservedC", CLEARED, L, c(("upstream:5BF94.c", "*(int*)&arg1->unk1C[i].unkC = 0;"))),
      "unk10": ("reserved10", CLEARED, L, c(("upstream:5BF94.c", "arg1->unk1C[i].unk10[2] = 0;"))),
      "unk2C": ("reserved2C", CLEARED, L, c(("upstream:5BF94.c", "arg1->unk1C[i].unk2C[1] = 0;"))),
  }, recv=r"\b(?:arg1|state)")
T("T00489", "State", "Effects::TransformSlot", TR,
  "Holds one effect transform slot: motion, attachment, world and view matrices; named by offset from the fork view "
  "func_800D0548_arg1 (T00493).", M, D("T00489", "unk38"),
  {**g.names_from_views("T00489", ["T00493"]),
   **{n: (f"reserved{n[3:]}", CLEARED, L, c(("5BF94.c", f"arg1->unk1C[i].{n} = 0;"))) for n in ("unk0", "unk8", "unk10", "unk14", "unk18", "unk28", "unk2C", "unk30")},
   "unk4": ("reserved4", CLEARED, L, c(("5BF94.c", "((short*)&arg1->unk1C[i].unk4)[1] = 0;"))),
   "unkC": ("reservedC", CLEARED, L, c(("5BF94.c", "arg1->unk1C[i].unkC = 0;"))),
   "unk1C": ("positionX", "Position x (same offset as TransformMotionView.position)", L, D("T00489", "unk1C")),
   "unk20": ("positionY", "Position y; set to -0x80 to raise an action effect", M, c(("5BF94.c", "arg0->unk1C[i].unk20 = -0x80;"))),
   "unk24": ("positionZ", "Position z (same offset as TransformMotionView.position)", L, D("T00489", "unk24"))},
  recv=r"unk1C\[[^\]]*\]")
T("T00494", "OnDisc", "Effects::TransformDefinition", TR,
  "Defines how a transform animates: flags, rotation, translation and scale ranges with their curves, and motion limits.", M,
  c(("5BF94.c", "vs_battle_lerpVector(arg0->unk20, D_800F5330[arg0->unk4], &arg2->unk10);")), {
      "flags": ("flags", "Bit 0 animated local transform, bit 1 attach (bits 3-4 source), bit 7 target (bits 10-11 source), bits 12-13 motion kind", M,
                c(("5BF94.c", "switch (arg0->flags & 0x3000) {"), ("5BF94.c", "switch (definition->unk0 & 0x18) {"))),
      "unk2": ("attachActor", "Actor the transform attaches to when the attach source is fixed", M,
               c(("5BF94.c", "state->actor = (u_char)definition->unk2;")) + VIA),
      "unk3": ("attachBone", "Bone the transform attaches to unless flag bit 2 is set", M,
               D("T00494", "unk3") + ";" + c(("5BF94.c", "state->bone = ((u_char*)&definition->unk2)[1];")) + VIA),
      "unk8": ("rotationX", "Initial rotation x", M, c(("5BF94.c", "state->rotation[0] = definition->unk8;")) + VIA),
      "unkC": ("rotationY", "Initial rotation y", M, c(("5BF94.c", "state->rotation[1] = definition->unkC;")) + VIA),
      "unk10": ("rotationZ", "Initial rotation z", M, c(("5BF94.c", "state->rotation[2] = definition->unk10;")) + VIA),
      "unk4": ("translationCurve", "Curve selecting the translation", M,
               c(("5BF94.c", "vs_battle_lerpVector(arg0->unk20, D_800F5330[arg0->unk4], &arg2->unk10);"))),
      "unk6": ("rotationCurve", "Curve selecting the rotation offset", M,
               c(("5BF94.c", "vs_battle_lerpSvector(arg0->unk14, D_800F5330[arg0->unk6], &arg2->unk8);"))),
      "unk7": ("scaleCurve", "Curve selecting the scale", M,
               c(("5BF94.c", "vs_battle_lerpVector(arg0->unk2C, D_800F5330[arg0->unk7], &arg2->unk20);"))),
      "unk14": ("rotationRange", "Rotation offset range", M,
                c(("5BF94.c", "vs_battle_lerpSvector(arg0->unk14, D_800F5330[arg0->unk6], &arg2->unk8);"))),
      "unk20": ("translationRange", "Translation range", M,
                c(("5BF94.c", "vs_battle_lerpVector(arg0->unk20, D_800F5330[arg0->unk4], &arg2->unk10);"))),
      "unk2C": ("scaleRange", "Scale range", M,
                c(("5BF94.c", "vs_battle_lerpVector(arg0->unk2C, D_800F5330[arg0->unk7], &arg2->unk20);"))),
  }, recv=r"\b(?:arg0|definition)")
T("T00495", "State", "Effects::AnimatedLocalTransform", TR, "Holds a transform's sampled rotation offset, translation, scale and local matrix.", M,
  c(("5BF94.c", "RotMatrix_gte(arg1, &arg2->unk48);")), {
      "unk8": ("rotationOffset", "Sampled rotation offset", M, c(("5BF94.c", "arg1->vx += arg2->unk8.vx;"))),
      "unk10": ("translation", "Sampled translation", M, c(("5BF94.c", "TransMatrix(&arg2->unk48, &arg2->unk10);"))),
      "unk20": ("scale", "Sampled scale", M, c(("5BF94.c", "ScaleMatrix(&arg2->unk48, &arg2->unk20);"))),
      "unk48": ("local", "Local matrix", M, c(("5BF94.c", "RotMatrix_gte(arg1, &arg2->unk48);"))),
  }, recv=r"\barg2")

# ---------------------------------------------------------------- particles
PT = E + "Particles.h"
T("T00534", "OnDisc", "Effects::ParticleDefinition", PT,
  "Defines a particle event: texture animation, end and tick events, emission flags, the curve that samples each "
  "range, the ranges themselves, color curves, transform and ring layout.", H,
  c(("5BF94.c", "func_800FA098_arg0* def = &D_800F569C->block5Data->unk4[event];")), {
      "unk0": ("animationId", "Texture animation in package block 2", M, c(("5BF94.c", "func_800D6CF0(&node->unk3C, def->unk1, def->unk0);"))),
      "unk1": ("textureIndex", "Texture table of the animation", M, c(("5BF94.c", "func_800D6CF0(&node->unk3C, def->unk1, def->unk0);"))),
      "unk2": ("endEvent", "Event emitted when a particle ends (1-based)", M, c(("5BF94.c", "*(u_short*)&node->endEvent = *(u_short*)&def->unk2;"))),
      "unk3": ("tickEvent", "Event emitted every tick (1-based); copied with endEvent as one halfword", M,
               "declaration-only;" + g.decl("T00534", "unk3") + ";" + c(("5BF94.c", "*(u_short*)&node->endEvent = *(u_short*)&def->unk2;"))),
      "flags": ("flags", "Emission flags: bits 0-2 spawn origin, 0x38 volume shape, 0x1800 distribution, 0xE000 direction mode, 0x1C0 attraction mode", H,
                c(("5BF94.c", "scratch->flags = def->flags;"))),
      "unk8": ("positionCurve", "Curve sampling positionRange", H, c(("5BF94.c", "sample = def->unk8;"))),
      "unk9": ("extentCurve", "Curve sampling extentRange", H, c(("5BF94.c", "sample = def->unk9;"))),
      "unkA": ("directionCurve", "Curve sampling the direction range", H, c(("5BF94.c", "sample = def->unkA;"))),
      "unkB": ("directionJitterCurve", "Curve sampling the direction jitter range", H, c(("5BF94.c", "sample = def->unkB;"))),
      "unkC": ("speedCurve", "Curve sampling the speed range", H, c(("5BF94.c", "sample = def->unkC;"))),
      "unkD": ("forceCurve", "Curve sampling the force ranges", H, c(("5BF94.c", "sample = def->unkD;"))),
      "unkE": ("accelerationCurve", "Curve sampling the acceleration ranges", H, c(("5BF94.c", "sample = def->unkE;"))),
      "unkF": ("attractionOffsetCurve", "Curve sampling the attraction target offset", H, c(("5BF94.c", "sample = def->unkF;"))),
      "unk10": ("attractionJitterCurve", "Curve sampling the attraction target jitter", H, c(("5BF94.c", "sample = def->unk10;"))),
      "unk11": ("attractionCurve", "Curve sampling the attraction strength range", H, c(("5BF94.c", "sample = def->unk11;"))),
      "unk12": ("massCurve", "Curve sampling the mass range", H, c(("5BF94.c", "sample = def->unk12;"))),
      "unk13": ("gravityScaleCurve", "Curve sampling the gravity scale range", H, c(("5BF94.c", "sample = def->unk13;"))),
      "unk14": ("lifetimeCurve", "Curve sampling the lifetime range", H, c(("5BF94.c", "sample = def->unk14;"))),
      "unk15": ("countCurve", "Curve sampling the particle count range", H, c(("5BF94.c", "sample = def->unk15;"))),
      "unk16": ("intervalCurve", "Curve sampling the emission interval", H, c(("5BF94.c", "sample = def->unk16;"))),
      "unk17": ("innerExtentCurve", "Curve sampling innerExtentRange", H, c(("5BF94.c", "sample = def->unk17;"))),
      "unk18": ("positionRange", "Spawn position range", H, c(("5BF94.c", "vs_battle_lerpVector(def->unk18, D_800F5330[sample], &scratch->unk98);"))),
      "unk24": ("extentRange", "Emission volume extent range", H, c(("5BF94.c", "vs_battle_lerpVector(def->unk24, D_800F5330[sample], &scratch->unkA8);"))),
      "unk30": ("innerExtentRange", "Inner extent ratio range", M, c(("5BF94.c", "sample = func_800CFE1C(def->unk30, D_800F5330[sample]);"))),
      "unk34": ("vectorRanges", "Direction, direction jitter, force min/max, acceleration min/max, attraction offset and jitter ranges", H,
                c(("5BF94.c", "vs_battle_lerpVector(def->unk34[0], D_800F5330[sample], &scratch->unkE8);"))),
      "unk94": ("pairRanges", "Mass, gravity scale, speed, attraction and lifetime min/max ranges", H,
                c(("5BF94.c", "vs_battle_lerp2DVector(def->unk94[2], D_800F5330[sample], scratch->unk118);"))),
      "unkBC": ("countRange", "Particle count range", H, c(("5BF94.c", "count = func_800CFE1C(def->unkBC, D_800F5330[sample]);"))),
      "unkC0": ("intervalRange", "Frames between emissions", H, c(("5BF94.c", "if (frame % func_800CFE1C(def->unkC0, D_800F5330[sample]) != 0) {"))),
      "unkC8": ("transformIndex", "Transform the particles spawn from", H, c(("5BF94.c", "scratch->unk168 = def->unkC8;"))),
      "unkC9": ("attractionTransform", "Transform the particles are attracted to", H, c(("5BF94.c", "scratch->unk16C = def->unkC9;"))),
      "unkCA": ("ringSegments", "Ring emission segments", M, c(("5BF94.c", "scratch->unk184 = def->unkCA;"))),
      "unkCB": ("ringPositions", "Ring emission positions; defaults to ringSegments", M, c(("5BF94.c", "scratch->unk188 = def->unkCB;"))),
  }, recv=r"\b(?:def|dst)")
T("T00535", "State", "Effects::ParticleEmitterScratch", PT,
  "Holds one emission's sampled parameters in the scratchpad while EmitParticleEvent spawns particles.", H,
  c(("5BF94.c", "func_800FA098_arg1* scratch = (func_800FA098_arg1*)0x1F8001D0;")), {
      "flags": ("flags", "Emission flags copied from the definition", H, c(("5BF94.c", "scratch->flags = def->flags;"))),
      "unk28": ("savedFlags", "Copy of the emission flags", L, c(("5BF94.c", "scratch->unk28 = scratch->flags;"))),
      "unk2C": ("vector", "Working vector", M, c(("5BF94.c", "RotMatrix_gte(&scratch->unk2C, (MATRIX*)scratch);"))),
      "unk34": ("aimPoint", "Point the particles aim at", M, c(("5BF94.c", "vs_battle_svecToVec(&scratch->unk2C, &scratch->unk34);"))),
      "unk44": ("spawnPoint", "Spawn point in world units", M, c(("5BF94.c", "scratch->unk44.vx = scratch->unk98.vx >> 12;"))),
      "unk64": ("aimMatrix", "Look-at matrix from spawnPoint to aimPoint", M,
                c(("5BF94.c", "vs_battle_lookAt(&scratch->unk44, &scratch->unk34, &scratch->unk64);"))),
      "unk98": ("position", "Sampled spawn position", H, c(("5BF94.c", "vs_battle_lerpVector(def->unk18, D_800F5330[sample], &scratch->unk98);"))),
      "unkA8": ("extent", "Sampled emission extent", H, c(("5BF94.c", "vs_battle_lerpVector(def->unk24, D_800F5330[sample], &scratch->unkA8);"))),
      "unkB8": ("direction", "Unit direction of one particle", M, c(("5BF94.c", "RotTrans(&scratch->unk2C, &scratch->unkB8, &scratch->unkB8.pad);"))),
      "unkC8": ("directionJitter", "Sampled direction jitter", H, c(("5BF94.c", "vs_battle_lerpVector(def->unk34[1], D_800F5330[sample], &scratch->unkC8);"))),
      "unkD8": ("innerExtent", "Extent scaled by the inner extent ratio", M, c(("5BF94.c", "scratch->unkD8.vx = (scratch->unkA8.vx * sample) >> 12;"))),
      "unkE8": ("baseDirection", "Sampled direction", H, c(("5BF94.c", "vs_battle_lerpVector(def->unk34[0], D_800F5330[sample], &scratch->unkE8);"))),
      "unkF8": ("forceMin", "Sampled minimum force", H, c(("5BF94.c", "vs_battle_lerpSvector(def->unk34[2], D_800F5330[sample], &scratch->unkF8);"))),
      "unk100": ("forceMax", "Sampled maximum force", H, c(("5BF94.c", "vs_battle_lerpSvector(def->unk34[3], D_800F5330[sample], &scratch->unk100);"))),
      "unk108": ("accelerationMin", "Sampled minimum acceleration", H, c(("5BF94.c", "vs_battle_lerpSvector(def->unk34[4], D_800F5330[sample], &scratch->unk108);"))),
      "unk110": ("accelerationMax", "Sampled maximum acceleration", H, c(("5BF94.c", "vs_battle_lerpSvector(def->unk34[5], D_800F5330[sample], &scratch->unk110);"))),
      "unk118": ("speedRange", "Sampled speed min and max", H, c(("5BF94.c", "vs_battle_lerp2DVector(def->unk94[2], D_800F5330[sample], scratch->unk118);"))),
      "unk120": ("speed", "Speed of one particle", H, c(("5BF94.c", "scratch->unk120 ="))),
      "unk124": ("massRange", "Sampled mass min and max", H, c(("5BF94.c", "vs_battle_lerp2DVector(def->unk94[0], D_800F5330[sample], scratch->unk124);"))),
      "unk12C": ("gravityScaleRange", "Sampled gravity scale min and max", H, c(("5BF94.c", "vs_battle_lerp2DVector(def->unk94[1], D_800F5330[sample], scratch->unk12C);"))),
      "unk134": ("lifetimeRange", "Sampled lifetime min and max", H, c(("5BF94.c", "vs_battle_lerp2DVector(def->unk94[4], D_800F5330[sample], scratch->unk134);"))),
      "unk13C": ("attractionOffset", "Sampled attraction target offset", H, c(("5BF94.c", "vs_battle_lerpSvector(def->unk34[6], D_800F5330[sample], &scratch->unk13C);"))),
      "unk144": ("attractionJitter", "Sampled attraction target jitter", H, c(("5BF94.c", "vs_battle_lerpSvector(def->unk34[7], D_800F5330[sample], &scratch->unk144);"))),
      "unk14C": ("attractionRange", "Sampled attraction strength min and max", H, c(("5BF94.c", "vs_battle_lerp2DVector(def->unk94[3], D_800F5330[sample], scratch->unk14C);"))),
      "unk154": ("colorCurves", "Red, green and blue curves and the particle age", H, c(("5BF94.c", "scratch->unk154.packed = *(int*)&def->rCurve;"))),
      "unk168": ("transformIndex", "Transform the particles spawn from", H, c(("5BF94.c", "scratch->unk168 = def->unkC8;"))),
      "unk16C": ("attractionTransform", "Transform the particles are attracted to", H, c(("5BF94.c", "scratch->unk16C = def->unkC9;"))),
      "unk170": ("sinAngle0", "Sine of the first random emission angle", M, c(("5BF94.c", "scratch->unk170 = rsin(angle);"))),
      "unk174": ("sinAngle1", "Sine of the second random emission angle", M, c(("5BF94.c", "scratch->unk174 = rsin(angle);"))),
      "unk178": ("cosAngle0", "Cosine of the first random emission angle", M, c(("5BF94.c", "scratch->unk178 = rcos(angle);"))),
      "unk17C": ("cosAngle1", "Cosine of the second random emission angle", M, c(("5BF94.c", "scratch->unk17C = rcos(angle);"))),
      "unk180": ("transparency", "Transparency mode given to the particles", H, c(("5BF94.c", "scratch->unk180 = def->transparencyCurve;"))),
      "unk184": ("ringSegments", "Ring emission segments", H, c(("5BF94.c", "step = ONE / scratch->unk184;"))),
      "unk188": ("ringPositions", "Ring emission positions", H, c(("5BF94.c", "D_800EC328 %= scratch->unk188;"))),
  }, recv=r"\b(?:scratch|arg0|arg1|s6)")
T("T00497", "State", "Effects::Particle", PT,
  "Holds one particle: links, motion, texture animation, emission flags, transparency, attraction, lifetime, events, "
  "color curves and age.", H, c(("5BF94.c", "node = func_800D27F0(arg0);")), {
      "unk3C": ("textureAnimation", "Texture animation state", H, c(("5BF94.c", "func_800D6CF0(&node->unk3C, def->unk1, def->unk0);"))),
      "unk5C": ("emitFlags", "Emission flags of the event that spawned it", H, c(("5BF94.c", "node->unk5C = scratch->flags;"))),
      "unk60": ("transparency", "Transparency mode (from the definition, the event or the parent)", M,
                "declaration-only;" + g.decl("T00497", "unk60", True) + ";written as unk60_0 in the pinned layout;"
                + c(("5BF94.c", "node->unk60_0 = scratch->unk180;"))),
      "unk62": ("attractionTarget", "Point the particle is attracted to (effectAttractionView targetX)", M,
                c(("5BF94.c", "node->unk62[0] = scratch->unk13C.vx;"))),
      "unk6A": ("transformIndex", "Transform it spawned from", H, c(("5BF94.c", "node->unk6A = scratch->unk168;"))),
      "unk6B": ("attractionTransform", "Transform it is attracted to", H, c(("5BF94.c", "node->unk6B = scratch->unk16C;"))),
      "unk70": ("attraction", "Attraction strength (effectAttractionView attraction)", M, c(("5BF94.c", "node->unk70 = value;"))),
      "unk74": ("colorCurves", "Red, green and blue curves", H, c(("5BF94.c", "*(int*)node->unk74 = scratch->unk154.packed;"))),
      "unk77": ("age", "Ticks since it spawned; 0xFF when allocated", H, c(("5BF94.c", "++node->unk77;"))),
  }, recv=r"\b(?:node|parent|arg0|arg1)")
T("T00820", "State", "Effects::ParticleMotion", PT, "Holds a particle's mass, gravity scale, position, velocity, force and acceleration.", H,
  "declaration-only;" + g.decl("T00820", "mass", True) + ";upstream layout adopted for the particle node", recv=None)
T("T00821", "Value", "Effects::FixedVector", PT, "Holds a 20.12 fixed-point vector.", M,
  "declaration-only;" + g.decl("T00821", "vx", True), recv=None)
T("T00419", "State", "Effects::ParticleMotionView", PT, "Views a particle's motion while it is integrated.", M,
  D("T00419", "mass") + ";view of Effects::ParticleMotion; fold into it when copying", recv=r"\b(?:motion|state|arg0)")
T("T00417", "State", "Effects::ParticleAttractionView", PT, "Views a particle's attraction target and strength while it is integrated.", M,
  D("T00417", "targetX") + ";view of Effects::Particle; fold into it when copying", recv=r"\b(?:view|attraction|arg0)")
T("T00513", "State", "Effects::ParticleTextureAnimation", PT,
  "Steps a particle's texture animation program: frame timer, frame offsets, program cursor and texture.", M,
  c(("5BF94.c", "int w = *(int*)(arg0->unk18 + arg0->unkE);")), {
      "unk6": ("frameTimer", "Ticks left on the current frame; zero when the program has ended", M, c(("5BF94.c", "arg0->unk6 -= 2;"))),
      "unk8": ("frameOffsetX", "First 14-bit offset of a 0x1 program word", L, c(("5BF94.c", "arg0->unk8 = (w << 18) >> 18;"))),
      "unkA": ("frameOffsetY", "Second 14-bit offset of a 0x1 program word", L, c(("5BF94.c", "arg0->unkA = (w << 4) >> 18;"))),
      "unkE": ("cursor", "Byte offset of the next program word", H, c(("5BF94.c", "int w = *(int*)(arg0->unk18 + arg0->unkE);"))),
      "unk18": ("program", "Animation program in package block 2", H,
                c(("5BF94.c", "arg0->unk18 = (u_char*)D_800F569C->block2Data + D_800F569C->block2Data[arg2 + 2];"))),
  }, recv=r"\b(?:arg0|node->unk3C)")
T("T00539", "State", "Effects::PluginEffectState", PT, "Views a plugin effect's position, transparency and variant when it emits particles.", M,
  c(("5BF94.c", "scratch->unk98.vx += parent->unkC.vx >> 12;")), {
      "unkC": ("position", "Position (20.12) added to the spawn position for emission flag 4", M, c(("5BF94.c", "scratch->unk98.vx += parent->unkC.vx >> 12;"))),
      "unk60": ("transparency", "Transparency given to emitted particles", M, c(("5BF94.c", "scratch->unk180 = parent->unk60;"))),
      "unk60_8": ("variant", "Variant passed to emitted events", L, c(("5BF94.c", "arg3 = parent->unk60_8 & 0xF;"))),
  }, recv=r"\bparent")
T("T00536", "State", "Effects::ParticleQuadSet", PT, "Holds a plugin's particle quads: definition, count, control points, age and animation.", M,
  D("T00536", "unk2"), {
      "unk1": ("definitionIndex", "Particle definition in package block 5", M,
               c(("EFFECT/particleDispatch.h", "temp_s2 = &D_800F569C->block5Data->unk4[temp_s1->unk1];"))),
      "unk2": ("count", "Number of particles", M, c(("EFFECT/particleSetup.h", "for (i = 0; i < arg3->unk2; ++i) {"))),
      "unk4": ("controlPoints", "Four control points per particle", M, c(("EFFECT/particleSetup.h", "temp_s1 = arg3->unk4 + (i * 4);"))),
      "unkC": ("age", "Age used to sample the curves", M, c(("EFFECT/particleSetup.h", "arg0->unk18, vs_battle_sampleCurve(arg0->unk8, arg3->unkC), &arg1->unk98);"))),
      "unk10": ("textureAnimation", "Texture animation state", M, c(("EFFECT/particleRender.h", "func_800D6D24(&arg3->unk10);"))),
  }, recv=r"\b(?:arg3|temp_s1)")
T("T00537", "State", "Effects::ParticleQuadWord", PT, "Declares a two-field word of a particle quad set; not read.", L,
  D("T00537", g.members["T00537"][0]["name"]), recv=None)
T("T00538", "State", "Effects::TrailQuadSet", PT, "Holds a trail plugin's quads: definition and texture animation.", M,
  D("T00538", "unk1"), {
      "unk1": ("definitionIndex", "Particle definition in package block 5", M,
               c(("EFFECT/trails5.h", "temp_s3 = &D_800F569C->block5Data->unk4[temp_v0->unk1];"))),
      "unkC": ("textureAnimation", "Texture animation state", M, c(("EFFECT/trails5.h", "func_800D6D24(&arg3->unkC);"))),
  }, recv=r"\b(?:arg3|temp_v0|temp_s0)")
T("T00268", "State", "Effects::ParticleGravity", PT, "Holds the gravity applied to particles.", H,
  c(("5BF94.c", "int gravityX = D_800F5620.unk0;")), {
      "unk0": ("x", "Gravity x", H, c(("5BF94.c", "int gravityX = D_800F5620.unk0;"))),
      "unk4": ("y", "Gravity y", H, c(("5BF94.c", "int gravityY = D_800F5620.unk4;"))),
      "unk8": ("z", "Gravity z", H, c(("5BF94.c", "int gravityZ = D_800F5620.unk8;"))),
  }, recv=r"D_800F5620")
T("T00490", "State", "Effects::ParticleEndpointView", PT, "Views a particle's two screen endpoints while one steps toward the other.", L,
  c(("5BF94.c", "dy = arg0->unk12 - arg0->unkE;")), {
      "unkC": ("endX", "Moving endpoint x", L, c(("5BF94.c", "if (*(int*)&arg0->unkC == *(int*)&arg0->unk10) {"))),
      "unkE": ("endY", "Moving endpoint y", L, c(("5BF94.c", "dy = arg0->unk12 - arg0->unkE;"))),
      "unk10": ("startX", "Fixed endpoint x", L, c(("5BF94.c", "if (*(int*)&arg0->unkC == *(int*)&arg0->unk10) {"))),
      "unk12": ("startY", "Fixed endpoint y", L, c(("5BF94.c", "dy = arg0->unk12 - arg0->unkE;"))),
  }, recv=r"\barg0")

# ---------------------------------------------------------------- camera and screen
CAM = E + "Camera.h"
T("T00266", "State", "Effects::CameraShakeChannel", CAM,
  "Holds one camera shake channel: phase, shape curve, phase-step modes and curves, amplitude and angle.", M,
  c(("5BF94.c", "v = ((rsin(D_800F54B8[i].unk5 * 32) * v) * D_800F54B8[i].unk4) >> 8;")), {
      "unk0": ("phase", "Curve phase; 0xFF when idle", M, c(("5BF94.c", "D_800F54B8[i].unk0 = 0xFF;"))),
      "unk1_0": ("shapeCurve", "Segmented curve sampled at the phase", M, c(("5BF94.c", "p->unk1_0 = arg1;"))),
      "unk1_5": ("curveStep", "Steps the phase by stepCurve", L, c(("5BF94.c", "if (arg0->unk1_5) {"))),
      "unk1_6": ("randomStep", "Steps the phase randomly within randomStepCurve", L, c(("5BF94.c", "if (arg0->unk1_6) {"))),
      "unk2": ("stepCurve", "Curve giving the phase step", L, c(("5BF94.c", "p->unk2 = arg4;"))),
      "unk3": ("randomStepCurve", "Curve giving the random phase step", L, c(("5BF94.c", "p->unk3 = arg6;"))),
      "unk4": ("amplitude", "Amplitude", M, c(("5BF94.c", "p->unk4 = arg2;"))),
      "unk5": ("angle", "Angle (times 32) of the shake direction", M, c(("5BF94.c", "p->unk5 = func_800D1E78(p);"))),
  }, recv=r"\b(?:arg0|p|D_800F54B8\[[^\]]*\])")
T("T00267", "State", "Effects::EffectCamera", CAM, "Holds the effect camera: pose, shake offsets and clip.", M,
  c(("5BF94.c", "offset.vx = rcos(yaw) * (camera->unk20 >> 12);")), {
      "unk20": ("shakeLateral", "Shake offset along the camera's horizontal axis", M, c(("5BF94.c", "offset.vx = rcos(yaw) * (camera->unk20 >> 12);"))),
      "unk24": ("shakeVertical", "Shake offset along the camera's vertical axis", M, c(("5BF94.c", "offset.vy += rcos(pitch) * (camera->unk24 >> 12);"))),
      "unk28": ("shakeDepth", "Shake offset along the camera's view axis", M, c(("5BF94.c", "offset.vy = -rsin(pitch) * (camera->unk28 >> 12);"))),
      "unk2C": ("shakeRoll", "Shake roll offset (biased by 0x8000)", M, c(("5BF94.c", "int rollOffset = camera->unk2C - 0x8000;"))),
  }, recv=r"\b(?:camera|arg0|D_800F54D8)")
T("T00540", "State", "Effects::Plg128::CameraEffectState", E + "Plugins\\Plg128.h", "Holds the camera plugin's endpoints and look-at matrix.", M,
  c(("EFFECT/PLG128.BIN/F8.c", "vs_battle_lookAt(&arg0->unk98, &arg0->unk88, &arg0->unkA8);")), {
      "unk88": ("lookAt", "Point looked at", M, c(("EFFECT/PLG128.BIN/F8.c", "vs_battle_lookAt(&arg0->unk98, &arg0->unk88, &arg0->unkA8);"))),
      "unk98": ("eye", "Camera position", M, c(("EFFECT/PLG128.BIN/F8.c", "vs_battle_lookAt(&arg0->unk98, &arg0->unk88, &arg0->unkA8);"))),
      "unkA8": ("view", "Look-at matrix", M, c(("EFFECT/PLG128.BIN/F8.c", "vs_battle_lookAt(&arg0->unk98, &arg0->unk88, &arg0->unkA8);"))),
  }, recv=r"\b(?:arg0|arg1)")
T("T00213", "State", "Effects::ScreenEffectOverlay", E + "ScreenEffects.h",
  "Holds the screen effect overlay: loading state, feedback layer, radial warp and vignette.", M,
  c(("5BF94.c", "p->unk0 = 1;")), {
      "unk0": ("loading", "Set while the overlay loads; scripts wait for it", M, c(("4A0A8.c", "if ((D_800EB9B4 != NULL) && (D_800EB9B4->unk0 != 0)) {"))),
      "unk2": ("feedbackLayer", "Feedback layer; tiles are drawn from layer 3 up", M, c(("GIM/SCREFF2.PRG/0.c", "D_800EB9B4->unk2 = arg0;"))),
      "unk1": ("opaque1", "Tested as 0 or 1 when the overlay is released or its feedback layer set", L,
               c(("5BF94.c", "if (D_800EB9B4->unk1 == 1) {"))),
      "unk4": ("radialWarpArg0", "First SetRadialWarp argument; 0x1000 in both halves disables the warp", L,
               c(("GIM/SCREFF2.PRG/0.c", "D_800EB9B4->unk4 = arg0;"))),
      "unk6": ("radialWarpArg1", "Second SetRadialWarp argument", L, c(("GIM/SCREFF2.PRG/0.c", "D_800EB9B4->unk6 = arg1;"))),
      "unk8": ("vignetteEnabled", "Draws the vignette while set", H, c(("GIM/SCREFF2.PRG/5E8.c", "void func_800F9DE8(int arg0) { D_800EB9B4->unk8 = arg0; }"))),
  }, recv=r"\b(?:p|temp_s0|D_800EB9B4)")
T("T00234", "State", "Field::WeatherParticle", "Native\\Game\\src\\Field\\Weather.h",
  "Holds one weather particle (rain, snow, embers): position, screen position and per-kind state bytes.", M,
  c(("146C.c", "if (((u_short)particle->unk8 < 320) && ((u_short)particle->unkA < 240)) {")), {
      "unk8": ("screenX", "Screen x of a screen-space particle; -1 when unused", M, c(("146C.c", "if (((u_short)particle->unk8 < 320) && ((u_short)particle->unkA < 240)) {"))),
      "unkA": ("screenY", "Screen y of a screen-space particle", M, c(("146C.c", "if (((u_short)particle->unk8 < 320) && ((u_short)particle->unkA < 240)) {"))),
      "unkD": ("swayPhase", "Phase of the sideways sway; also the splash frame of rain", L, c(("146C.c", "particle->position.vy -= rsin(particle->unkD << 6) >> 9;"))),
      "unkE": ("fallShift", "Shift applied to the fall speed of rain", L, c(("146C.c", "particle->position.vy += D_800F1D98[1] << particle->unkE;"))),
      "unkF": ("speed", "Rise speed of embers; random phase of screen particles", L, c(("146C.c", "particle->position.vy -= particle->unkF * vs_gametime_tickspeed;"))),
  }, recv=r"\b(?:particle|arg0|var_s1)")

# ---------------------------------------------------------------- trails, requests and primitives
T("T00343", "State", "Effects::Trail", E + "Trails.h", "Holds one trail effect's state.", M, D("T00343", g.members["T00343"][0]["name"]))
T("T00421", "State", "Effects::BoneTrail", E + "Trails.h", "Holds a bone trail: configuration, age and the two edge histories.", M,
  D("T00421", "config"), recv=r"\b(?:t|trail|arg0|arg1)")
T("T00294", "Packet", "Render::TrailPacket", "Native\\Graphics\\src\\LegacyPackets.h", "Is one trail segment's gouraud quad packet.", M,
  D("T00294", g.members["T00294"][0]["name"]), {
      "b0": ("blue0", "Blue of the first head vertex", M, c(("5BF94.c", "p->b0 = p->b1 = D_800EC308[t->config.color].b * head - 128;"))),
      "b1": ("blue1", "Blue of the second head vertex", M, c(("5BF94.c", "p->b0 = p->b1 = D_800EC308[t->config.color].b * head - 128;"))),
      "b2": ("blue2", "Blue of the first tail vertex", M, c(("5BF94.c", "p->b2 = p->b3 = D_800EC308[t->config.color].b * tail - 128;"))),
      "b3": ("blue3", "Blue of the second tail vertex", M, c(("5BF94.c", "p->b2 = p->b3 = D_800EC308[t->config.color].b * tail - 128;"))),
  }, recv=r"\bp")
T("T00363", "Value", "Effects::ActionEffectRequest", RT, "Requests an action's effects: effect id, targets, others and position.", H,
  D("T00363", "effectId"), recv=r"\b(?:request|arg0|sp10|D_800F5230)")
T("T00364", "Value", "Effects::ActionEffectTarget", RT, "Gives one effect target: kind and an actor/part or a position.", H,
  D("T00364", "type"), {"value": ("value", "Actor and part, or position, by kind", M, c(("5BF94.c", "scratch->unk2C = arg0->unkD1C.unkC.value.position;"))),
                        "unk2": ("colorPreset", "Byte 2; selects the particle color preset of the target slot", L,
                                 c(("5BF94.c", "dst->rCurve = D_800EC330[arg0->unkD1C.unk18.unk2][i][0];")))}, recv=r"\b(?:targets\[[^\]]*\]|source|arg0->unkD1C\.unk\w+)")
g.TYPES.append(["T00743", "Value", "Effects::EffectRenderer", RT,
                "Is the function type of an effect renderer called each frame with its renderer state.", H,
                c(("BATTLE/BATTLE.PRG/6E644.h", "typedef int (*effectExec)(func_800D4910_t*, u_int, int);"))])
T("T00498", "State", "Effects::RendererSlot", RT, "Holds one attached renderer: next slot, function and its trail or state.", M,
  D("T00498", g.members["T00498"][0]["name"]), {
      "next": ("next", "Next attached renderer", M, D("T00498", "next")),
      "unk8": ("rendererState", "State the renderer owns (the bone trail for trail renderers)", M,
               c(("5BF94.c", "effectTrailState* t = node->unk8;"))),
  }, recv=r"\b(?:var_s1|node)")
T("T00269", "State", "Effects::EffectLoadState", E + "Loading.h", "Holds effect package loading state; not read by decompiled functions.", L,
  D("T00269", g.members["T00269"][0]["name"]), recv=None)
T("T00434", "Value", "Effects::HitEffectRequest", RT, "Holds a hit effect request built when an action hit is presented.", L,
  D("T00434", "unk0"), recv=r"\bsp10")
T("T00443", "Value", "Battle::HitTargetWord", "Native\\Game\\src\\Battle\\ActionResolution.h",
  "Gives a hit entry's target actor, limb and impact kind.", M, D("T00443", g.members["T00443"][0]["name"]), {
      "unk3": ("impactKind", "Selects the hit sounds and impact effects", M, c(("146C.c", "switch (temp_s3->unk4C[i].unk0.unk3) {"))),
  }, recv=r"unk0")
T("T00292", "State", "Effects::Plg005::AnalyzeGridQuad", E + "Plugins\\Plg005.h", "Holds one Analyze grid quad of plugin 005.", L,
  D("T00292", g.members["T00292"][0]["name"]), recv=r"\bfp")
PRIM = "Native\\Graphics\\src\\LegacyPackets.h"
for _tid, _name, _tex in (("T00626", "Render::EffectFlatQuadPacket", "flat textured"), ("T00627", "Render::EffectGouraudQuadPacket", "gouraud textured"),
                          ("T00721", "Render::ParticleQuadPacket", "gouraud textured"), ("T00720", "Render::ParticleLinePacket", "line")):
    _ov = {}
    _names = [m["name"] for m in g.members[_tid]]
    for n in ("unk0", "unk1", "unk2"):
        if n in _names:
            _ov[n] = (f"tag{n[-1]}", "Byte of the ordering-table tag, written when the packet is added", L, D(_tid, n))
    if "unk3" in _names:
        _ov["unk3"] = ("wordCount", "Packet length in words", M, D(_tid, "unk3"))
    for n in ("unk38", "unk2C"):
        if n in _names:
            _ov[n] = ("drawMode", "Draw mode command word (0xE1)", M, D(_tid, n))
    T(_tid, "Packet", _name, PRIM, f"Is one {_tex} effect packet with its draw mode word.", M, D(_tid, _names[0]), _ov, recv=None)
for _tid, _name in (("T00685", "Render::ClosedRingPacket"), ("T00687", "Render::ClosedRingTexCoord"), ("T00688", "Render::GridPacket"),
                    ("T00715", "Render::Mesh10Packet"), ("T00717", "Render::Mesh10TexCoord"), ("T00718", "Render::MirrorSurfacePacket"),
                    ("T00722", "Render::RingPacket"), ("T00724", "Render::RotatingSurfacePacket"), ("T00727", "Render::SurfacePacket")):
    T(_tid, "Packet", _name, PRIM, f"Is one effect template packet ({_name.split('::')[1]}).", M, D(_tid, g.members[_tid][0]["name"]), recv=None)
TPL = E + "Templates\\TemplateState.h"
for _tid, _name in (("T00686", "Effects::ClosedRingState"), ("T00689", "Effects::GridState"), ("T00716", "Effects::Mesh10State"),
                    ("T00719", "Effects::MirrorSurfaceState"), ("T00723", "Effects::RingState"), ("T00725", "Effects::RotatingSurfaceState"),
                    ("T00728", "Effects::SurfaceState")):
    T(_tid, "State", _name, TPL, f"Holds one {_name.split('::')[1][:-5]} effect template's state.", M,
      D(_tid, g.members[_tid][0]["name"]), recv=r"\b(?:state|effect|arg0|arg3)")

if __name__ == "__main__":
    g.write()
