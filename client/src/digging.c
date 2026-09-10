#include "digging.h"
#include "inventoryclient.h"
#include "textures.h"
#include "networking/packet.h"
#include "block/block.h"
#include "world.h"
#include "rlgl.h"
#include "blockshape.h"

static int textureId;
static Texture2D fallback;
void Digging_HandleTexture(void) { if (packetDataLength==2) textureId=packetData[1]; }
void Digging_Reset(void) {
    textureId=0;
    if (fallback.id) UnloadTexture(fallback);
    fallback=(Texture2D){0};
}
static Texture2D BreakingTexture(void) {
    if (textureId) return ClientTextures_Get(textureId);
    if (!fallback.id) {
        Image image=GenImageColor(160,16,BLANK);
        // A small default atlas; mods can replace all ten frames with a PNG.
        for (int stage=0;stage<10;stage++) {
            for (int branch=0;branch<=stage;branch++) {
                int x=8,y=8;
                for (int step=0;step<3+stage;step++) {
                    ImageDrawPixel(&image,stage*16+x,y,(Color){15,15,15,200});
                    x+=(branch%3)-1; y+=((branch/3)%3)-1;
                    if (x<0 || x>15 || y<0 || y>15) break;
                }
            }
        }
        fallback=LoadTextureFromImage(image); UnloadImage(image);
        SetTextureFilter(fallback,TEXTURE_FILTER_POINT);
    }
    return fallback;
}
void Digging_Draw(void) {
    Vector3 position;
    float progress=ClientInventory_DigProgress(&position);
    if (progress<0) return;
    Texture2D texture=BreakingTexture();
    if (!texture.id) return;
    int block=World_GetBlock(position);
    if (block<=0) return;
    const Block *definition=Block_GetDefinition(block);
    for(int box=0;box<Block_BoxCount(definition,true);box++) {
        BoundingBox bounds=Block_GetBox(definition,box,position,true);
        Vector3 a=bounds.min, b=bounds.max;
        float e=0.002f;
        a.x-=e; a.y-=e; a.z-=e; b.x+=e; b.y+=e; b.z+=e;
        Vector3 corners[]={ {a.x,a.y,a.z},{b.x,a.y,a.z},{b.x,b.y,a.z},{a.x,b.y,a.z},
            {a.x,a.y,b.z},{b.x,a.y,b.z},{b.x,b.y,b.z},{a.x,b.y,b.z} };
        const int faces[6][4]={{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};
        int stage=(int)(progress*10); if (stage>9) stage=9;
        float u=stage/10.0f,v=(stage+1)/10.0f;
        const float uv[4][2]={{u,1},{v,1},{v,0},{u,0}};
        rlDrawRenderBatchActive(); rlDisableDepthMask(); rlDisableBackfaceCulling();
        rlSetTexture(texture.id); rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
        for (int face=0;face<6;face++) for (int point=0;point<4;point++) {
            Vector3 vertex=corners[faces[face][point]];
            rlTexCoord2f(uv[point][0],uv[point][1]); rlVertex3f(vertex.x,vertex.y,vertex.z);
        }
        rlEnd(); rlSetTexture(0); rlDrawRenderBatchActive();
        rlEnableBackfaceCulling(); rlEnableDepthMask();
    }
}
