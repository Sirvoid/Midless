#ifndef MIDLESS_SERVER_DIGGING_H
#define MIDLESS_SERVER_DIGGING_H
#include "player.h"

typedef struct BlockDigging {
    double hardness;
    bool unbreakable;
    char group[65];
} BlockDigging;

void ServerDigging_Reset(void);
void ServerDigging_Define(int id, const BlockDigging *definition);
double ServerDigging_Time(Player *player, Vector3 position, int block, ItemStack stack);
#endif
