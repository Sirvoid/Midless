#ifndef G_PLAYER_H
#define G_PLAYER_H

typedef struct Player{
    Camera camera;
} Player;

void Player_Init(Player *player);
void Player_Update(Player *player);

#endif