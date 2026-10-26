"""Regressions for native source adaptations, without game assets."""

import csv
import re
import unittest
from pathlib import Path

import copy_fixups
import native_views


class EncounterFixupTests(unittest.TestCase):
    def test_every_native_redirect_retains_its_module_symbol_binding(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            rows = {row["id"]: row for row in csv.DictReader(stream)}
        source = (root / r"Native\Game\src\Boot\ModulePointers.c").read_text(
            encoding="utf-8"
        )
        for identifier in copy_fixups.NATIVE_FUNCTION_REDIRECTS:
            name = rows[identifier]["final_name"].replace("::", "_")
            self.assertTrue(
                f"(void*)&{name}," in source,
                f"Native alias {identifier} must retain its original module binding",
            )

    def test_fixed_trig_entries_redirect_to_existing_typed_native_bodies(self):
        for identifier, name in (
            ("F00764", "Core_Gte_Cos"),
            ("F00765", "Core_Gte_Sin"),
            ("F03400", "Core_Gte_Cos"),
            ("F03401", "Core_Gte_Sin"),
        ):
            self.assertEqual(
                copy_fixups.NATIVE_FUNCTION_REDIRECTS[identifier],
                (name, r"Native\Engine\src\Core\Gte.cpp"),
            )
        root = Path(__file__).resolve().parents[2]
        header = (root / r"Native\Engine\src\Core\Math\Trig.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("#define Core_Math_CosineFixedPoint Core_Gte_Cos", header)
        self.assertIn("#define Core_Math_SineFixedPoint Core_Gte_Sin", header)

    def test_tile_translation_callers_preserve_the_full_actor_id_abi(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Game\src\Actors\Movement.c").read_text(
            encoding="utf-8"
        )
        self.assertFalse(
            "((u_int (*)(u_char, void*, int))Actors_TranslateActorToTileAtRate)"
            in source,
            "Tile-translation callers must not narrow the int actor-ID ABI",
        )
        self.assertEqual(
            source.count(
                "Actors_TranslateActorToTileAtRate(arg0->instance.slot, "
                "(Actors_TileTarget*)&"
            ),
            3,
        )

    def test_navigation_leaves_own_typed_entries_and_forward_the_actor_id(self):
        owned = self.owned_items()
        for identifier, name in (
            ("F01801", "Battle_Ai_ActorNavigationOverrideStatus"),
            ("F01802", "Battle_Ai_HasActorNavigationState"),
            ("F01803", "Battle_Ai_CopyActorNavigationOverrideState"),
        ):
            self.assertEqual(owned[identifier]["name"], name)
            self.assertEqual(
                owned[identifier]["path"],
                r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
            )
        root = Path(__file__).resolve().parents[2]
        caller = (root / r"Native\Game\src\Actors\Movement.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("Battle_Ai_ActorNavigationOverrideStatus(arg0);", caller)
        self.assertNotIn("Battle_Ai_ActorNavigationOverrideStatus();", caller)

    def test_vignette_producers_publish_one_encoded_ordering_word(self):
        for identifier, head, packet in (
            ("F02461", "arg3", "arg2"),
            ("F02462", "temp_s4", "var_s2"),
        ):
            fixes = copy_fixups.FIXES[identifier]
            old = "\n".join(item[0] for item in fixes)
            new = "\n".join(item[1] for item in fixes)
            self.assertIn(f"void** {head}", old)
            self.assertIn(f"u_long* {head}", new)
            self.assertIn(f"(*{head} & 0xFFFFFF)", new)
            self.assertIn(f"*{head} = LegacyGpu_Handle({packet});", new)
            self.assertNotIn(f"LegacyGpu_Handle(*{head})", new)
            self.assertNotIn("uintptr_t", new)

    @staticmethod
    def owned_items():
        with Path(__file__).resolve().with_name("owned.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            return {row["id"]: row for row in csv.DictReader(stream)}

    def test_relocation_entries_own_complete_existing_motion_requests(self):
        owned = self.owned_items()
        names = {
            "F04184": "Actors_BeginTeleport",
            "F04185": "Actors_BeginTileTravel",
            "F04186": "Actors_BeginTimedTravel",
        }
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Game\src\Actors\Movement.c").read_text(
            encoding="utf-8"
        )
        for identifier, name in names.items():
            self.assertEqual(owned[identifier]["name"], name)
            self.assertEqual(
                owned[identifier]["path"], r"Native\Game\src\Actors\MotionRequests.cpp"
            )
            self.assertNotIn(f"void {name}(", source)

    def test_actor_sound_transition_owns_both_catalog_occurrences_and_callers(self):
        owned = self.owned_items()
        for identifier in ("F00995", "F01007"):
            self.assertEqual(owned[identifier]["name"], "Audio_UpdateActorSoundTransition")
            self.assertEqual(
                owned[identifier]["path"], r"Native\Game\src\Actors\SoundOverrides.cpp"
            )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Game\src\Actors\ModelRendering.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("Audio_UpdateActorSoundTransition(actor);", source)
        self.assertIn("Audio_UpdateActorSoundTransition(attached);", source)
        self.assertNotIn("soundEffect.parameter -= step;", source)
        header = (root / r"Native\Engine\src\Audio\ActorSound.h").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("int Audio_UpdateActorSoundTransition()", header)

    def test_ai_target_memory_aliases_preserve_the_original_prefix(self):
        old, new, _ = next(
            fix for fix in copy_fixups.TYPE_FIXES["T00532"]
            if fix[0] == "char reserved3C[8];"
        )
        self.assertIn(old, new)
        self.assertIn("u_int rememberedTargetTile;", new)
        self.assertIn("u_short targetMemoryFrames;", new)
        self.assertIn("short savedRelationOptions;", new)
        aliases = native_views.NATIVE_FIELD_ALIASES["T00532"]
        self.assertEqual(aliases["rememberedTargetTile"], 0x3C)
        self.assertEqual(aliases["targetMemoryFrames"], 0x40)
        owned = self.owned_items()
        self.assertEqual(owned["F01792"]["name"], "Battle_Ai_RememberAiActionTarget")
        self.assertEqual(
            owned["F01792"]["path"],
            r"Native\Game\src\Battle\Ai\PlanningPrimitives.cpp",
        )

    def test_lethal_chain_success_keeps_credit_without_continuing(self):
        old, new, _ = copy_fixups.FIXES["F00099"][0]
        self.assertIn("abilityInputSucceeded == 1)", new.splitlines()[0])
        self.assertIn("currentHp != 0", new.splitlines()[1])
        self.assertIn("} else {\n            Battle_FinishBattleAbilitySequence();", new)
        for operation in (
            "++Battle_actionState->chainStep;",
            "++Battle_actionState->queueIndex;",
            "Battle_QueueChainCount(Battle_actionState->queueIndex);",
        ):
            self.assertEqual(old.count(operation), 1)
            self.assertEqual(new.count(operation), 1)
        self.assertNotIn("kills.totalKills", old)
        self.assertNotIn("kills.totalKills", new)

    def test_both_credit_paths_bank_points_beyond_pending_thresholds(self):
        old, new, _ = copy_fixups.FIXES["F00099"][1]
        source = old + "\n" + old
        source = source.replace(old, new)
        self.assertEqual(source.count("kills.totalKills < 0xFFFF"), 2)
        self.assertNotIn("Battle_battleAbilityPointRequirements", source)

    def test_music_cancellation_clears_the_native_pointer_field(self):
        source = "    *(int*)&Audio_Seq_pendingMusicData.param1 = 0;"
        for old, new, _ in copy_fixups.FIXES["F03246"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source, "    Audio_Seq_pendingMusicData.param1 = 0;")

    def test_particle_child_events_project_native_parent_fields(self):
        source = """        ++node->age;
        Effects_EmitParticleEvent(arg0, node->tickEvent - 1, 0, 0, (void*)node);
        Effects_EmitParticleEvent(arg0, node->endEvent - 1, 0, 0, (void*)node);
        Effects_EmitParticleEvent(arg0, node->endEvent - 1, 0, 0, (void*)node);"""
        for old, new, _ in copy_fixups.FIXES["F01535"]:
            source = source.replace(old, new)
        self.assertIn("Effects_PluginEffectState parent = {0};", source)
        for axis in "xyz":
            self.assertIn(
                f"parent.position.v{axis} = node->motion.position.v{axis};",
                source,
            )
        self.assertIn("parent.transparency = node->transparency;", source)
        self.assertIn("parent.variant = node->opaque61Bit0;", source)
        self.assertEqual(source.count("&parent"), 3)
        self.assertNotIn("(void*)node", source)

    def test_original_occlusion_queries_use_the_owned_terrain_decoder(self):
        registry = Path(__file__).resolve().with_name("owned.csv")
        with registry.open(encoding="utf-8", newline="") as stream:
            owned = {row["id"]: row for row in csv.DictReader(stream)}
        for fid, name in (
            ("F01767", "Field_IsPositionAboveGround"),
            ("F01768", "Field_IsTerrainSegmentBlocked"),
        ):
            with self.subTest(fid=fid):
                self.assertEqual(owned[fid]["name"], name)
                self.assertEqual(
                    owned[fid]["path"],
                    r"Native\Game\src\Field\TerrainHeight.cpp",
                )
                self.assertEqual(
                    owned[fid]["header"],
                    r"Native\Game\src\Field\TerrainHeight.h",
                )

    def test_palette_pulse_uses_the_native_gimmick_owner(self):
        registry = Path(__file__).resolve().with_name("owned.csv")
        with registry.open(encoding="utf-8", newline="") as stream:
            owned = {row["id"]: row for row in csv.DictReader(stream)}
        self.assertEqual(
            owned["F00979"]["header"],
            r"Native\Game\src\Actors\AppearanceTransition.h",
        )
        source = """((void (*)(void*, void*, int))Actors_UpdateActorPalettePulse)(actor, NULL, 0);
((void (*)(void*, void*, int))Actors_UpdateActorPalettePulse)(attached, NULL, 0);
((void (*)(void*, void*, int))Actors_UpdateActorPalettePulse)(NULL, (Actors_ActorRecord*)object, 0);"""
        for old, new, _ in copy_fixups.FIXES["F01008"]:
            source = source.replace(old, new)
        self.assertEqual(
            source,
            """Actors_UpdateActorPalettePulse(actor, NULL, 0);
Actors_UpdateActorPalettePulse(attached, NULL, 0);
Actors_UpdateActorPalettePulse(NULL, object, 0);""",
        )

    def test_transition_text_uses_original_encoded_controls_not_ascii(self):
        fixes = copy_fixups.FIXES["F01337"]
        replacements = {old: new for old, new, _ in fixes}
        self.assertEqual(
            replacements["currentChar < '\\f'"],
            "currentChar < Core_TextCode_NonPrinting",
        )
        self.assertEqual(
            replacements["currentChar - '\\f'"],
            "currentChar - Core_TextCode_NonPrinting",
        )
        self.assertEqual(
            replacements["currentChar != ' '"],
            "currentChar != Core_TextCode_Space",
        )
        self.assertIn("(u_char)*str++",
                      replacements["u_int currentChar = *str++;"])
        self.assertIn("(u_char)*str++",
                      replacements["u_int opcodeParam = *str++;"])

    def test_attached_draw_requires_a_resident_parent_matrix(self):
        old, new, _ = next(
            fix for fix in copy_fixups.FIXES["F01008"]
            if "entries[actor->instance.attachParent]->boneMatrices" in fix[0]
        )
        self.assertIn("entries[actor->instance.attachParent]->boneMatrices", old)
        self.assertIn("Actors_ResolveAttachmentParentMatrix(actor)", new)
        self.assertIn("if (parent_matrix != NULL)", new)
        self.assertNotIn("entries[", new)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS[
                "Actors_ResolveAttachmentParentMatrix"
            ],
            r"Native\Game\src\Actors\ModelDrawing.h",
        )

    def test_grounded_history_uses_the_actor_native_layout(self):
        registry = Path(__file__).resolve().with_name("owned.csv")
        with registry.open(encoding="utf-8", newline="") as stream:
            owned = {row["id"]: row for row in csv.DictReader(stream)}
        self.assertEqual(
            owned["F00989"]["header"],
            r"Native\Game\src\Actors\MotionRequests.h",
        )
        landing = copy_fixups.FIXES["F00988"][0]
        self.assertIn("Field_GimmickInstance", landing[0])
        self.assertEqual(landing[1], "Actors_SaveActorGroundedTransform(actor)")
        motion = copy_fixups.FIXES["F04130"][0]
        self.assertIn("instance.previousTileX", motion[0])
        self.assertIn("actor->savedPlacement", motion[0])
        self.assertEqual(
            motion[1], "        Actors_SaveActorGroundedTransform(actor);"
        )
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS[
                "Actors_SaveActorGroundedTransform"
            ],
            owned["F00989"]["header"],
        )

    def test_native_file_cards_draw_after_footer_and_before_fade(self):
        for fid, prefix, title in (("F02757", "Menu_Files", 0),
                                   ("F03624", "Title", 1)):
            with self.subTest(fid=fid):
                old, new, _ = copy_fixups.FIXES[fid][0]
                fade = f"    if ({prefix}_fileMenuScreenFade != 0) {{"
                self.assertEqual(old, fade)
                self.assertEqual(
                    new,
                    f"    Save_NativeDrawFileOverlay({title});\n{fade}",
                )
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Save_NativeDrawFileOverlay"],
            r"Native\Game\include\Save\NativeMenu.h",
        )

    def test_trigger_marker_import_owns_the_complete_geometry_definition(self):
        blob = next(
            item for item in copy_fixups.MODULE_DATA_BLOBS
            if item[4] == "Content_battleTriggerMarkerBytes"
        )
        self.assertEqual(
            blob[:4], ("BATTLE/BATTLE.PRG", 0x7FD48, 0x800E8548, 116)
        )
        self.assertNotIn(blob[4], copy_fixups.MODULE_DATA_BLOB_TYPES)
        registry = Path(__file__).resolve().with_name("owned.csv")
        with registry.open(encoding="utf-8", newline="") as stream:
            owned = {row["id"]: row for row in csv.DictReader(stream)}
        for fid in ("F00617", "F00618"):
            with self.subTest(fid=fid):
                self.assertEqual(
                    owned[fid]["path"],
                    r"Native\Game\src\Field\RoomTriggerMarkers.c",
                )
                self.assertEqual(
                    owned[fid]["header"],
                    r"Native\Game\src\Field\RoomTriggerMarkers.h",
                )

    def test_native_resume_hooks_precede_event_and_reset_mutation(self):
        finish = copy_fixups.FIXES["F00543"][-1]
        self.assertIn("Boot_JumpToTitle(NULL)", finish[1])
        self.assertLess(finish[1].index("Save_NativeFinishResume"),
                        finish[1].index("if ((temp_s4"))
        zone = copy_fixups.FIXES["F00533"][0]
        self.assertTrue(zone[1].startswith("    if (!Save_NativeResumePending())"))
        for fid in ("F03423", "F03437", "F03673"):
            with self.subTest(fid=fid):
                reset = copy_fixups.FIXES[fid][-1]
                self.assertIn("{\n    Save_NativeResetSession();", reset[1])
        for name in ("Save_NativeResumePending", "Save_NativeFinishResume",
                     "Save_NativeResetSession"):
            self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[name],
                             r"Native\Game\include\Save\NativeResume.h")

    def test_ai_direction_queries_keep_bounded_mutable_source_cells(self):
        for fid in ("F01728", "F01739", "F01742"):
            with self.subTest(fid=fid):
                fixes = copy_fixups.FIXES[fid]
                self.assertEqual(len(fixes), 1)
                old, new, _ = fixes[0]
                self.assertIn("tile.tileZ", old)
                self.assertIn("Battle_Ai_TerrainRegionCell", new)
                self.assertNotIn("Core_ScratchBase", new)
                self.assertNotIn("TERRAIN_TILE_MASK", new)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Battle_Ai_TerrainRegionCell"],
            r"Native\Game\src\Battle\Ai\FrameUpdate.h",
        )

    def test_room_geometry_names_preserve_index_flags_and_bias_roles(self):
        examples = (
            ("Field::FindRoomGeometryGroup",
             "void* find(int arg0) { int v1; int* a1; return arg0 < v1 ? a1 + arg0 * 16 : NULL; }",
             "void* find(int group_index) { int group_count; int* groups; return group_index < group_count ? groups + group_index * 16 : NULL; }"),
            ("Field::ShowRoomGeometryGroup",
             "int show(int arg0) { int* temp_v0 = find(arg0); *temp_v0 &= ~0x100; }",
             "int show(int group_index) { int* group = find(group_index); *group &= ~0x100; }"),
            ("Field::HideRoomGeometryGroup",
             "int hide(int arg0) { int* temp_v0 = find(arg0); *temp_v0 |= 0x100; }",
             "int hide(int group_index) { int* group = find(group_index); *group |= 0x100; }"),
            ("Field::SwapRoomGeometryGroupFlags",
             "int swap(int arg0, int arg1) { int* temp_s0 = find(arg0); int* temp_a1 = find(arg1); int temp_a0 = *temp_s0; *temp_s0 = *temp_a1; *temp_a1 = temp_a0; }",
             "int swap(int first_group_index, int second_group_index) { int* first_group = find(first_group_index); int* second_group = find(second_group_index); int saved_flags = *first_group; *first_group = *second_group; *second_group = saved_flags; }"),
            ("Field::AddRoomGeometryDepthBias",
             "int bias(int arg0, int arg1) { u_short* temp_v0 = find(arg0); temp_v0[1] = (u_short)(temp_v0[1] + arg1); }",
             "int bias(int group_index, int depth_bias) { u_short* group_halfwords = find(group_index); group_halfwords[1] = (u_short)(group_halfwords[1] + depth_bias); }"),
            ("Field::DisableRoomGeometryGroup",
             "int disable(int arg0) { int* temp_v0 = find(arg0); *temp_v0 |= 0x1000; }",
             "int disable(int group_index) { int* group = find(group_index); *group |= 0x1000; }"),
            ("Field::UnrelatedGeometryCommand",
             "void untouched(int arg0) { int temp_v0 = arg0; }",
             "void untouched(int arg0) { int temp_v0 = arg0; }"),
        )
        for name, source, expected in examples:
            with self.subTest(name=name):
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_lfo_catalogue_preserves_unsigned_byte_source_data_references(self):
        source = "short* Audio_Seq_kLfoWaveforms[];"
        for old, new, _ in copy_fixups.FIXES["G01840"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source, "short* Audio_Seq_kLfoWaveforms[256];")
        self.assertEqual(
            copy_fixups.MODULE_SAMPLE_POINTERS["G01840"],
            "Content_residentLfoSamples",
        )
        blob = next(
            item for item in copy_fixups.MODULE_DATA_BLOBS
            if item[4] == "Content_residentLfoSamples"
        )
        self.assertEqual(blob[:4], ("SLUS_010.40", 0x800, 0x80010000, 0x52000))
        self.assertEqual(
            copy_fixups.MODULE_DATA_BLOB_TYPES[blob[4]], ("short", 2)
        )

    def test_lfo_enables_preflight_before_mutation_and_decode_unsigned_operands(self):
        for fid, flag, member, count in (
            ("F03317", "1", "vibratoShape", 3),
            ("F03321", "SOUND_UPDATE_TREMOLO", "tremoloShape", 3),
            ("F03325", "SOUND_UPDATE_PAN_LFO", "autoPanShape", 2),
        ):
            source = (
                f"    in_pChannel->updateFlags |= {flag};\n"
                f"    in_pChannel->{member} = *in_pChannel->programCounter++;\n"
                f"    wave = Audio_Seq_kLfoWaveforms[in_pChannel->{member}];"
            )
            for old, new, _ in copy_fixups.FIXES[fid]:
                self.assertIn(old, source)
                source = source.replace(old, new)
            self.assertLess(source.index("Audio_Seq_LfoWaveform"),
                            source.index("updateFlags"))
            self.assertIn(f"programCounter += {count}", source)
            self.assertIn("(u_char)*in_pChannel->programCounter++", source)
            self.assertNotIn("Audio_Seq_kLfoWaveforms[", source)

    def test_hud_timer_retirement_does_not_add_a_packed_pointer_view(self):
        ledger = (
            Path(__file__).resolve().parents[2]
            / "Docs"
            / "ledgers"
            / "DATA_LEDGER.csv"
        )
        with ledger.open(encoding="utf-8", newline="") as stream:
            row = next(item for item in csv.DictReader(stream) if item["id"] == "G01934")
        self.assertEqual(row["disposition"], "drop")
        self.assertNotIn("view-of:", row["notes"])

    def test_puzzle_timer_uses_owned_mutable_text_instead_of_pointer_binding(self):
        source = 'char const Battle_puzzleTimerText[] = "00:00:00";'
        for old, new, _ in copy_fixups.FIXES["G00614"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source, 'char Battle_puzzleTimerText[9] = "00:00:00";')
        source = (
            "void Battle_DrawPuzzleTimer(int* value)\n{\n"
            "    char sp10[4];\n"
            "    *(int*)sp10 = *value;\n"
            "Battle_puzzleTimerTextPointer[6 - i * 3] = digit;\n"
            "    Render_DrawCompactText(Battle_puzzleTimerTextPointer, xy, ordering);"
            "\n}"
        )
        for old, new, _ in copy_fixups.FIXES["F01384"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertNotIn("Battle_puzzleTimerTextPointer", source)
        self.assertIn("Render_DrawCompactText(Battle_puzzleTimerText,", source)
        self.assertLess(source.index("Core_Debugf"), source.index("Render_DrawCompactText"))
        self.assertIn("const State_SceneTimer* timer", source)
        self.assertIn("timer->seconds, timer->minutes, timer->hours", source)
        self.assertNotIn("*(int*)sp10", source)
        source = "Battle_DrawPuzzleTimer(&Battle_puzzleTimer);"
        for old, new, _ in copy_fixups.FIXES["F01392"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source, "Battle_DrawPuzzleTimer(&State_sceneTimer);")

    def test_orbit_return_globals_preserve_complete_command_objects(self):
        for gid, name, extent in (
            ("G00130", "Event_kOrbitReturnFirstCommand", 4),
            ("G00131", "Event_kOrbitReturnSecondCommand", 3),
        ):
            source = f"u_char {name};"
            for old, new, _ in copy_fixups.FIXES[gid]:
                self.assertIn(old, source)
                source = source.replace(old, new)
            self.assertEqual(source, f"u_char {name}[{extent}];")
        source = (
            "Event_CommandCameraMotion(&Event_kOrbitReturnFirstCommand, 0);\n"
            "Event_CommandCameraMotion(&Event_kOrbitReturnSecondCommand, 0);"
        )
        for old, new, _ in copy_fixups.FIXES["F01238"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertNotIn("&Event_kOrbitReturn", source)

    def test_camera_track_diagnostics_preserve_duration_and_update_order(self):
        source = "        c->durationFrames = script[2];"
        for old, new, _ in copy_fixups.FIXES["F01221"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertTrue(source.startswith("        c->durationFrames = script[2];"))
        self.assertIn("Core_Debugf", source)
        source = "    if (c->motionMode != 11) {"
        for old, new, _ in copy_fixups.FIXES["F01223"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("Core_Warnf", source)
        self.assertNotIn("return", source)
        self.assertTrue(source.endswith("    if (c->motionMode != 11) {"))

    def test_particle_dispatch_preserves_native_prefix_and_textured_variant(self):
        source = "temp_s1 = Core_Memory_AllocateFromHighEnd(0x10);"
        for old, new, _ in copy_fixups.FIXES["F01865"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("offsetof(Effects_ParticleQuadSet, textureAnimation)", source)
        source = (
            "#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg265_ParticleDispatch2\n"
            "#define VS_PARTICLE_UNTEXTURED\n"
            '#include "Effects/Templates/ParticleDispatch.inl"\n'
            "#undef VS_PARTICLE_UNTEXTURED\n"
        )
        for old, new, _ in copy_fixups.FIXES["F02351"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertNotIn("VS_PARTICLE_UNTEXTURED", source)
        self.assertIn("Effects_Plg265_ParticleDispatch2", source)
        copier = Path(__file__).with_name("copy_to_native.py").read_text()
        templates = copier.split("    def copy_templates(", 1)[1].split(
            "    # ---", 1
        )[0]
        self.assertIn("block = self.apply_fixups(r, block)", templates)

    def test_texture_reset_uses_the_complete_native_state(self):
        source = "\n".join(f"    arg0[{index}] = 0;" for index in range(8))
        for old, new, _ in copy_fixups.FIXES["F01590"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("*state = (Effects_ParticleTextureAnimation){0};", source)
        self.assertNotIn("arg0[", source)

    def test_unbound_texture_diagnostics_do_not_invent_a_frame(self):
        source = "    arg0->textureIndex = textureIndex;"
        for old, new, _ in copy_fixups.FIXES["F01591"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("Core_Debugf", source)
        self.assertIn("request_effect=%u", source)
        self.assertNotIn("Effects_currentInstance->context.effectId", source)
        self.assertNotIn("return", source)
        self.assertTrue(source.endswith("arg0->textureIndex = textureIndex;"))
        source = "        Effects_InitializeParticleTextureAnimation(&state->texture, effect->ringSegments, effect->ringPositions);"
        for old, new, _ in copy_fixups.FIXES["F01861"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("closed ring definition=%u parameter=0x%X", source)
        self.assertNotIn("return", source)

    def test_texture_provenance_does_not_read_secondary_only_instance_metadata(self):
        for fid in ("F01591", "F01592", "F01861"):
            with self.subTest(fid=fid):
                replacement = "\n".join(new for _, new, _ in copy_fixups.FIXES[fid])
                self.assertNotIn("Effects_currentInstance", replacement)
                self.assertIn("Effects_secondaryRequest.effectId", replacement)
                self.assertIn("Effects_actionRequest.effectId", replacement)

    def test_texture_names_preserve_state_and_stream_roles(self):
        examples = (
            ("Effects::ClearParticleTextureState",
             "void clear(int* arg0) { state = arg0; }",
             "void clear(int* storage) { state = storage; }"),
            ("Effects::InitializeParticleTextureAnimation",
             "void init(State* arg0, int textureIndex, int arg2) { arg0->program = table[arg2 + 2]; }",
             "void init(State* animation, int textureIndex, int programIndex) { animation->program = table[programIndex + 2]; }"),
            ("Effects::AdvanceParticleTextureAnimation",
             "void advance(State* arg0) { int w = read(arg0); switch (w & 0xF0000000) {} }",
             "void advance(State* animation) { int command = read(animation); switch (command & 0xF0000000) {} }"),
        )
        for name, source, expected in examples:
            with self.subTest(name=name):
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_shared_bank_retirement_unwinds_before_detaching_and_freeing(self):
        source = "void Actors_UnloadSharedSequence(int arg0) { Content_ReleaseActorSequence(arg0, 1); }"
        for old, new, _ in copy_fixups.FIXES["F00279"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertTrue(source.startswith("int Actors_UnloadSharedSequence"))
        self.assertLess(source.index("Actors_RestartActorAnimationEvents"),
                        source.index("actor->animationHeader = 0"))
        self.assertLess(source.index("actor->blendPose.eventScript = NULL"),
                        source.index("Content_ReleaseActorSequence"))
        self.assertIn("return -1;", source)
        self.assertIn("return result;", source)

    def test_animation_event_unwind_has_an_explicit_nullable_contract(self):
        source = """void Actors_RestartActorAnimationEvents(Actors_ActorRecord* arg0)
{
    u_char** v0 = ((u_char** (*)(Actors_ActorRecord*, u_char, u_short**))Actors_FindActorAnimationHeader)(arg0, arg0->animationId, &sp10);
    u_char* temp_a3 = *v0;
    ((void (*)())Actors_RunAnimationEvents)(arg0, &arg0->blendPose, 2, var_a3);
}
"""
        for old, new, _ in copy_fixups.FIXES["F00998"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertTrue(source.startswith("int Actors_RestartActorAnimationEvents"))
        self.assertLess(source.index("v0 == NULL || sp10 == NULL"),
                        source.index("VS_ONDISC"))
        self.assertIn("return -1;", source)
        self.assertIn("return 0;", source)
        self.assertIn("Core_Errorf", source)
        for fid in ("F00856", "F00992", "F01081"):
            self.assertTrue(any("return -1;" in new or "return 1;" in new or
                                "return;" in new
                                for _, new, _ in copy_fixups.FIXES[fid]))

    def test_unwind_callers_match_pre_normalization_flag_expressions(self):
        for fid, expression in (
            ("F00856", "((*(int*)((char*)actor + 0x5AC)) & 0x800000)"),
            ("F00992", "(*(int*)((u_char*)actor + 0x5AC) & 0x800000)"),
        ):
            guard = f"    if {expression} {{\n"
            source = (("    actor->animationEventFlag = 0;\n"
                       if fid == "F00856" else "") +
                      guard +
                      "        Actors_RestartActorAnimationEvents(actor);\n"
                      "    }\n" +
                      ("    actor->poseOverrideActive = 0;"
                       if fid == "F00856" else ""))
            replacements = [(old, new) for old, new, _ in copy_fixups.FIXES[fid]
                            if "Actors_RestartActorAnimationEvents" in old]
            self.assertEqual(len(replacements), 1)
            old, new = replacements[0]
            self.assertIn(old, source)
            source = source.replace(old, new)
            self.assertIn("Actors_RestartActorAnimationEvents(actor) < 0",
                          source)

    def test_scripted_effect_requires_its_actor_before_starting_playback(self):
        source = """    Effects_StartScriptedActorEffect(
        Core_Memory_ReadUnalignedShort(arg0 + 1), Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 3)));
    return 0;
"""
        for old, new, _ in copy_fixups.FIXES["F01170"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertLess(source.index("(unsigned)actor >= 16"),
                        source.index("Actors_actorRecords[actor]"))
        self.assertLess(source.index("return 1;"),
                        source.index("Effects_StartScriptedActorEffect"))
        self.assertIn("Field_gimmickInstances[actor] == NULL", source)
        self.assertIn("referenceType <= 1 ? Event_ResolveScriptActorReference",
                      source)
        self.assertIn("target->modelId != selector", source)
        self.assertIn("Core_Errorf", source)

    def test_sfx_selection_is_checked_before_pointer_table_lookup(self):
        source = """    case 0xFF000:
        var_s0_2 = Audio_soundState.sfxBankData[Audio_soundState.currentSfxBank - 1];
    case 0xF00000:
        var_s0_2 = Audio_soundState.sfxBankData[2];
"""
        for old, new, _ in copy_fixups.FIXES["F03515"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertLess(source.index("currentSfxBank == 0"),
                        source.index("currentSfxBank - 1]"))
        self.assertIn("Core_Warnf", source)
        self.assertIn("Core_Errorf", source)
        self.assertIn("var_s0_2 == NULL", source)
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS["Core_Errorf"],
                         r"Native\Engine\include\Core\Diagnostics.h")

    def test_empty_stack_top_is_checked_after_remaining_member_updates(self):
        source = """    UpdateRemainingMembers();
    i = 0;

    while ((temp_v0_2 = Field_FindActorAboveGimmick(object, i))) {}
"""
        for old, new, _ in copy_fixups.FIXES["F00836"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        guard = source.index("if (Field_gimmickInstances[temp_s4] == NULL)")
        self.assertLess(source.index("UpdateRemainingMembers"), guard)
        self.assertLess(guard, source.index("while ("))
        self.assertIn("no eligible stack top", source)

    def test_title_arena_reset_invalidates_native_ai_before_new_allocations(self):
        source = """void Boot_InitializeTitleSession(void)
{
    Core_Memory_InitializeHeap((Core_HeapBlockHeader*)0x8010C000, 0xF2000);
    Content_InitializeDiscQueue();
}
"""
        for old, new, _ in copy_fixups.FIXES["F03437"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertLess(source.index("Core_Memory_InitializeHeap"),
                        source.index("Battle_Ai_InvalidateSessionBindings"))
        self.assertLess(source.index("Save_NativeResetSession"),
                        source.index("Core_Memory_InitializeHeap"))
        self.assertLess(source.index("Battle_Ai_InvalidateSessionBindings"),
                        source.index("Content_InitializeDiscQueue"))
        self.assertEqual(source.count("Battle_Ai_InvalidateSessionBindings"), 1)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Battle_Ai_InvalidateSessionBindings"],
            r"Native\Game\src\Battle\Ai\RoomState.h")

    def test_perception_uses_owned_model_prefixes_for_auxiliary_targets(self):
        source = """targetObject = (Battle_Ai_PerceivedObjectView*)Actors_actorRecords[id];
Battle_Ai_PerceivedObjectView* object = (Battle_Ai_PerceivedObjectView*)Actors_actorRecords[candidate];
"""
        for old, new, _ in copy_fixups.FIXES["F01640"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("Battle_actors[id]->model", source)
        self.assertIn("actor->model", source)
        self.assertNotIn("Actors_actorRecords", source)

    def test_optional_actor_wrappers_diagnose_and_return_before_dereferencing(self):
        for key in ("F00946", "F00947", "F00948", "F00949", "F00950", "F00952", "F00953"):
            old, new, _ = copy_fixups.FIXES[key][0]
            self.assertIn("Actors_actorRecords[", old)
            self.assertIn("== NULL", new)
            self.assertIn("Core_Debugf", new)
            self.assertIn("return;", new)

    def test_optional_absolute_slide_checks_actor_before_position_access(self):
        source = """    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];
    sp10.vx = arg1->vx - temp_s1->instance.position.vx;
"""
        for old, new, _ in copy_fixups.FIXES["F00951"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertLess(source.index("if (temp_s1 == NULL)"),
                        source.index("sp10.vx"))
        self.assertLess(source.index("return;"), source.index("sp10.vx"))
        self.assertIn("absolute slide skips absent actor=%d", source)

    def test_bone_endpoints_resolve_owner_before_model_access(self):
        source = """    Actors_ShpModel* model = actor->instance.model;
    MATRIX* matrix;
    if (actor == NULL) {
        actor = (Actors_ActorRecord*)Field_gimmickInstances[actorId];
        if (actor == NULL) {
            return -1;
        }

        model = actor->instance.model;
    }

    matrix = Actors_ActorBoneWorldTransform(actorId, bone, &Actors_boneMatrixScratch);
"""
        for old, new, _ in copy_fixups.FIXES["F00887"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source.count("actor->instance.model"), 1)
        self.assertLess(source.index("return -1;"),
                        source.index("model = actor->instance.model;"))
        self.assertLess(source.index("model = actor->instance.model;"),
                        source.index("matrix = Actors_ActorBoneWorldTransform"))

    def test_room_lifetimes_revoke_diagnostic_readiness(self):
        for key, header in (
            ("F00543", "void Field_FinishRoomLoad(int arg0)\n{"),
            ("F00545", "void Field_UnloadCurrentRoom(void)\n{"),
        ):
            source = header + "\n    OriginalRoomWork();\n}"
            for old, new, _ in copy_fixups.FIXES[key]:
                source = source.replace(old, new)
            self.assertLess(source.index("Boot_ResetGameplayInspection();"),
                            source.index("OriginalRoomWork();"))

    def test_gimmick_palette_matches_ledger_element_width_without_growth(self):
        source = "u_char pulsedPalette[32];"
        for old, new, _ in copy_fixups.TYPE_FIXES["T00255"]:
            source = source.replace(old, new)
        self.assertEqual(source, "u_short pulsedPalette[16];")

    def test_room_rendering_dispatches_typed_appearance_owners(self):
        source = """((void (*)(void*))Actors_UpdateActorAppearanceTransition)(actor);
((void (*)(void*))Actors_UpdateActorAppearanceTransition)(attached);
((void (*)(void*))Actors_UpdateActorAppearanceTransition)((Actors_ActorRecord*)object);
"""
        for old, new, _ in copy_fixups.FIXES["F01008"]:
            source = source.replace(old, new)
        self.assertIn("Actors_UpdateActorAppearanceTransition(actor);", source)
        self.assertIn("Actors_UpdateActorAppearanceTransition(attached);", source)
        self.assertIn("Field_UpdateGimmickAppearanceTransition(object);", source)
        self.assertNotIn("void (*)(void*)", source)

    def test_effect_register_consumers_preserve_original_operand_order(self):
        for function_id, function_name in (
            ("F01531", "Effects::SetEffectScriptRegister"),
            ("F01532", "Effects::BranchIfEffectRegisterEqual"),
        ):
            old, replacement, _ = copy_fixups.FIXES[function_id][0]
            source = old.replace(old, replacement)
            for row, pattern, value, _ in copy_fixups.REGEX_FIXES:
                if re.search(row, function_name):
                    source = re.sub(pattern, value, source)
            self.assertIn("registerIndex = Effects_ReadScriptByte(instance)", source)
            self.assertIn("value = Effects_ReadScriptHalfword(instance)", source)
            self.assertLess(source.index("registerIndex ="), source.index("value ="))
            self.assertNotIn("arg0", source)
            self.assertEqual(source.count("Effects_ReadScript"), 2)
        self.assertIn("(u_char)instance.registers", source.replace("->", "."))

    def test_score_preview_requests_use_initialized_native_record_storage(self):
        source = """Content_ActorAssetRequest slot;
slot.destination = Menu_Score_zudBuffer + 0x1900;
slot.destination = Menu_Score_zudBuffer + 0x1900;
slot.destination = Menu_Score_zudBuffer + 0x1EE8;
"""
        for old, new, _ in copy_fixups.FIXES["F02840"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
            if re.search(row, "Menu::Score::LoadEnemyModelParts"):
                source = re.sub(pattern, replacement, source)
        self.assertIn("Content_ActorAssetRequest request = {0};", source)
        self.assertEqual(source.count("sizeof(Actors_ActorRecord)"), 3)
        self.assertIn("sizeof(Actors_WeaponModel)", source)
        self.assertNotIn("0x1900", source)
        self.assertNotIn("0x1EE8", source)
        allocation = """Core_Memory_AllocateFromLowEnd(0x3200);
Core_Memory_AllocateFromLowEnd(0x24D0);
"""
        for old, new, _ in copy_fixups.FIXES["F02839"]:
            self.assertIn(old, allocation)
            allocation = allocation.replace(old, new)
        self.assertIn("2 * sizeof(Actors_ActorRecord)", allocation)
        self.assertIn("2 * sizeof(Actors_WeaponModel)", allocation)

    def test_geometry_copy_preserves_offsets_and_verified_parameter_names(self):
        source = """void* Content_CopyModelGeometry(int* arg0, void* arg1, int arg2)
{
    if (arg0 == NULL) {
        arg0 = Core_Memory_AllocateFromLowEnd(arg2);
    }
    memcpy(arg0, arg1, arg2);
    arg0[12] += (u_long)arg0;
    arg0[13] += (u_long)arg0;
    arg0[14] += (u_long)arg0;
    arg0[15] += (u_long)arg0;
    return arg0;
}
"""
        for old, new, _ in copy_fixups.FIXES["F00786"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
            if re.search(row, "Content::CopyModelGeometry"):
                source = re.sub(pattern, replacement, source)
        self.assertIn("int* destination, void* source, int byteCount", source)
        self.assertIn("memcpy(destination, source, byteCount);", source)
        self.assertNotIn("(u_long)", source)
        self.assertNotRegex(source, r"\barg\d+\b")

    def test_gimmick_copy_uses_only_the_native_owned_runtime_prefix(self):
        source = """memset(s1, 0, 0x178);
Core_Memory_CopyAlignedWords(s1, Field_gimmickInstances[arg0->dataAddress], 0x178);
"""
        for old, new, _ in copy_fixups.FIXES["F00782"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(
            source.count("offsetof(Field_GimmickInstance, reserved178)"), 2
        )
        self.assertNotIn("sizeof(Field_GimmickInstance)", source)
        self.assertNotIn("0x178", source)

    def test_score_viewer_banner_uses_native_frame_pointers(self):
        source = """    void** scratch = (void**)Core_ScratchAddr(0);

    line = scratch[0];
AddPrim(*((void**)Core_ScratchAddr(1)) + 0x1C, line++);
    scratch = (void**)Core_ScratchAddr(0);

    AddPrim(scratch[1] + 0x1C, poly++);

    scratch[0] = (void*)poly;
"""
        for old, new, _ in copy_fixups.FIXES["F02810"]:
            self.assertEqual(source.count(old), 1)
            source = source.replace(old, new)
        self.assertEqual(source.count("Render_frameScratch.orderingTable + 0x1C"), 2)
        self.assertIn("Render_frameScratch.primitiveCursor = poly;", source)
        self.assertNotIn("Core_ScratchAddr", source)

    def test_score_scroll_links_keep_the_original_byte_displacement(self):
        source = """before = Render_frameScratch.overlayOrderingTable + 8;
*before = ((u_long)prim << 8) >> 8;
"""
        for old, new, _ in copy_fixups.FIXES["F02799"]:
            self.assertEqual(source.count(old), 1)
            source = source.replace(old, new)
        self.assertIn("*before = LegacyGpu_Handle(prim);", source)
        self.assertIn("Render_frameScratch.overlayOrderingTable + 8;", source)
        self.assertNotIn("((u_long)prim", source)

    def test_score_number_buffers_include_all_digits_and_terminators(self):
        cases = (
            ("F02816", 'char digits[2];\nsprintf(digits, "%02d", titleNo);',
             'char digits[sizeof "00"];', '"%02d"'),
            ("F02817", 'char digits[2];\nsprintf(digits, "%03d", enemyNo);',
             'char digits[sizeof "000"];', '"%03d"'),
            ("F02820", 'char sp60[12];\nsprintf(sp60, "NO.   %03d/%03d", selected + 1, 78);',
             'char sp60[sizeof "NO.   000/000"];', '"NO.   %03d/%03d"'),
        )
        for function, source, declaration, format_string in cases:
            with self.subTest(function=function):
                if function == "F02816":
                    source += """
poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);
AddPrim(*(void**)((uintptr_t)Core_ScratchBase() + 0x008), poly++);
*(void**)((uintptr_t)Core_ScratchBase() + 0x000) = poly;
"""
                elif function == "F02817":
                    source += """
poly = *(void**)Core_ScratchAddr(0);
AddPrim(*(void**)Core_ScratchAddr(2), poly++);
*(void**)Core_ScratchAddr(0) = poly;
"""
                for old, new, _ in copy_fixups.FIXES[function]:
                    self.assertEqual(source.count(old), 1)
                    source = source.replace(old, new)
                self.assertIn(declaration, source)
                self.assertIn(format_string, source)
                if function != "F02820":
                    self.assertIn("AddPrim(Render_frameScratch.overlayOrderingTable, poly++);", source)
                    self.assertNotIn("Core_Scratch", source)

    def test_npc_area_filter_keeps_geometry_without_creating_player_ui(self):
        source = """    Battle_actionState->areaOrigin = Battle_actionState->savedAreaCenter;
    ((void (*)(void*))Battle_SetSecondarySphereVolume)(&Battle_actionState->areaShape);
    kept = 0;
    Battle_IsPointInTargetVolume(&Battle_actionState->areaShape, position);
"""
        for old, new, _ in copy_fixups.FIXES["F00103"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("if (Battle_actionState->isPlayerAction != 0)", source)
        self.assertIn("Battle_SetSecondarySphereVolume", source)
        self.assertIn("    kept = 0;\n    Battle_IsPointInTargetVolume", source)
        self.assertNotIn("Battle_InitSphere", source)
        self.assertNotIn("return;", source)

    def test_native_target_capacity_is_not_a_ps1_allocation_or_visible_limit(self):
        root = Path(__file__).resolve().parents[2]
        action = (root / "Native/Game/include/Battle/Action.h").read_text()
        self.assertIn("Battle_TargetPreview previews[255];", action)
        self.assertIn("Battle_HitResult targets[255];", action)
        boot = (root / "Native/Game/src/Boot/GameLoop.c").read_text()
        self.assertIn("AllocateFromHighEnd(sizeof *Battle_actionState)", boot)
        self.assertNotIn("0x2C10", boot)
        targeting = (root / "Native/Game/src/Battle/Targeting.c").read_text()
        self.assertIn("sizeof Battle_actionState->selection", targeting)
        self.assertNotIn("0x84C", targeting)
        self.assertIn("Battle_ValidateTargetPreviewIndex(count)", targeting)
        self.assertIn("Battle_MapTargetPageIndex(", targeting)
        self.assertIn("Battle_HandleTargetPageButtons()", targeting)
        self.assertNotIn("sphere-cursor", targeting)

    def test_ai_planning_owns_the_full_native_action_result(self):
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        self.assertEqual(
            owned["F01718"]["path"],
            r"Native\Game\src\Battle\Ai\PlanningWorkspace.cpp",
        )
        source = (root / "Native/Game/src/Battle/Ai/ActionPlanning.c").read_text()
        self.assertNotIn("int Battle_Ai_RebuildActorActionPlans(", source)
        for offset in ("0x1ECC", "0x9CC", "0xD8C", "0x1D8C"):
            self.assertNotIn(offset, source)
        spawning = (root / "Native/Game/src/Actors/ActorLoading.c").read_text()
        self.assertNotIn("spawn137", spawning)

    def test_particle_attraction_uses_native_fields_after_pointer_widening(self):
        source = """damping = ((Effects_ParticleAttractionView*)node)->attraction;
direction.vx = ((Effects_ParticleAttractionView*)node)->targetX - (state->x >> 12);
direction.vy = ((Effects_ParticleAttractionView*)node)->targetY - (state->y >> 12);
direction.vz = ((Effects_ParticleAttractionView*)node)->targetZ - (state->z >> 12);
"""
        for old, new, _ in copy_fixups.FIXES["F01554"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("damping = node->attraction;", source)
        for index, axis in enumerate("xyz"):
            self.assertIn(
                f"direction.v{axis} = node->attractionTarget[{index}] - (state->{axis} >> 12);",
                source,
            )
        self.assertNotIn("Effects_ParticleAttractionView", source)

    def test_door_queries_keep_owned_native_pointer_storage(self):
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        for item in ("T00232", "G00330", "F00579"):
            with self.subTest(item=item):
                self.assertIn(item, owned)
                self.assertEqual(owned[item]["header"],
                                 r"Native\Game\src\Field\DoorMatches.h")
        source = (root / "Native/Game/src/Field/Doors.c").read_text()
        caller = (root / "Native/Game/src/Field/Interactions.c").read_text()
        self.assertIn('"Field/DoorMatches.h"', caller)
        self.assertNotIn("Field_DoorMatchList Field_matchingDoorRecords;", source)
        self.assertNotIn("Field_TransitionDoorsAtTile(int", source)
        owned_header = (root / "Native/Game/src/Field/DoorMatches.h").read_text()
        self.assertIn("Field_MpdDoorRecord** records;", owned_header)
        self.assertNotIn("records[0]", owned_header)

    def test_weapon_naming_draw_links_use_native_handles(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Rename.c").read_text()
        for function, variable in (("DrawFrame", "prim"),
                                   ("DrawSelectionBox", "temp_t0")):
            with self.subTest(function=function):
                body = source.split(f"void Menu_Rename_{function}(", 1)[1]
                body = body.split("\n}\n", 1)[0]
                self.assertNotIn(f"((u_long){variable} << 8) >> 8", body)
                self.assertIn(f"LegacyGpu_Handle({variable})", body)

    def test_workshop_choices_use_native_pointer_arrays(self):
        root = Path(__file__).resolve().parents[2]
        root = root / "Native" / "Game" / "src" / "Menu" / "Setup"
        selectors = (
            ("Assembly.c", "SelectAssemblyBlade", "menuText", 17),
            ("Assembly.c", "SelectAssemblyGrip", "menuText", 17),
            ("Assembly.c", "SelectGem", "menuText", 49),
            ("Combination.c", "SelectCombinationBlade", "menuText", 17),
            ("Combination.c", "SelectCombinationShield", "menuText", 9),
            ("Combination.c", "SelectCombinationArmor", "sp10", 17),
        )
        for filename, function, variable, rows in selectors:
            with self.subTest(function=function):
                source = (root / filename).read_text()
                body = source.split(f"int Menu_Setup_{function}(", 1)[1]
                body = body.split("\n}\n", 1)[0]
                self.assertTrue(f"char* {variable}[2 * {rows}];" in body,
                                f"{function} needs aligned native pointer storage")
                self.assertNotIn(f"char** {variable};", body)
                self.assertNotRegex(body, rf"{variable}\s*=\s*.*0x(?:6A4|384|1324)")
                self.assertIn("Menu_Setup_BuildItemList(", body)
                self.assertIn("Core_Memory_FreeFromHighEnd(", body)

    def test_loot_staging_uses_last_slots_and_preflights_composites(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Loot/LootInventory.c").read_text()
        for fid in ("F02846", "F02847", "F02848", "F02849", "F02850", "F02851", "F02852"):
            for old, new, _ in copy_fixups.FIXES[fid]:
                self.assertIn(new, source)
                self.assertNotIn(old, source)
        self.assertIn("Items_ItemCategory_Grip, 1", source)
        self.assertIn("Items_ItemCategory_Gem, gems", source)

    def test_loot_overflow_never_appends_a_failed_staging_index(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Loot/LootInventory.c").read_text()
        stage = source.split("void Menu_Loot_StageDropRecord(", 1)[1]
        stage = stage.split("// decomp:", 1)[0]
        self.assertNotIn("Menu_Loot_AppendLootRow(", stage)
        self.assertEqual(stage.count("Menu_Loot_AppendStagedDrop("), 8)
        append = source.split("void Menu_Loot_AppendStagedDrop(", 1)[1]
        append = append.split("int Menu_Loot_AdvanceStagedInventory(", 1)[0]
        self.assertLess(append.index("if (index == 0)"), append.index("Menu_Loot_AppendLootRow("))
        self.assertIn("Menu_Loot_paging.deferred = 1;", append)

    def test_loot_pages_share_row_capacity_and_reset_before_source_release(self):
        root = Path(__file__).resolve().parents[2]
        screen = (root / "Native/Game/src/Menu/Loot/LootScreen.c").read_text()
        self.assertNotIn("160 * sizeof", screen)
        self.assertNotIn("sizeof *Menu_Loot_lootList * 160", screen)
        self.assertEqual(screen.count("kMenuLootRowCapacity"), 6)
        selection = screen.split("int Menu_Loot_UpdateLootSelection(", 1)[1]
        self.assertIn("} else if (Menu_Loot_AdvanceStagedInventory()) {", selection)
        cleanup = (root / "Native/Game/src/Items/Loot.c").read_text()
        cleanup = cleanup.split("void Items_ClearRoomLootList(", 1)[1]
        self.assertLess(cleanup.index("Menu_Loot_ResetPaging();"),
                        cleanup.index("Core_Memory_FreeFromHighEnd("))

    def test_misc_inventory_write_checks_capacity_before_mutation(self):
        old, new, _ = copy_fixups.FIXES["F02511"][0]
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native" / "Game" / "src" / "Items" / "Inventory.c").read_text()
        self.assertIn(new, source)
        self.assertNotIn(old, source)
        self.assertIn("write != 0", new)
        self.assertIn("Items_Inventory_CopyItem(Items_ItemCategory_Misc,", new)
        self.assertIn("targetInventory, sourceItemIndex, sourceInventory) == 0", new)
        self.assertLess(source.index(new), source.index("int space_left ="))

    def test_particle_cone_preserves_original_zero_height_quotient(self):
        _, new, _ = copy_fixups.FIXES["F01515"][1]
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Effects/Particles.c").read_text()
        self.assertTrue(new in source, "cone fix missing from generated particle emitter")
        self.assertIn("scratch->vector.vx = scratch->vector.vz = -1;", new)
        self.assertIn("0x800D34BC/0x800D34F8", new)
        self.assertEqual(new.count("/ scratch->extent.vy;"), 2)
        self.assertNotIn("Core_Random_Rand", new)

    def test_particle_ribbon_preserves_original_zero_divisor_remainder(self):
        old, new, _ = copy_fixups.FIXES["F01867"][0]
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Effects/Templates/ParticleRender.inl").read_text()
        self.assertIn(new, source)
        self.assertNotIn(old, source)
        self.assertEqual(new.count("Core_Random_Rand()"), 1)
        self.assertIn("if (width != 0) {\n                sampleJitter %= width;", new)
        self.assertIn("0x800FAA74", new)

    def test_ashley_spawn_initializes_materials_for_empty_equipment_slots(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Actors/Ashley.c").read_text()
        spawn = source.split("void Actors_SpawnAshleyWithEquipment(void)", 1)[1]
        spawn = spawn.split("\n}\n", 1)[0]
        self.assertIn("int bladeMaterial = 0;", spawn)
        self.assertIn("int shieldMaterial = 0;", spawn)

    def test_misc_shortcut_label_is_encoded_before_reset_capture(self):
        old, new, _ = copy_fixups.FIXES["F02553"][0]
        declaration = 'static char Menu_Inventory_miscItemShortcutMisc[] = ' + old + ';'
        encoded = declaration.replace(old, new)
        self.assertIn(r'"\x16\x12\x1C\x0C\xE7\x00\x00\xF8"', encoded)
        self.assertNotIn("MISC", encoded)
        self.assertNotIn("|0xF8|", encoded)

    def test_generated_label_and_reload_initializer_both_terminate(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Inventory/Inventory.c").read_text()
        label = r'"\x16\x12\x1C\x0C\xE7\x00\x00\xF8"'
        self.assertIn("static char Menu_Inventory_miscItemShortcutMisc[] = " + label, source)
        self.assertIn("static const char initial[] = " + label, source)
        self.assertNotIn(r'"MISC\00000|0xF8|"', source)

    def test_title_settings_retains_original_defaults_in_shared_helper(self):
        old, new, _ = copy_fixups.FIXES["F03669"][0]
        self.assertIn("Input_settings.menuFlags = 0x2D8;", old)
        self.assertEqual(new.strip(), "Boot_InitializePlayerSettings();")
        self.assertIn("Boot_InitializePlayerSettings", copy_fixups.NATIVE_HELPER_HEADERS)

    def test_misc_shortcut_reserves_all_native_text_pointers(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Inventory/Inventory.c").read_text()
        self.assertIn(
            "Core_Memory_AllocateFromHighEnd(0x1900 + 64 * 2 * sizeof(*menuText))",
            source,
        )
        self.assertIn("menuText = buf + 0x1900;", source)
        self.assertNotIn("Core_Memory_AllocateFromHighEnd(0x1B00)", source)

    def test_plugin_table_never_binds_the_development_dispatcher(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Effects/PurgeTables.c").read_text()
        table = source.split("Effects_EffectRenderer Effects_effectEntryPoints[] = {", 1)[1]
        table = table.split("};", 1)[0]
        self.assertNotIn("Effects_InvokeAllPluginUpdates", table)
        self.assertIn("Effects_Plg198_Ring", table)

    def test_rank_menu_preserves_native_packet_link(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Magic.c").read_text()
        menu = source.split("int Menu_Magic_UpdateWarlockMenu(", 1)[1]
        menu = menu.split("int Menu_Magic_UpdateShamanMenu(", 1)[0]
        self.assertIn("*temp_s6 = LegacyGpu_Handle(prim);", menu)
        self.assertNotIn("*temp_s6 = ((u_int)prim << 8) >> 8;", menu)

    def test_break_art_menu_reserves_navigation_and_matching_reset_storage(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/BreakArts.c").read_text()
        self.assertIn("static u_short Menu_BreakArts_artListActionIds[5];", source)
        reset = source.split("void Menu_BreakArts_ResetMenu1Statics(", 1)[1]
        self.assertIn("static const u_short initial[5];", reset)
        self.assertNotIn("static const u_short initial[4];", reset)

    def test_encounter_trace_uses_action_actor_not_targeting_mode(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Boot/Encounter.cpp").read_text()
        inspect = source.split("void InspectTargeting(", 1)[1].split("\n}", 1)[0]
        self.assertIn(
            "state.source_actor = action.selection.primaryTarget.target.targetActor;",
            inspect,
        )
        self.assertNotIn("state.source_actor = action.sourceActorId;", inspect)

    def test_secondary_body_uses_the_live_zone_character_table(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Actors/ActorLoading.c").read_text()
        spawn = source.split("Battle_Actor* Actors_SpawnRoomEnemy(", 1)[1]
        spawn = spawn.split("\n}\n", 1)[0]
        self.assertIn(
            "object.modelId = (u_char)Field_zoneContext.zndEnemies[zudIndex].recordBytes[2] + 1;",
            spawn,
        )
        self.assertNotIn("Actors_zoneCharacterInitData[", spawn)

    def test_preview_weapon_alias_uses_real_model_slots(self):
        root = Path(__file__).resolve().parents[2]
        preview = (root / "Native/Game/src/Menu/Framework/ActorPreview.c").read_text()
        reset = preview.split("void Menu_Framework_ResetActorPreviewPose(", 1)[1]
        reset = reset.split("void Menu_Framework_ReleaseActorPreview(", 1)[0]
        self.assertIn("Actors_weaponModels[i + 2] != NULL", reset)
        self.assertIn("Actors_weaponModels[i + 2]->reserved08Bit4 = 1;", reset)
        self.assertNotIn("Battle_unusedGlobal1", reset)
        inventory = (root / "Native/Game/src/Menu/Inventory/Inventory.c").read_text()
        self.assertIn("Actors_weaponModels[i + 2]->reserved11 = 0;", inventory)
        self.assertNotIn("Battle_unusedGlobal1", inventory)
        self.assertIn(
            "Actors_weaponModels[3] != NULL && Menu_Inventory_IsTwoHandedWeapon(category)",
            inventory,
        )
        self.assertNotIn("Battle_unusedGlobal2", inventory)

    def test_equipment_slot_storage_is_owned_and_native_sized(self):
        root = Path(__file__).resolve().parents[2]
        allocation = (root / "Native/Game/src/Actors/ActorAllocation.h").read_text()
        slot = (root / "Native/Game/src/Actors/ActorModelSlot.h").read_text()
        self.assertIn('#include "Actors/ActorModelSlot.h"', allocation)
        self.assertNotIn("storage[0x5E8]", allocation)
        self.assertIn("Actors_WeaponModel weapon;", slot)
        registry = (root / "tools/port/owned.csv").read_text()
        self.assertIn("T00633,type,Actors_ActorModelSlot,", registry)

    def test_native_audio_preserves_optional_layer_guard_and_connects_sfx(self):
        fixes = copy_fixups.FIXES["F03192"]
        self.assertEqual(len(fixes), 4)
        root = Path(__file__).resolve().parents[2]
        mixer = (root / "Native/Engine/src/Audio/Mixer/VoiceMixer.c").read_text()
        self.assertEqual(mixer.count("Audio_Native_ConfigureChannel("), 3)
        self.assertNotIn("Audio_Driver_WriteDirtyVoiceParameters(", mixer)
        self.assertNotIn("Audio_Driver_WriteVoiceParameters(", mixer)
        self.assertIn("Audio_Native_Envelope(i)", mixer)
        self.assertIn("Audio_Seq_secondaryMusicConfig != NULL ?", mixer)
        sequencer = (root / "Native/Engine/src/Audio/Seq/Sequencer.c").read_text()
        self.assertEqual(sequencer.count("Audio_Native_Tick();"), 1)


if __name__ == "__main__":
    unittest.main()
