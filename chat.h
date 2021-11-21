/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_CHAT_H
#define G_CHAT_H

extern bool Chat_open;

void Chat_AddLine(char *line);
void Chat_Draw(Vector2 offset, Color uiColor);

#endif