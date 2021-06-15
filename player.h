#ifndef G_PLAYER_H
#define G_PLAYER_H

typedef struct Player{
    Camera camera;
    float speed;
} Player;

//Initialize a player.
void Player_Init(Player *player);

//Update a player.
void Player_Update(Player *player);

#endif