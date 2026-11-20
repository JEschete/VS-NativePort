// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Actors/Spawning.hpp"

namespace VS::Actors {
// decomp: D_800FAB18 (D_800FAB18@src/BATTLE/INITBTL.PRG/18.c)
CharacterInit
    ashleyInitialLoadout =
        {
            .sub_type = 174,
            .reach = 7,
            .name = {"\x0A\x36\x2B\x2F\x28\x3C\xFA\x06\x1B\x2C\x32\x37\xE7"},
            .hp = 250,
            .mp = 50,
            .strength = 100,
            .intelligence = 100,
            .agility = 100,
            .recovery_delay_bonus = 1,
            .material = 2,
            .walk_speed = 10,
            .gait_class = 4,
            .run_speed = 20,
            .mobility_limbs = {4},
            .default_target_limb = 3,
            .weapon =
                {
                    .blade =
                        {
                            .id = 14,
                            .sub_id = 14,
                            .weapon_model_id = 16,
                            .category = 2,
                            .strength = 9,
                            .intelligence = 1,
                            .agility = -3,
                            .current_dp = 12600,
                            .max_dp = 12600,
                            .max_pp = 136,
                            .damage_type = 2,
                            .cost_type = 2,
                            .cost = 1,
                            .range =
                                {
                                    .range = 3,
                                    .second_range = 2,
                                    .third_range = 3,
                                    .shape_bits = 1,
                                },
                            .classes = {-1, -1, -2, -1, -1, -5},
                            .affinities = {8, 5, 5, -3, -3, 2, 2, 0},
                        },
                    .grip =
                        {
                            .id = 96,
                            .sub_id = 1,
                            .category = 1,
                            .strength = 1,
                            .agility = -1,
                            .types = {0, 0, 4, 1},
                        },
                    .material = 3,
                    .name = {"\x0F\x24\x31\x27\x24\x31\x2A\x32\xE7"},
                },
            .accessory =
                {
                    .equipment =
                        {
                            .id = 223,
                            .sub_id = 97,
                            .category = 7,
                            .intelligence = 1,
                            .classes = {0, 0, 5, -5, -5, 5},
                            .affinities = {5, 0, 0, 0, 0, 5, -5, 0},
                        },
                },
            .limbs =
                {{
                    {
                        .hp = 200,
                        .actions =
                            {{
                                {.action_id = 1},
                                {.index = 1},
                                {.index = 2},
                                {.index = 3},
                            }},
                        .armor =
                            {
                                .equipment =
                                    {
                                        .id = 191,
                                        .sub_id = 65,
                                        .category = 5,
                                        .strength = 1,
                                        .intelligence = 13,
                                        .current_dp = 3350,
                                        .max_dp = 3350,
                                        .types = {0, 1, 0, 0},
                                        .affinities = {2, 5, 5,
                                                       -1, -1, -5, -5, 0},
                                    },
                                .material = 2,
                                .opaque32 = 1,
                            },
                        .dp_splash = {100, 0, 0, 5, 0, 0},
                    },
                    {
                        .hp = 200,
                        .actions =
                            {{
                                {.action_id = 1},
                                {.index = 1},
                                {.index = 2},
                                {.index = 3},
                            }},
                        .armor =
                            {
                                .equipment =
                                    {
                                        .id = 191,
                                        .sub_id = 65,
                                        .category = 5,
                                        .strength = 1,
                                        .intelligence = 13,
                                        .current_dp = 3350,
                                        .max_dp = 3350,
                                        .types = {0, 1, 0, 0},
                                        .affinities = {2, 5, 5,
                                                       -1, -1, -5, -5, 0},
                                    },
                                .material = 2,
                                .opaque32 = 1,
                            },
                        .dp_splash = {0, 100, 0, 5, 0, 0},
                    },
                    {
                        .hp = 200,
                        .actions =
                            {{
                                {0},
                                {.index = 1},
                                {.index = 2},
                                {.index = 3},
                            }},
                        .armor =
                            {
                                .equipment =
                                    {
                                        .id = 143,
                                        .sub_id = 17,
                                        .category = 2,
                                        .strength = 1,
                                        .intelligence = 8,
                                        .current_dp = 3800,
                                        .max_dp = 3800,
                                        .affinities = {2, 5, 5,
                                                       -1, -1, -5, -5, 0},
                                    },
                                .material = 2,
                                .opaque32 = 1,
                            },
                        .dp_splash = {0, 0, 100, 10, 0, 0},
                    },
                    {
                        .hp = 200,
                        .actions =
                            {{
                                {0},
                                {.index = 1},
                                {.index = 2},
                                {.index = 3},
                            }},
                        .armor =
                            {
                                .equipment =
                                    {
                                        .id = 159,
                                        .sub_id = 33,
                                        .category = 3,
                                        .strength = 5,
                                        .intelligence = 10,
                                        .current_dp = 5500,
                                        .max_dp = 5500,
                                        .types = {0, 1, 1, 0},
                                        .affinities = {2, 5, 5,
                                                       -1, -1, -5, -5, 0},
                                    },
                                .material = 2,
                                .opaque32 = 1,
                            },
                        .dp_splash = {5, 5, 5, 100, 5, 0},
                    },
                    {
                        .hp = 200,
                        .actions =
                            {{
                                {0},
                                {.index = 1},
                                {.index = 2},
                                {.index = 3},
                            }},
                        .armor =
                            {
                                .equipment =
                                    {
                                        .id = 175,
                                        .sub_id = 49,
                                        .category = 4,
                                        .strength = 1,
                                        .intelligence = 12,
                                        .current_dp = 4050,
                                        .max_dp = 4050,
                                        .affinities = {2, 5, 5,
                                                       -1, -1, -5, -5, 0},
                                    },
                                .material = 2,
                                .opaque32 = 1,
                            },
                        .dp_splash = {0, 0, 0, 5, 100, 0},
                    },
                    {
                        .actions =
                            {{
                                {0},
                                {.index = 1},
                                {.index = 2},
                                {.index = 3},
                            }},
                    },
                }},
};

// decomp: D_8004FDD0 (D_8004FDD0@SLUS_010.40)
std::array<char, kPlaceholderActorNameBytes> placeholderActorName; /* contents: module image (ModuleData.cpp) */

// decomp: D_8004FE88 (D_8004FE88@SLUS_010.40)
std::array<std::uint8_t, kBodyModelIds> usesSecondaryBodyModel; /* contents: module image (ModuleData.cpp) */
}  // namespace VS::Actors

