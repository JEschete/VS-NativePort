#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_NAVIGATIONPRIMITIVES_H
#define VS_OWNED_BATTLE_AI_NAVIGATIONPRIMITIVES_H
#ifdef __cplusplus
extern "C" {
#endif

void Battle_Ai_ReserveAiDestinationTile(int, int);

int Battle_Ai_IsAdjacentTileStepBlocked(unsigned int, int);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_NAVIGATIONPRIMITIVES_H
