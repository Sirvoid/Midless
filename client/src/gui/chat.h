/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_CHAT_H
#define MIDLESS_CLIENT_CHAT_H

extern bool chatOpen;

//Add a line and take ownership.
void Chat_AddOwnedLine(char *line);
void Chat_Draw(Vector2 offset, Color uiColor);
void Chat_Shutdown(void);

#endif