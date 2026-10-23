"""Regressions for encounter and effect source adaptations, without game assets."""

import unittest
from pathlib import Path

import copy_fixups


class EncounterFixupTests(unittest.TestCase):
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
