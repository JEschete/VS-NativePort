"""Regressions for native source adaptations, without game assets."""

import csv
import re
import runpy
import unittest
from pathlib import Path

import audit_assembly_gaps
import copy_fixups
import native_views


class EncounterFixupTests(unittest.TestCase):
    def test_effect_origin_aliases_the_primary_request_position(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\DATA_LEDGER.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            row = next(item for item in csv.DictReader(stream)
                       if item["id"] == "G00532")
        self.assertIn("view-of:G00529+0xE0", row["notes"])
        self.assertIn("Effects_ActionEffectRequest.position", row["notes"])
        self.assertEqual(row["size"], "8")
        self.assertEqual(row["type"], "SVECTOR[1]")
        header = (root / r"Native\Game\src\Effects\Runtime.h").read_text(
            encoding="utf-8"
        )
        source = (root / r"Native\Game\src\Effects\Runtime.c").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "#define Effects_effectOrigin ((SVECTOR*)(&Effects_actionRequest.position))",
            header,
        )
        self.assertNotRegex(
            source, r"SVECTOR\s+Effects_effectOrigin\s*\["
        )

    def test_texture_binding_receives_its_payload_extent(self):
        fixups = copy_fixups.FIXES["F01626"]
        self.assertTrue(any(
            "Effects_BindEffectTextureTables(arg0->blocks," in new
            and "sizeof(arg0->meta)" in new
            for _, new, _ in fixups
        ))
        self.assertTrue(any(
            "Effects_BindEffectAuxiliarySoundBank(" in new
            for _, new, _ in fixups
        ))
        self.assertEqual(
            self.owned_items()["F01606"]["path"],
            r"Native\Game\src\Effects\TextureBindings.cpp",
        )

    def test_retired_execution_hooks_have_no_production_consumers(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            catalog = {row["id"]: row for row in csv.DictReader(stream)}
        for identity in ("F03575", "F00766"):
            row = catalog[identity]
            self.assertEqual(row["copy"], "no")
            self.assertIn("retired-", row["notes"])
            name = row["final_name"].replace("::", "_")
            for path in (root / "Native").rglob("*"):
                if path.suffix not in (".c", ".cpp", ".h", ".hpp"):
                    continue
                if "Tests" in path.parts:
                    continue
                text = audit_assembly_gaps.mask_literals(
                    path.read_text(encoding="utf-8")
                )
                self.assertNotRegex(text, rf"\b{re.escape(name)}\b", str(path))

    def test_scripted_navigation_uses_full_native_records_and_explicit_aliases(self):
        root = Path(__file__).resolve().parents[2]
        owned = self.owned_items()
        for identity, address in (("F01805", 0x800E527C), ("F01806", 0x800E5308)):
            self.assertEqual(
                owned[identity]["path"],
                r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
            )
            self.assertIn(
                ("BATTLE/BATTLE.PRG", address, identity),
                copy_fixups.MODULE_FUNCTION_ENTRIES,
            )
        self.assertEqual(
            native_views.NATIVE_FIELD_ALIASES["T00532"]["scriptedTargetXZ"], 0x160
        )
        source = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("std::construct_at(storage)", source)
        self.assertIn("Core_Memory_AllocateFromHighEnd(sizeof(Battle_Ai_ActorAiState))", source)
        self.assertIn("auto* destination = state;", source)
        self.assertIn("destination->scriptedTargetXZ = target;", source)
        self.assertIn("std::bit_cast<std::int32_t>(squared)", source)
        self.assertIn("Battle_Ai_FreeActorNavigationOverride(route->actorId)", source)
        self.assertIn("Battle_Ai_SaveMovementRandomState();", source)
        self.assertNotIn("actor[3]", source)

    def test_route_availability_keeps_original_shift_mutation_and_native_bounds(self):
        root = Path(__file__).resolve().parents[2]
        self.assertEqual(
            self.owned_items()["F01694"]["path"],
            r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
        )
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x800DC638, "F01694"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        source = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("kOriginalPlatformCapacity = 13", source)
        self.assertIn("first_bit = 1u << (first_bit & kVariableShiftMask)", source)
        self.assertIn("second_bit = 1u << (second_bit & kVariableShiftMask)", source)
        self.assertIn("auxiliary == Field_roomData.section6", source)
        self.assertIn("Field_roomData.header.section6Length >= kRouteAuxiliaryBytes", source)
        self.assertIn("first route word depends on an original stack-register value", source)
        self.assertIn("std::memcpy(location, &word, sizeof word)", source)

    def test_gimmick_physics_occupancy_calls_cross_the_explicit_native_view_boundary(self):
        old, new, reason = copy_fixups.FIXES["F01013"][0]
        self.assertEqual(old, "Battle_Ai_AddAiTileOccupant(object);")
        self.assertIn("(const Field_GimmickInstance*)object", new)
        self.assertIn("native owning Gimmick", reason)

    def test_occupancy_entries_use_typed_native_grids_and_shared_contact_helper(self):
        root = Path(__file__).resolve().parents[2]
        owned = self.owned_items()
        for identity in ("F01826", "F01827"):
            self.assertEqual(
                owned[identity]["path"],
                r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
            )
        self.assertEqual(
            owned["F01836"]["path"], r"Native\Game\src\Battle\Ai\ActorCleanup.cpp"
        )
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x800E6EB0, "F01836"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        grid = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("const Field_GimmickInstance* gimmick", grid)
        self.assertIn("(layer >> 2)", grid)
        self.assertIn("offset > kOccupancyMapBytes - kOccupancyCellBytes", grid)
        self.assertIn("offset % kOccupancyCellBytes != 0", grid)
        self.assertIn("static_cast<int>(cell) + delta", grid)
        self.assertEqual(grid.count("ChangeGimmickOccupancy(gimmick,"), 2)
        cleanup = (root / r"Native\Game\src\Battle\Ai\ActorCleanup.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("kContactCommands = 0x4800", cleanup)
        self.assertIn("if (model->instance.supportLayer != 0) return;", cleanup)
        self.assertIn(
            "Battle_Ai_ClearAiContactOccupancy(static_cast<int>(contact))", cleanup
        )

    def test_ashley_alliance_uses_native_callback_storage_without_shifting_views(self):
        self.assertEqual(
            native_views.NATIVE_FIELD_ALIASES["T00373"],
            {"allianceReactionCount": 0x126, "temporaryAshleyAlliance": 0x13},
        )
        self.assertEqual(
            native_views.NATIVE_FIELD_ALIASES["T00392"]["savedDecisionCallback"],
            0x6C,
        )
        self.assertNotIn(0x6C, native_views.AI_POINTERS)
        reaction = native_views.rebase(
            "T00373",
            "    unsigned char usesRoomActionDelay;\n"
            "    char reserved89[164];\n",
            [],
        )
        self.assertIn("u_char temporaryAshleyAlliance;", reaction)
        self.assertIn("char reserved89Prefix[0x9D];", reaction)
        self.assertIn("u_char allianceReactionCount;", reaction)
        settings = native_views.rebase("T00392", "    char reserved6C[40];\n", [])
        self.assertIn("char reserved6C[40];", settings)
        self.assertIn(
            "int (*savedDecisionCallback)(Battle_Ai_ActorAiState*);", settings
        )
        with self.assertRaisesRegex(ValueError, "reaction counter storage"):
            native_views.rebase("T00373", "unsigned char usesRoomActionDelay;\n", [])
        with self.assertRaisesRegex(ValueError, "saved decision callback slot"):
            native_views.rebase("T00392", "char reserved6C[4];\n", [])

    def test_ashley_alliance_ownership_and_source_integer_comparison(self):
        root = Path(__file__).resolve().parents[2]
        owned = self.owned_items()
        for identity in ("F01681", "F01682"):
            self.assertEqual(
                owned[identity]["path"], r"Native\Game\src\Battle\Ai\Relations.c"
            )
        source = (root / r"Native\Game\src\Battle\Ai\Relations.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("(unsigned)state->allianceReactionCount + 1", source)
        self.assertIn("state->allianceReactionCount = (u_char)reactions;", source)
        self.assertIn("if (reactions < kRequiredReactions) return;", source)
        self.assertIn("Battle_Ai_UpdateEngagementApproachBehavior", source)
        self.assertIn("settings->savedDecisionCallback = NULL;", source)
        self.assertNotIn("VS_UNPORTED", source)

    def test_plg074_reuses_identical_code_without_merging_overlay_state(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as handle:
            catalog = {row["id"]: row for row in csv.DictReader(handle)}
        addresses = (
            0x800F98F8, 0x800FA7F0, 0x800FB414, 0x800FBBB0, 0x800FBDC8,
            0x800FC28C, 0x800FC404, 0x800FC590, 0x800FCFEC, 0x800FD534,
        )
        for offset, address in enumerate(addresses):
            identity = f"F{1986 + offset:05}"
            with self.subTest(id=identity):
                row = catalog[identity]
                self.assertIn(
                    ("EFFECT/PLG074.BIN", address, identity),
                    copy_fixups.MODULE_FUNCTION_ENTRIES,
                )
                self.assertEqual(row["copied"], "x")
                if offset in (2, 8):
                    self.assertEqual(
                        audit_assembly_gaps.canonical_id(row, catalog), identity
                    )
                    self.assertEqual(
                        audit_assembly_gaps.template_status(row)[0],
                        "template-instance",
                    )
                else:
                    self.assertEqual(
                        audit_assembly_gaps.canonical_id(row, catalog),
                        f"F{1975 + offset:05}",
                    )
        cursor = next(
            blob for blob in copy_fixups.MODULE_DATA_BLOBS
            if blob[4] == "Content_plg074TrailEmissionCounter"
        )
        self.assertEqual(cursor[:4], ("EFFECT/PLG074.BIN", 0x577C, 0x800FEF7C, 4))
        source = (root / r"Native\Game\src\Effects\Templates\RadialSurge.c").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "#define VS_TRAIL_CORNER_STATE Content_plg074TrailEmissionCounter[0]",
            source,
        )
        self.assertEqual(
            source.count(
                "#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg074_GenerateTrailOrigins"
            ),
            2,
        )
        self.assertNotIn("Effects_Plg073_trailEmissionCounter", source)

    def test_room_packet_linking_reuses_shared_depth_resolution_and_native_handles(self):
        self.assertEqual(
            self.owned_items()["F00752"]["path"],
            r"Native\Engine\src\Render\RoomDraw.c",
        )
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x800980F8, "F00752"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\RoomDraw.c").read_text(
            encoding="utf-8"
        )
        self.assertEqual(source.count("Render_LinkRoomPolygonByDepth("), 2)
        self.assertIn("ResolveRoomPolygonDepth(", source)
        self.assertIn("biased < 0", source)
        self.assertIn("AddPrim((u_long*)Render_frameScratch.orderingTable + depth, packet)", source)

    def test_room_subdivision_entries_share_the_existing_bounded_recursion(self):
        owned = self.owned_items()
        for identity, address in (("F00748", 0x80097AEC), ("F00749", 0x80097CF0)):
            self.assertEqual(
                owned[identity]["path"], r"Native\Engine\src\Render\RoomDraw.c"
            )
            self.assertIn(
                ("BATTLE/BATTLE.PRG", address, identity),
                copy_fixups.MODULE_FUNCTION_ENTRIES,
            )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\RoomDraw.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("RoomSubdivisionLimit(", source)
        self.assertIn("memcpy(captured, vertices, sizeof captured)", source)
        self.assertIn("Render_SubdivideRoomTriangle(vertices, subdivision", source)
        self.assertIn("Render_SubdivideRoomQuad(vertices, subdivision", source)

    def test_room_coordinate_decoder_uses_the_live_owned_polygon_pipeline(self):
        self.assertEqual(
            self.owned_items()["F00746"]["path"],
            r"Native\Engine\src\Render\RoomDraw.c",
        )
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x8009797C, "F00746"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\RoomDraw.c").read_text(
            encoding="utf-8"
        )
        self.assertEqual(source.count("Render_DecodeRoomPolygonVertices("), 2)
        self.assertIn("memcpy(packed, data, sizeof packed)", source)
        self.assertIn("if (index == kQuadCorners - 1)", source)

    def test_fog_and_depth_cue_share_the_existing_typed_native_helpers(self):
        for identity, name, address in (
            ("F03404", "Core_Gte_DpqColor", 0x80041A00),
            ("F03408", "Core_Gte_SetFogNear", 0x80041954),
        ):
            self.assertEqual(
                copy_fixups.NATIVE_FUNCTION_REDIRECTS[identity],
                (name, r"Native\Engine\src\Core\Gte.cpp"),
            )
            self.assertIn(
                ("SLUS_010.40", address, identity),
                copy_fixups.MODULE_FUNCTION_ENTRIES,
            )
            self.assertNotIn(identity, self.owned_items())

    def test_source_root_and_normalization_keep_the_wide_native_api_separate(self):
        owned = self.owned_items()
        for identity in ("F03405", "F03414"):
            self.assertEqual(
                owned[identity]["path"],
                r"Native\Engine\src\Core\Math\ScalarMath.c",
            )
        for address, identity in ((0x80041A1C, "F03405"), (0x80041AF4, "F03414")):
            self.assertIn(
                ("SLUS_010.40", address, identity),
                copy_fixups.MODULE_FUNCTION_ENTRIES,
            )
        self.assertEqual(
            copy_fixups.NATIVE_FUNCTION_REDIRECTS["F03411"],
            ("Core_Gte_VectorNormal", r"Native\Engine\src\Core\Gte.cpp"),
        )

    def test_resident_rotation_pair_redirects_to_the_existing_typed_kernel(self):
        self.assertEqual(
            copy_fixups.NATIVE_FUNCTION_REDIRECTS["F03399"],
            (
                "Core_Math_LookupRotationSineCosine",
                r"Native\Engine\src\Core\Math\ScalarMath.c",
            ),
        )
        self.assertIn(
            ("SLUS_010.40", 0x80041104, "F03399"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        self.assertNotIn("F03399", self.owned_items())

    def test_tile_component_entry_shares_native_width_helpers_and_callers(self):
        self.assertEqual(
            self.owned_items()["F01772"]["path"],
            r"Native\Engine\src\Core\Math\TileDistance.cpp",
        )
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x800E4604, "F01772"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        root = Path(__file__).resolve().parents[2]
        source = (
            root / r"Native\Engine\src\Core\Math\TileDistance.cpp"
        ).read_text(encoding="utf-8")
        self.assertEqual(source.count("Core_Math_SquaredTileDistanceFromComponents("), 2)
        self.assertIn("Core_Math_WrapSigned16", source)
        self.assertIn("Core_Math_SquaredVectorLength(dx, 0, dz)", source)
        caller = (
            root / r"Native\Game\src\Battle\Ai\GroundNavigation.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("Core_Math_SquaredTileDistanceFromComponents(", caller)

    def test_native_terrain_envelope_calls_the_existing_builder_without_guest_stack(self):
        self.assertEqual(
            self.owned_items()["F01832"]["path"],
            r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
        )
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x800E6B4C, "F01832"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        root = Path(__file__).resolve().parents[2]
        source = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp"
        ).read_text(encoding="utf-8")
        bodies = audit_assembly_gaps.definitions(
            source, {"Battle_Ai_BuildAiTerrainHeightsWithWorkspace"}
        )
        self.assertEqual(len(bodies), 1)
        body = bodies[0].body
        self.assertIn("Battle_Ai_BuildAiSpecialTerrainHeights();", body)
        self.assertNotIn("AllocateFromHighEnd", body)
        self.assertNotIn("Core_ScratchBase", body)

    def test_segment_trace_owns_typed_geometry_and_the_live_boolean_consumer(self):
        root = Path(__file__).resolve().parents[2]
        scaffold = runpy.run_path(str(root / r"tools\scaffold\gen_solution.py"))
        self.assertTrue(
            scaffold["needs_c_headers"]("VS.Game", r"src\Field\SegmentCollision.cpp")
        )
        for identity in ("F01833", "F01834"):
            self.assertEqual(
                self.owned_items()[identity]["path"],
                r"Native\Game\src\Field\SegmentCollision.cpp",
            )
        for address, identity in ((0x800E6BA0, "F01833"), (0x800E6C34, "F01834")):
            self.assertIn(
                ("BATTLE/BATTLE.PRG", address, identity),
                copy_fixups.MODULE_FUNCTION_ENTRIES,
            )
        self.assertEqual(
            copy_fixups.FIXES["F00514"][0][1], "spec->resetsHitResults"
        )
        self.assertIn("!= NULL;", copy_fixups.FIXES["F00514"][1][1])

    def test_screen_warp_owns_native_geometry_and_original_entry(self):
        root = Path(__file__).resolve().parents[2]
        scaffold = runpy.run_path(str(root / r"tools\scaffold\gen_solution.py"))
        self.assertTrue(
            scaffold["needs_c_headers"](
                "VS.Game", r"src\Effects\ScreenDistortion\RadialWarp.cpp"
            )
        )
        self.assertEqual(
            self.owned_items()["F02458"]["path"],
            r"Native\Game\src\Effects\ScreenDistortion\RadialWarp.cpp",
        )
        self.assertIn(
            ("GIM/SCREFF2.PRG", 0x800F9BD8, "F02458"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        source = (
            root / r"Native\Game\src\Effects\ScreenDistortion\RadialWarp.cpp"
        ).read_text(encoding="utf-8")
        self.assertNotIn("Core_ScratchBase", source)
        self.assertNotIn("0x1F800", source)
        self.assertIn("Core_Math_kArcTangentTable", source)

    def test_screen_effect_defaults_own_native_state_and_use_typed_access(self):
        root = Path(__file__).resolve().parents[2]
        scaffold = runpy.run_path(str(root / r"tools\scaffold\gen_solution.py"))
        self.assertTrue(
            scaffold["needs_c_headers"]("VS.Game", r"src\Effects\ScreenEffectState.cpp")
        )
        owned = self.owned_items()
        self.assertEqual(owned["G00512"]["kind"], "global")
        self.assertEqual(
            owned["G00512"]["path"], r"Native\Game\src\Effects\ScreenEffectState.cpp"
        )
        for identity in ("F01363", "F01389", "F01390"):
            self.assertEqual(len(copy_fixups.FIXES[identity]), 1)
        self.assertIn(
            "Effects_screenEffectDefaults.radialWarpArg0",
            copy_fixups.FIXES["F01363"][0][1],
        )
        self.assertEqual(
            copy_fixups.FIXES["F01390"][0][1], "&Effects_screenEffectDefaults"
        )
        with (root / r"Docs\ledgers\DATA_LEDGER.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            row = next(row for row in csv.DictReader(stream) if row["id"] == "G00512")
        self.assertEqual(
            (row["address"], row["size"], row["type"]),
            ("0x800F4ED8", "16", "short[8]"),
        )
        symbols = (root / r"Native\Game\src\Boot\ModulePointers.c").read_text(
            encoding="utf-8"
        )
        for address, size, name in (
            ("800F4ED8", 16, "Effects_screenEffectDefaults"),
            ("800F1D08", 4, "Field_matchingDoorRecords"),
        ):
            self.assertIn(
                f'{{"BATTLE/BATTLE.PRG", 0x{address}u, {size}u, '
                f"(void*)&{name}, sizeof {name}, 0u, 0u}}",
                symbols,
            )

    def test_actor_shadows_import_mesh_data_and_own_the_production_draw_entry(self):
        expected = {
            "Content_battleShadowVertices": (0x81268, 0x800E9A68, 72),
            "Content_battleShadowFaces": (0x812D4, 0x800E9AD4, 96),
        }
        for name, values in expected.items():
            blob = next(item for item in copy_fixups.MODULE_DATA_BLOBS if item[4] == name)
            self.assertEqual(blob[:4], ("BATTLE/BATTLE.PRG", *values))
        owned = self.owned_items()
        self.assertEqual(
            owned["F01268"]["path"], r"Native\Engine\src\Render\ModelGeometry.c"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Game\src\Actors\ModelDrawing.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("companion == 0 && actor->instance.soundEnabled", source)
        self.assertIn("Render_DrawActorGroundShadow(actor,", source)
        self.assertIn("goto draw_shadow;", source)

    def test_room_euler_uses_the_source_staged_kernel_and_shared_octant_pair(self):
        owned = self.owned_items()
        for identity in ("F00761", "F00762"):
            self.assertEqual(
                owned[identity]["path"], r"Native\Engine\src\Core\Math\ScalarMath.c"
            )
        self.assertEqual(
            copy_fixups.NATIVE_FUNCTION_REDIRECTS["F01017"],
            (
                "Core_Math_LookupRotationSineCosine",
                r"Native\Engine\src\Core\Math\ScalarMath.c",
            ),
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\RoomDraw.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("Core_Math_BuildEulerRotationMatrix(&rotation, output);", source)
        self.assertNotIn("Core_Gte_RotMatrixYXZ(", source)

    def test_core_idle_and_matrix_entries_reuse_native_leaf_implementations(self):
        self.assertEqual(
            copy_fixups.NATIVE_FUNCTION_REDIRECTS["F03568"],
            ("Core_IgnoreCall", r"Native\Engine\src\Core\Runtime.c"),
        )
        owned = self.owned_items()
        self.assertEqual(
            owned["F03406"]["path"], r"Native\Engine\src\Core\Math\ScalarMath.c"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Core\Math\ScalarMath.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("MultiplyRotationMatrices(left, right, right);", source)
        self.assertIn("MultiplyRotationMatrices(left, right, left);", source)

    def test_weapon_override_reads_a_typed_matrix_member_not_native_guest_offset(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01269"]["path"], r"Native\Engine\src\Render\ModelGeometry.c"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\ModelGeometry.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("weapon->boneWorldMatrices[kWeaponPaletteMatrix]", source)
        self.assertIn("AppearanceMaterial(weapon->ownerMotionState)", source)

    def test_gimmick_override_uses_the_existing_model_kernel_and_typed_state(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01270"]["path"], r"Native\Engine\src\Render\ModelGeometry.c"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\ModelGeometry.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("AppearanceMaterial(object->motionState)", source)
        self.assertIn("object->palettePulse.kind", source)
        self.assertIn("override ? AppearanceMaterial(state) : 0", source)

    def test_room_tint_pass_shares_the_explicit_native_color_contract(self):
        owned = self.owned_items()
        for identity in ("F00769", "F00770"):
            self.assertEqual(
                owned[identity]["path"],
                r"Native\Engine\src\Render\RoomDraw.c",
            )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\RoomDraw.c").read_text(
            encoding="utf-8"
        )
        self.assertEqual(source.count("Render_TintRoomPrimitiveColor("), 2)
        self.assertIn("const int16_t strength = tint->strength;", source)
        header = (root / r"Native\Engine\src\Render\RoomDraw.h").read_text(
            encoding="utf-8"
        )
        dependency = header.index('#include "Field/Room.h"')
        self.assertLess(header.index('extern "C" {'), dependency)
        self.assertLess(dependency, header.index("\n}\n#endif"))

    def test_room_light_triangle_shares_the_existing_owned_sampler_kernel(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F00768"]["path"], r"Native\Engine\src\Render\RoomLighting.c"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\RoomLighting.c").read_text(
            encoding="utf-8"
        )
        self.assertEqual(source.count("Render_InterpolateRoomLightTriangle("), 2)
        self.assertIn("return SignedWord((uint32_t)area);", source)

    def test_script_weapon_mode_owns_its_original_opcode_and_named_wait_state(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01130"]["path"], r"Native\Game\src\Event\WeaponMode.cpp"
        )
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\DATA_LEDGER.csv").open(
            encoding="utf-8-sig", newline=""
        ) as handle:
            row = next(row for row in csv.DictReader(handle) if row["id"] == "G00473")
        self.assertEqual(row["final_name"], "Event::weaponTransitionPending")
        self.assertEqual(row["confidence"], "high")
        with (root / r"tools\naming\sym_out\Game_Event\globals.csv").open(
            encoding="utf-8-sig", newline=""
        ) as handle:
            proposal = next(
                row for row in csv.DictReader(handle) if row["id"] == "G00473"
            )
        for field in ("final_name", "purpose", "confidence", "notes"):
            self.assertEqual(proposal[field], row[field])
        with (root / r"Docs\ledgers\TRACE_QUEUE.csv").open(
            encoding="utf-8-sig", newline=""
        ) as handle:
            self.assertNotIn("G00473", {item["id"] for item in csv.DictReader(handle)})
        source = (root / r"Native\Game\src\Event\ScriptVm.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("Event_weaponTransitionPending = 0;", source)
        self.assertNotIn("Event_reservedResetByte", source)

    def test_owned_function_catalog_matches_the_registered_native_definition(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8-sig", newline=""
        ) as handle:
            rows = list(csv.DictReader(handle))
        owned = self.owned_items()
        rows = [
            row
            for row in rows
            if row["id"] in owned
            and owned[row["id"]]["kind"] == "function"
            and row["copy"] == "yes"
        ]
        self.assertTrue(rows)
        definitions, paths = audit_assembly_gaps.native_definitions(rows, owned)
        for row in rows:
            with self.subTest(id=row["id"]):
                if row["kind"].startswith("template:") or re.search(
                    r"(?:^|;)native-template:", row["notes"]
                ):
                    status, _ = audit_assembly_gaps.template_status(row)
                    self.assertIn(
                        status, ("template-instance", "stub", "partial-unported")
                    )
                else:
                    bodies = [
                        body
                        for body in definitions[paths[row["id"]]]
                        if body.name == row["final_name"].replace("::", "_")
                    ]
                    self.assertEqual(len(bodies), 1)
                    status = audit_assembly_gaps.native_status(bodies[0].body)
                    self.assertIn(status, ("implemented", "stub", "partial-unported"))
                expected = (
                    "x" if status in ("implemented", "template-instance") else "stub"
                )
                self.assertEqual(
                    (row["final_location"], row["copied"]),
                    (owned[row["id"]]["path"], expected),
                )

    def test_orbit_constructor_shares_the_existing_owned_teardown(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01845"]["path"],
            r"Native\Game\src\Battle\Ai\ActorCleanup.cpp",
        )
        self.assertEqual(owned["F01845"]["name"], "Battle_Ai_AllocateOrbitingBossState")

    def test_hostility_transition_is_owned_with_existing_reaction_lifecycle(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01695"]["path"],
            r"Native\Game\src\Battle\Ai\ActorCleanup.cpp",
        )
        self.assertEqual(owned["F01695"]["name"], "Battle_Ai_MakeTargetHostile")

    def test_dynamic_tile_marker_is_owned_with_existing_tile_primitives(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01828"]["path"],
            r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
        )
        self.assertEqual(owned["F01828"]["name"], "Battle_Ai_MarkAiTileDynamicObstacle")

    def test_room_screen_bounds_share_the_original_predicate(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F00756"]["path"], r"Native\Engine\src\Render\RoomDraw.c"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\RoomDraw.c").read_text(
            encoding="utf-8"
        )
        self.assertEqual(source.count("OutsideScreenBounds(&projected)"), 2)
        for edge in ("left", "top", "right", "bottom"):
            self.assertIn(
                f"room_screen_bounds.{edge} = Core_Math_WrapSigned16((uint32_t){edge});",
                source,
            )

    def test_room_far_depth_owns_both_packet_emission_paths(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F00754"]["path"], r"Native\Engine\src\Render\RoomDraw.c"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\RoomDraw.c").read_text(
            encoding="utf-8"
        )
        self.assertEqual(source.count("biased >= room_far_depth"), 1)
        self.assertIn(
            "ResolveRoomPolygonDepth(polygon->depth, polygon->material.flags)",
            source,
        )
        self.assertEqual(source.count("return WriteRoomPolygon(&polygon, limit);"), 2)
        self.assertIn(
            "room_far_depth = Core_Math_WrapSigned16((uint32_t)depth);", source
        )

    def test_native_pointer_sort_is_owned_and_shared_with_ai_ranking(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01829"]["path"], r"Native\Engine\src\Core\PointerSort.cpp"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Game\src\Battle\Ai\PlanningPrimitives.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("Core_SortPointersByDescendingKey(pointers.data()", source)
        self.assertNotIn("SortCandidatePointers(entries.first(", source)

    def test_ai_lifecycle_leaves_share_the_native_persistence_projection(self):
        owned = self.owned_items()
        for identifier, name in (
            ("F01820", "Battle_Ai_PackActorAiPersistenceFlags"),
            ("F01831", "Battle_Ai_ClearActorAiNavigationReset"),
            ("F01835", "Battle_Ai_InitializeActorAiExtensionHook"),
        ):
            self.assertEqual(owned[identifier]["name"], name)
            self.assertEqual(
                owned[identifier]["path"],
                r"Native\Game\src\Battle\Ai\ActorCleanup.cpp",
            )
        root = Path(__file__).resolve().parents[2]
        snapshot = (root / r"Native\Game\src\Save\RuntimeSnapshot.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "found->aiFlags = Battle_Ai_PackActorAiPersistenceFlags(slot);",
            snapshot,
        )
        self.assertNotIn("relation.previousClassification <<", snapshot)

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
        with (
            Path(__file__)
            .resolve()
            .with_name("owned.csv")
            .open(encoding="utf-8", newline="") as stream
        ):
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
            self.assertEqual(
                owned[identifier]["name"], "Audio_UpdateActorSoundTransition"
            )
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
            fix
            for fix in copy_fixups.TYPE_FIXES["T00532"]
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
        self.assertIn(
            "} else {\n            Battle_FinishBattleAbilitySequence();", new
        )
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
        self.assertIn("(u_char)*str++", replacements["u_int currentChar = *str++;"])
        self.assertIn("(u_char)*str++", replacements["u_int opcodeParam = *str++;"])

    def test_attached_draw_requires_a_resident_parent_matrix(self):
        old, new, _ = next(
            fix
            for fix in copy_fixups.FIXES["F01008"]
            if "entries[actor->instance.attachParent]->boneMatrices" in fix[0]
        )
        self.assertIn("entries[actor->instance.attachParent]->boneMatrices", old)
        self.assertIn("Actors_ResolveAttachmentParentMatrix(actor)", new)
        self.assertIn("if (parent_matrix != NULL)", new)
        self.assertNotIn("entries[", new)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Actors_ResolveAttachmentParentMatrix"],
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
        self.assertEqual(motion[1], "        Actors_SaveActorGroundedTransform(actor);")
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Actors_SaveActorGroundedTransform"],
            owned["F00989"]["header"],
        )

    def test_native_file_cards_draw_after_footer_and_before_fade(self):
        for fid, prefix, title in (("F02757", "Menu_Files", 0), ("F03624", "Title", 1)):
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
            item
            for item in copy_fixups.MODULE_DATA_BLOBS
            if item[4] == "Content_battleTriggerMarkerBytes"
        )
        self.assertEqual(blob[:4], ("BATTLE/BATTLE.PRG", 0x7FD48, 0x800E8548, 116))
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
        self.assertLess(
            finish[1].index("Save_NativeFinishResume"), finish[1].index("if ((temp_s4")
        )
        zone = copy_fixups.FIXES["F00533"][0]
        self.assertTrue(zone[1].startswith("    if (!Save_NativeResumePending())"))
        for fid in ("F03423", "F03437", "F03673"):
            with self.subTest(fid=fid):
                reset = copy_fixups.FIXES[fid][-1]
                self.assertIn("{\n    Save_NativeResetSession();", reset[1])
        for name in (
            "Save_NativeResumePending",
            "Save_NativeFinishResume",
            "Save_NativeResetSession",
        ):
            self.assertEqual(
                copy_fixups.NATIVE_HELPER_HEADERS[name],
                r"Native\Game\include\Save\NativeResume.h",
            )

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
            (
                "Field::FindRoomGeometryGroup",
                "void* find(int arg0) { int v1; int* a1; return arg0 < v1 ? a1 + arg0 * 16 : NULL; }",
                "void* find(int group_index) { int group_count; int* groups; return group_index < group_count ? groups + group_index * 16 : NULL; }",
            ),
            (
                "Field::ShowRoomGeometryGroup",
                "int show(int arg0) { int* temp_v0 = find(arg0); *temp_v0 &= ~0x100; }",
                "int show(int group_index) { int* group = find(group_index); *group &= ~0x100; }",
            ),
            (
                "Field::HideRoomGeometryGroup",
                "int hide(int arg0) { int* temp_v0 = find(arg0); *temp_v0 |= 0x100; }",
                "int hide(int group_index) { int* group = find(group_index); *group |= 0x100; }",
            ),
            (
                "Field::SwapRoomGeometryGroupFlags",
                "int swap(int arg0, int arg1) { int* temp_s0 = find(arg0); int* temp_a1 = find(arg1); int temp_a0 = *temp_s0; *temp_s0 = *temp_a1; *temp_a1 = temp_a0; }",
                "int swap(int first_group_index, int second_group_index) { int* first_group = find(first_group_index); int* second_group = find(second_group_index); int saved_flags = *first_group; *first_group = *second_group; *second_group = saved_flags; }",
            ),
            (
                "Field::AddRoomGeometryDepthBias",
                "int bias(int arg0, int arg1) { u_short* temp_v0 = find(arg0); temp_v0[1] = (u_short)(temp_v0[1] + arg1); }",
                "int bias(int group_index, int depth_bias) { u_short* group_halfwords = find(group_index); group_halfwords[1] = (u_short)(group_halfwords[1] + depth_bias); }",
            ),
            (
                "Field::DisableRoomGeometryGroup",
                "int disable(int arg0) { int* temp_v0 = find(arg0); *temp_v0 |= 0x1000; }",
                "int disable(int group_index) { int* group = find(group_index); *group |= 0x1000; }",
            ),
            (
                "Field::UnrelatedGeometryCommand",
                "void untouched(int arg0) { int temp_v0 = arg0; }",
                "void untouched(int arg0) { int temp_v0 = arg0; }",
            ),
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
            item
            for item in copy_fixups.MODULE_DATA_BLOBS
            if item[4] == "Content_residentLfoSamples"
        )
        self.assertEqual(blob[:4], ("SLUS_010.40", 0x800, 0x80010000, 0x52000))
        self.assertEqual(copy_fixups.MODULE_DATA_BLOB_TYPES[blob[4]], ("short", 2))

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
            self.assertLess(
                source.index("Audio_Seq_LfoWaveform"), source.index("updateFlags")
            )
            self.assertIn(f"programCounter += {count}", source)
            self.assertIn("(u_char)*in_pChannel->programCounter++", source)
            self.assertNotIn("Audio_Seq_kLfoWaveforms[", source)

    def test_hud_timer_retirement_does_not_add_a_packed_pointer_view(self):
        ledger = (
            Path(__file__).resolve().parents[2] / "Docs" / "ledgers" / "DATA_LEDGER.csv"
        )
        with ledger.open(encoding="utf-8", newline="") as stream:
            row = next(
                item for item in csv.DictReader(stream) if item["id"] == "G01934"
            )
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
        self.assertLess(
            source.index("Core_Debugf"), source.index("Render_DrawCompactText")
        )
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
        templates = copier.split("    def copy_templates(", 1)[1].split("    # ---", 1)[
            0
        ]
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
            (
                "Effects::ClearParticleTextureState",
                "void clear(int* arg0) { state = arg0; }",
                "void clear(int* storage) { state = storage; }",
            ),
            (
                "Effects::InitializeParticleTextureAnimation",
                "void init(State* arg0, int textureIndex, int arg2) { arg0->program = table[arg2 + 2]; }",
                "void init(State* animation, int textureIndex, int programIndex) { animation->program = table[programIndex + 2]; }",
            ),
            (
                "Effects::AdvanceParticleTextureAnimation",
                "void advance(State* arg0) { int w = read(arg0); switch (w & 0xF0000000) {} }",
                "void advance(State* animation) { int command = read(animation); switch (command & 0xF0000000) {} }",
            ),
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
        self.assertLess(
            source.index("Actors_RestartActorAnimationEvents"),
            source.index("actor->animationHeader = 0"),
        )
        self.assertLess(
            source.index("actor->blendPose.eventScript = NULL"),
            source.index("Content_ReleaseActorSequence"),
        )
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
        self.assertLess(
            source.index("v0 == NULL || sp10 == NULL"), source.index("VS_ONDISC")
        )
        self.assertIn("return -1;", source)
        self.assertIn("return 0;", source)
        self.assertIn("Core_Errorf", source)
        for fid in ("F00856", "F00992", "F01081"):
            self.assertTrue(
                any(
                    "return -1;" in new or "return 1;" in new or "return;" in new
                    for _, new, _ in copy_fixups.FIXES[fid]
                )
            )

    def test_unwind_callers_match_pre_normalization_flag_expressions(self):
        for fid, expression in (
            ("F00856", "((*(int*)((char*)actor + 0x5AC)) & 0x800000)"),
            ("F00992", "(*(int*)((u_char*)actor + 0x5AC) & 0x800000)"),
        ):
            guard = f"    if {expression} {{\n"
            source = (
                ("    actor->animationEventFlag = 0;\n" if fid == "F00856" else "")
                + guard
                + "        Actors_RestartActorAnimationEvents(actor);\n"
                "    }\n"
                + ("    actor->poseOverrideActive = 0;" if fid == "F00856" else "")
            )
            replacements = [
                (old, new)
                for old, new, _ in copy_fixups.FIXES[fid]
                if "Actors_RestartActorAnimationEvents" in old
            ]
            self.assertEqual(len(replacements), 1)
            old, new = replacements[0]
            self.assertIn(old, source)
            source = source.replace(old, new)
            self.assertIn("Actors_RestartActorAnimationEvents(actor) < 0", source)

    def test_scripted_effect_requires_its_actor_before_starting_playback(self):
        source = """    Effects_StartScriptedActorEffect(
        Core_Memory_ReadUnalignedShort(arg0 + 1), Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 3)));
    return 0;
"""
        for old, new, _ in copy_fixups.FIXES["F01170"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertLess(
            source.index("(unsigned)actor >= 16"),
            source.index("Actors_actorRecords[actor]"),
        )
        self.assertLess(
            source.index("return 1;"), source.index("Effects_StartScriptedActorEffect")
        )
        self.assertIn("Field_gimmickInstances[actor] == NULL", source)
        self.assertIn("referenceType <= 1 ? Event_ResolveScriptActorReference", source)
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
        self.assertLess(
            source.index("currentSfxBank == 0"), source.index("currentSfxBank - 1]")
        )
        self.assertIn("Core_Warnf", source)
        self.assertIn("Core_Errorf", source)
        self.assertIn("var_s0_2 == NULL", source)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Core_Errorf"],
            r"Native\Engine\include\Core\Diagnostics.h",
        )

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
        self.assertLess(
            source.index("Core_Memory_InitializeHeap"),
            source.index("Battle_Ai_InvalidateSessionBindings"),
        )
        self.assertLess(
            source.index("Save_NativeResetSession"),
            source.index("Core_Memory_InitializeHeap"),
        )
        self.assertLess(
            source.index("Battle_Ai_InvalidateSessionBindings"),
            source.index("Content_InitializeDiscQueue"),
        )
        self.assertEqual(source.count("Battle_Ai_InvalidateSessionBindings"), 1)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Battle_Ai_InvalidateSessionBindings"],
            r"Native\Game\src\Battle\Ai\RoomState.h",
        )

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
        for key in (
            "F00946",
            "F00947",
            "F00948",
            "F00949",
            "F00950",
            "F00952",
            "F00953",
        ):
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
        self.assertLess(source.index("if (temp_s1 == NULL)"), source.index("sp10.vx"))
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
        self.assertLess(
            source.index("return -1;"), source.index("model = actor->instance.model;")
        )
        self.assertLess(
            source.index("model = actor->instance.model;"),
            source.index("matrix = Actors_ActorBoneWorldTransform"),
        )

    def test_room_lifetimes_revoke_diagnostic_readiness(self):
        for key, header in (
            ("F00543", "void Field_FinishRoomLoad(int arg0)\n{"),
            ("F00545", "void Field_UnloadCurrentRoom(void)\n{"),
        ):
            source = header + "\n    OriginalRoomWork();\n}"
            for old, new, _ in copy_fixups.FIXES[key]:
                source = source.replace(old, new)
            self.assertLess(
                source.index("Boot_ResetGameplayInspection();"),
                source.index("OriginalRoomWork();"),
            )

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

    def test_heap_names_preserve_native_header_units_and_search_direction(self):
        examples = {
            "Core::Memory::FreeFromHighEnd": (
                "var_a1 = var_a1->prev; target->next = var_a1->next;",
                "previous_free = previous_free->prev; "
                "target->next = previous_free->next;",
            ),
            "Core::Memory::AllocateFromHighEnd": (
                "blockSz = size; var_a2 = var_a1; "
                "var_a1 += var_a1->blockSize;",
                "allocation_units = size; following_free = allocation_block; "
                "allocation_block += allocation_block->blockSize;",
            ),
            "Core::Memory::AllocateFromLowEnd": (
                "temp_v1 = var_a1 + blockSz; var_v1->next = temp_v1;",
                "remainder = allocation_block + allocation_units; "
                "previous_free->next = remainder;",
            ),
            "Core::Memory::InitializeHeap": (
                "node->blockSize = value / sizeof(Core_HeapBlockHeader) - 1;",
                "node->blockSize = byte_count / "
                "sizeof(Core_HeapBlockHeader) - 1;",
            ),
        }
        for name, (original, expected) in examples.items():
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_core_names_preserve_memory_units_and_random_draws(self):
        examples = {
            "Core::Memory::ReadUnalignedShort": (
                "return arg0[0] | (arg0[1] << 8);",
                "return source[0] | (source[1] << 8);",
            ),
            "Core::Memory::ZeroWordsBackward": (
                "var_v0 = arg0 + arg1; do { *--var_v0 = 0; } "
                "while (arg0 != var_v0);",
                "word_cursor = destination + byte_count; "
                "do { *--word_cursor = 0; } "
                "while (destination != word_cursor);",
            ),
            "Core::Random::NextInterfaceRandomBits": (
                "state = temp_a1 * 0x19660D; return temp_a1 >> (32 - value);",
                "state = previous_state * 0x19660D; "
                "return previous_state >> (32 - bit_count);",
            ),
            "Core::Random::RandomRangeExclusive": (
                "if (arg1 < arg0) { new_var = Rand() % (arg0 - arg1); "
                "return new_var + arg1; }",
                "if (bound_b < bound_a) { "
                "offset = Rand() % (bound_a - bound_b); "
                "return offset + bound_b; }",
            ),
            "Core::Random::NextBounded": (
                "index = var_v0 >> 15; temp_s0 = 32767 - table[index]; "
                "var_a0 = temp_s0 * max; return var_a0 >> 15;",
                "index = scaled_index >> 15; "
                "picked_sample = 32767 - table[index]; "
                "scaled_result = picked_sample * max; "
                "return scaled_result >> 15;",
            ),
            "Core::Random::NextAveraged": (
                "return (Next(arg0) + Next(arg0)) / 12;",
                "return (Next(max) + Next(max)) / 12;",
            ),
        }
        for name, (original, expected) in examples.items():
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_math_names_preserve_distinct_vector_destination_positions(self):
        examples = {
            "Core::Math::AddVectors": (
                "setVector(arg0, arg1->vx + arg2->vx, "
                "arg1->vy + arg2->vy, arg1->vz + arg2->vz); return arg0;",
                "setVector(destination, left->vx + right->vx, "
                "left->vy + right->vy, left->vz + right->vz); "
                "return destination;",
            ),
            "Core::Math::AddWideVectors": (
                "arg2->vx = arg0->vx + arg1->vx;",
                "destination->vx = left->vx + right->vx;",
            ),
            "Core::Math::NormalizeFixedVector": (
                "setVector(&sp10, arg0->vx >> 12, arg0->vy >> 12, "
                "arg0->vz >> 12); VectorNormal(&sp10, arg1);",
                "setVector(&integer_vector, source->vx >> 12, "
                "source->vy >> 12, source->vz >> 12); "
                "VectorNormal(&integer_vector, destination);",
            ),
        }
        for name, (original, expected) in examples.items():
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_interpolation_names_preserve_duration_order_and_rounding(self):
        examples = {
            "Core::Math::InterpolateTimedScalar": (
                "return Interpolate(arg0, arg1, (arg3 * (ONE * 2)) / arg2);",
                "return Interpolate(start, end, "
                "(current_step * (ONE * 2)) / total_steps);",
            ),
            "Core::Math::InterpolateShortPair": (
                "int new_var = arg0[0]; "
                "return (((arg0[1] - new_var) * arg1) >> 7) + new_var;",
                "int start = endpoints[0]; "
                "return (((endpoints[1] - start) * weight_q7) >> 7) + start;",
            ),
            "Core::Math::InterpolateThirteenBitScalar": (
                "int var_v0 = (arg1 - arg0) * arg2; "
                "return arg0 + var_v0 / (ONE * 2);",
                "int scaled_delta = (end - start) * weight_q13; "
                "return start + scaled_delta / (ONE * 2);",
            ),
        }
        for name, (original, expected) in examples.items():
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_weapon_sequence_copy_names_preserve_the_native_offset_contract(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            row = next(item for item in csv.DictReader(stream)
                       if item["id"] == "F00797")
        self.assertEqual(row["name"], "func_8009C378")
        self.assertEqual(row["final_name"], "Content::CopyWeaponSequence")
        self.assertEqual(len(copy_fixups.FIXES["F00797"]), 4)
        self.assertTrue(all(not new for _, new, _ in copy_fixups.FIXES["F00797"]))
        source = (root / row["final_location"]).read_text(encoding="utf-8")
        definition = audit_assembly_gaps.definitions(
            source, {"Content_CopyWeaponSequence"}
        )
        self.assertEqual(len(definition), 1)
        self.assertEqual(
            definition[0].body.strip(),
            "Core_Memory_CopyAlignedWords(destination, source, 0x7800);",
        )

    def test_audio_names_preserve_distinct_bank_and_output_bindings(self):
        examples = {
            "Audio::PlayRandomActorImpactSound": (
                "Play(arg0, arg1, arg2);",
                "Play(sound_bank, pan, volume);",
            ),
            "Audio::ActorSoundPanAndVolume": (
                "temp_s0 = actors[arg0]; *arg1 = temp_v0 >> 16; "
                "if (arg2) *arg2 = temp_v0 & 65535;",
                "actor = actors[actor_index]; "
                "*output_pan = packed_pan_volume >> 16; "
                "if (output_volume) *output_volume = "
                "packed_pan_volume & 65535;",
            ),
        }
        for name, (original, expected) in examples.items():
            with self.subTest(name=name):
                source = original
                unrelated = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                    if re.search(row, "Content::Unrelated"):
                        unrelated = re.sub(pattern, replacement, unrelated)
                self.assertEqual(source, expected)
                self.assertEqual(unrelated, original)

    def test_room_actor_cache_opcode_name_preserves_original_provenance(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            row = next(item for item in csv.DictReader(stream)
                       if item["id"] == "F01099")
        self.assertEqual(row["name"], "func_800B91C4")
        self.assertEqual(row["final_name"], "Event::ScriptClearRoomActorCache")
        source = (root / row["final_location"]).read_text(encoding="utf-8")
        self.assertIn("func_800B91C4", source)
        self.assertIn("Event_ScriptClearRoomActorCache", source)
        self.assertNotIn("Event_ScriptPreloadRoom", source)
        self.assertIn(
            "Field_ClearSavedRoomEnemies(instruction[2], instruction[1]);",
            source,
        )

    def test_event_parameter_scope_matches_registered_opcode_handlers(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            scope = {
                row["id"] for row in csv.DictReader(stream)
                if re.match(r"^Event::(?:Script|Command)", row["final_name"])
            }
        self.assertEqual(scope, copy_fixups.EVENT_OPCODE_HANDLERS)

    def test_event_opcode_parameter_names_follow_the_dispatch_contract(self):
        examples = (
            "Event::ScriptBranchOnFlagImmediate",
            "Event::ScriptPlayActorAnimation",
            "Event::CommandStartScriptContext",
            "Event::CommandLoadNextEvent",
        )
        original = (
            "intptr_t Handler(u_char* arg0, short arg1) "
            "{ return arg0[Event_threadWaitFrames[arg1]]; }"
        )
        expected = (
            "intptr_t Handler(u_char* instruction, short thread_index) "
            "{ return instruction[Event_threadWaitFrames[thread_index]]; }"
        )
        for name in examples:
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_event_opcode_naming_does_not_change_non_handler_parameters(self):
        original = "int Utility(int arg0, int arg1) { return arg0 + arg1; }"
        for name in ("Event::EvaluateRoomEventTriggers", "Event::OpcodeSize"):
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, original)

    def test_vibration_names_are_scoped_identifier_only_changes(self):
        examples = {
            "Input::MapStickDeadZone": (
                "int Map(int arg0) { if (arg0 < 64) return arg0 - 64; "
                "if (arg0 >= 192) return arg0 - 192; return 0; }",
                "int Map(int axis_byte) { "
                "if (axis_byte < 64) return axis_byte - 64; "
                "if (axis_byte >= 192) return axis_byte - 192; return 0; }",
            ),
            "Input::PlayVibrationPreset": (
                "void Play(int arg0) { int new_var4, new_var, new_var3, new_var2; "
                "if (arg0 < presets[0]) { new_var = 5; new_var3 = 4; "
                "new_var4 = 6; new_var2 = 7; "
                "Start(&presets[presets[arg0 * new_var3 + new_var3] "
                "+ (presets[arg0 * new_var3 + new_var] << 8) "
                "+ (presets[arg0 * new_var3 + new_var4] << 16) "
                "+ (presets[arg0 * new_var3 + new_var2] << 24)], 255, 128); }}",
                "void Play(int preset_index) { int offset_byte2_index, "
                "offset_byte1_index, directory_stride, offset_byte3_index; "
                "if (preset_index < presets[0]) { offset_byte1_index = 5; "
                "directory_stride = 4; offset_byte2_index = 6; "
                "offset_byte3_index = 7; "
                "Start(&presets[presets[preset_index * directory_stride "
                "+ directory_stride] "
                "+ (presets[preset_index * directory_stride "
                "+ offset_byte1_index] << 8) "
                "+ (presets[preset_index * directory_stride "
                "+ offset_byte2_index] << 16) "
                "+ (presets[preset_index * directory_stride "
                "+ offset_byte3_index] << 24)], 255, 128); }}",
            ),
            "Input::MotorCommand": (
                "char Input_MotorCommand(int port, int pos) "
                "{ return Input_controllerPorts[port].actuatorLevels[pos]; }",
                "char Input_MotorCommand(int port_index, int motor_index) "
                "{ return Input_controllerPorts[port_index]."
                "actuatorLevels[motor_index]; }",
            ),
            "Input::ResetVibrationPatterns": (
                "int i; for (i = 0; i < 2; ++i) slots[i].active = 0;",
                "int slot_index; for (slot_index = 0; slot_index < 2; "
                "++slot_index) slots[slot_index].active = 0;",
            ),
            "Input::RoundMotorIntensityTowardZero": (
                "int Round(int arg) { return arg < 0 ? arg += 0xFF : arg; }",
                "int Round(int intensity_q8) { return intensity_q8 < 0 ? "
                "intensity_q8 += 0xFF : intensity_q8; }",
            ),
            "Input::SetMotorCommand": (
                "void Input_SetMotorCommand(int port, int pos, int val) "
                "{ Input_controllerPorts[port].actuatorLevels[pos] = val; }",
                "void Input_SetMotorCommand(int port_index, int motor_index, "
                "int command) { Input_controllerPorts[port_index]."
                "actuatorLevels[motor_index] = command; }",
            ),
            "Input::StartVibrationPattern": (
                "int Start(char* arg0, u_int arg1, int arg2) "
                "{ int i = 0; int ret = arg1; slots[i].pattern = arg0; "
                "slots[i].ownerTag = arg2; return ret; }",
                "int Start(char* pattern, u_int slot_request, int owner_tag) "
                "{ int slot_index = 0; int slot_id = slot_request; "
                "slots[slot_index].pattern = pattern; "
                "slots[slot_index].ownerTag = owner_tag; return slot_id; }",
            ),
            "Input::StopVibrationPatterns": (
                "int Stop(u_int arg0, int arg1) "
                "{ int i; if (arg0 - 1 < 2) return Clear(arg0); "
                "for (i = 0; i < 2; ++i) "
                "if (slots[i].ownerTag == arg1) Clear(i + 1); return 1; }",
                "int Stop(u_int slot_selector, int owner_tag) "
                "{ int slot_cursor; if (slot_selector - 1 < 2) "
                "return Clear(slot_selector); "
                "for (slot_cursor = 0; slot_cursor < 2; ++slot_cursor) "
                "if (slots[slot_cursor].ownerTag == owner_tag) "
                "Clear(slot_cursor + 1); return 1; }",
            ),
            "Input::UpdateVibrationPatterns": (
                "int temp_a0, var_a2_2, i, j, var_v0, new_var; "
                "char* temp_t1; int (*a1)[6]; "
                "new_var = i != 0; a1[i][5] = temp_t1[temp_a0++]; "
                "slots[j].active = a1[i][0];",
                "int pattern_offset, var_a2_2, motor_index, slot_index, "
                "var_v0, interpolate_intensity; "
                "char* pattern; int (*motor_envelopes)[6]; "
                "interpolate_intensity = motor_index != 0; "
                "motor_envelopes[motor_index][5] = pattern[pattern_offset++]; "
                "slots[slot_index].active = motor_envelopes[motor_index][0];",
            ),
        }
        for action in ("Clear", "Pause", "Resume"):
            index = "i" if action == "Clear" else "v0"
            examples[f"Input::{action}VibrationSlot"] = (
                f"int Slot(int arg0) {{ int {index} = arg0 - 1; "
                f"slots[{index}].active = 0; return arg0; }}",
                "int Slot(int slot_id) { int slot_index = slot_id - 1; "
                "slots[slot_index].active = 0; return slot_id; }",
            )
        for action in ("Pause", "Resume"):
            examples[f"Input::{action}VibrationPatterns"] = (
                "int Select(int arg0) { int i; "
                "if ((arg0 - 1u) < 2) return Slot(arg0); "
                "for (i = 0; i < 2;) Slot(++i); return 1; }",
                "int Select(int slot_selector) { int slot_id; "
                "if ((slot_selector - 1u) < 2) return Slot(slot_selector); "
                "for (slot_id = 0; slot_id < 2;) Slot(++slot_id); return 1; }",
            )
        for name, (source, expected) in examples.items():
            with self.subTest(name=name):
                renamed = source
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        renamed = re.sub(pattern, replacement, renamed)
                self.assertEqual(renamed, expected)
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, "Content::Unrelated"):
                        source = re.sub(pattern, replacement, source)
                self.assertNotEqual(source, renamed)

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
            (
                "F02816",
                'char digits[2];\nsprintf(digits, "%02d", titleNo);',
                'char digits[sizeof "00"];',
                '"%02d"',
            ),
            (
                "F02817",
                'char digits[2];\nsprintf(digits, "%03d", enemyNo);',
                'char digits[sizeof "000"];',
                '"%03d"',
            ),
            (
                "F02820",
                'char sp60[12];\nsprintf(sp60, "NO.   %03d/%03d", selected + 1, 78);',
                'char sp60[sizeof "NO.   000/000"];',
                '"NO.   %03d/%03d"',
            ),
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
                    self.assertIn(
                        "AddPrim(Render_frameScratch.overlayOrderingTable, poly++);",
                        source,
                    )
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
                self.assertEqual(
                    owned[item]["header"], r"Native\Game\src\Field\DoorMatches.h"
                )
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
        for function, variable in (
            ("DrawFrame", "prim"),
            ("DrawSelectionBox", "temp_t0"),
        ):
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
                self.assertTrue(
                    f"char* {variable}[2 * {rows}];" in body,
                    f"{function} needs aligned native pointer storage",
                )
                self.assertNotIn(f"char** {variable};", body)
                self.assertNotRegex(body, rf"{variable}\s*=\s*.*0x(?:6A4|384|1324)")
                self.assertIn("Menu_Setup_BuildItemList(", body)
                self.assertIn("Core_Memory_FreeFromHighEnd(", body)

    def test_loot_staging_uses_last_slots_and_preflights_composites(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Loot/LootInventory.c").read_text()
        for fid in (
            "F02846",
            "F02847",
            "F02848",
            "F02849",
            "F02850",
            "F02851",
            "F02852",
        ):
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
        self.assertLess(
            append.index("if (index == 0)"), append.index("Menu_Loot_AppendLootRow(")
        )
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
        self.assertLess(
            cleanup.index("Menu_Loot_ResetPaging();"),
            cleanup.index("Core_Memory_FreeFromHighEnd("),
        )

    def test_misc_inventory_write_checks_capacity_before_mutation(self):
        old, new, _ = copy_fixups.FIXES["F02511"][0]
        root = Path(__file__).resolve().parents[2]
        source = (
            root / "Native" / "Game" / "src" / "Items" / "Inventory.c"
        ).read_text()
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
        self.assertTrue(
            new in source, "cone fix missing from generated particle emitter"
        )
        self.assertIn("scratch->vector.vx = scratch->vector.vz = -1;", new)
        self.assertIn("0x800D34BC/0x800D34F8", new)
        self.assertEqual(new.count("/ scratch->extent.vy;"), 2)
        self.assertNotIn("Core_Random_Rand", new)

    def test_particle_ribbon_preserves_original_zero_divisor_remainder(self):
        old, new, _ = copy_fixups.FIXES["F01867"][0]
        root = Path(__file__).resolve().parents[2]
        source = (
            root / "Native/Game/src/Effects/Templates/ParticleRender.inl"
        ).read_text()
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
        declaration = "static char Menu_Inventory_miscItemShortcutMisc[] = " + old + ";"
        encoded = declaration.replace(old, new)
        self.assertIn(r'"\x16\x12\x1C\x0C\xE7\x00\x00\xF8"', encoded)
        self.assertNotIn("MISC", encoded)
        self.assertNotIn("|0xF8|", encoded)

    def test_generated_label_and_reload_initializer_both_terminate(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Inventory/Inventory.c").read_text()
        label = r'"\x16\x12\x1C\x0C\xE7\x00\x00\xF8"'
        self.assertIn(
            "static char Menu_Inventory_miscItemShortcutMisc[] = " + label, source
        )
        self.assertIn("static const char initial[] = " + label, source)
        self.assertNotIn(r'"MISC\00000|0xF8|"', source)

    def test_title_settings_retains_original_defaults_in_shared_helper(self):
        old, new, _ = copy_fixups.FIXES["F03669"][0]
        self.assertIn("Input_settings.menuFlags = 0x2D8;", old)
        self.assertEqual(new.strip(), "Boot_InitializePlayerSettings();")
        self.assertIn(
            "Boot_InitializePlayerSettings", copy_fixups.NATIVE_HELPER_HEADERS
        )

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
        table = source.split(
            "Effects_EffectRenderer Effects_effectEntryPoints[] = {", 1
        )[1]
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
