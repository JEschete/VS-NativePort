"""Regressions for encounter-entry source adaptations, without game assets."""

import unittest
from pathlib import Path

import copy_fixups


class EncounterFixupTests(unittest.TestCase):
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
