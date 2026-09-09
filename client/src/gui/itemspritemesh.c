#include "itemspritemesh.h"
#include <string.h>

static void Face(Mesh *mesh, int *vertex, Vector3 a, Vector3 b, Vector3 c, Vector3 d,
    Vector2 uvA, Vector2 uvB, Vector2 uvC, Vector2 uvD, unsigned char shade) {
    Vector3 points[] = {a,b,c,a,c,d};
    Vector2 uvs[] = {uvA,uvB,uvC,uvA,uvC,uvD};
    for (int i=0;i<6;i++,(*vertex)++) {
        memcpy(mesh->vertices + *vertex*3, &points[i], sizeof(Vector3));
        memcpy(mesh->texcoords + *vertex*2, &uvs[i], sizeof(Vector2));
        unsigned char *color = mesh->colors + *vertex*4;
        color[0]=color[1]=color[2]=shade; color[3]=255;
    }
}
static bool Opaque(Color *pixels, int w, int h, int x, int y) {
    return x>=0 && y>=0 && x<w && y<h && pixels[y*w+x].a>=128;
}
Mesh ItemSprite_CreateMesh(Image image) {
    Mesh mesh = {0};
    int w=image.width, h=image.height, faces=2;
    Color *pixels=LoadImageColors(image);
    if (!pixels || w>64 || h>64) { if (pixels) UnloadImageColors(pixels); return mesh; }
    const int dx[]={-1,1,0,0}, dy[]={0,0,-1,1};
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) if (Opaque(pixels,w,h,x,y))
        for (int side=0;side<4;side++) if (!Opaque(pixels,w,h,x+dx[side],y+dy[side])) faces++;
    mesh.vertexCount=faces*6; mesh.triangleCount=faces*2;
    mesh.vertices=MemAlloc(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords=MemAlloc(mesh.vertexCount*2*sizeof(float));
    mesh.colors=MemAlloc(mesh.vertexCount*4);
    if (!mesh.vertices || !mesh.texcoords || !mesh.colors) { UnloadMesh(mesh); UnloadImageColors(pixels); return (Mesh){0}; }
    float pixel=1.0f/(w>h?w:h), left=-w*pixel/2, top=h*pixel/2, z=pixel/2;
    int vertex=0;
    for (int back=0;back<2;back++) {
        float depth=back?-z:z;
        Face(&mesh,&vertex,(Vector3){left,top,depth},(Vector3){-left,top,depth},
            (Vector3){-left,-top,depth},(Vector3){left,-top,depth},
            (Vector2){0,0},(Vector2){1,0},(Vector2){1,1},(Vector2){0,1},255);
    }
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) if (Opaque(pixels,w,h,x,y)) {
        float x0=left+x*pixel,x1=x0+pixel,y0=top-y*pixel,y1=y0-pixel;
        Vector2 uv={(x+0.5f)/w,(y+0.5f)/h};
        for (int side=0;side<4;side++) if (!Opaque(pixels,w,h,x+dx[side],y+dy[side])) {
            Vector3 a,b;
            if (side<2) { float edge=side?x1:x0; a=(Vector3){edge,y0,z}; b=(Vector3){edge,y1,z}; }
            else { float edge=side==2?y0:y1; a=(Vector3){x0,edge,z}; b=(Vector3){x1,edge,z}; }
            Vector3 c=b,d=a; c.z=d.z=-z;
            Face(&mesh,&vertex,a,b,c,d,uv,uv,uv,uv,side==2?240:180);
        }
    }
    UnloadImageColors(pixels); return mesh;
}
