#ifndef G_PLAYER_H
#define G_PLAYER_H

typedef struct {
    Camera camera;
} Player;

void Player_Init(Player *player);

#endif