/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include "entitymodeldefinition.h"
#include "textureprotocol.h"
bool ModelDefinition_Validate(int id, const ModelDefinition *d) {
    if (!d || id < 1 || id > 255 || !d->name[0] || !memchr(d->name, 0, 65) ||
        d->texture >= TEXTURE_LIMIT || !d->partCount || d->partCount > ENTITY_MODEL_MAX_PARTS) return false;
    for (int i = 0; i < d->partCount; i++) {
        const ModelPartDefinition *p = &d->parts[i];
        if (p->role > 5 || p->firstPersonVisible > 1 || p->hasGrip > 1) return false;
        for (int a = 0; a < 3; a++) if (p->min[a] >= p->max[a]) return false;
        for (int f = 0; f < 6; f++) {
            if (!p->uv[f][2] || !p->uv[f][3]) return false;
            if (p->uv[f][0] < 0 || p->uv[f][1] < 0 ||
                p->uv[f][0] + p->uv[f][2] < 0 || p->uv[f][1] + p->uv[f][3] < 0) return false;
        }
    }
    return true;
}

ModelDefinition ModelDefinition_Humanoid(void) {
    return (ModelDefinition){.name="Humanoid",.texture=0,.partCount=6,.parts={
        {.role=1,.firstPersonVisible=0,.position={0,1216,0},.min={-256,-32,-256},.max={256,480,192},
         .uv={{30,14,14,16},{0,14,14,16},{30,14,-16,-14},{46,0,-16,14},{14,14,16,16},{44,14,16,16}}},
        {.role=0,.firstPersonVisible=0,.position={26,1171,-26},.min={-250,-531,-102},.max={198,45,90},
         .uv={{20,36,6,18},{0,36,6,18},{20,36,-14,-6},{34,30,-14,6},{6,36,14,18},{26,36,14,18}}},
        {.role=2,.firstPersonVisible=1,.position={-224,1120,0},.min={-179,-576,-128},.max={19,64,64},
         .uv={{58,36,-6,20},{46,36,-6,20},{46,36,6,-6},{52,30,6,6},{52,36,-6,20},{64,36,-6,20}}},
        {.role=3,.firstPersonVisible=0,.position={224,1120,0},.min={-19,-576,-128},.max={179,64,64},
         .uv={{82,36,-6,20},{70,36,-6,20},{70,36,6,-6},{76,30,6,6},{76,36,-6,20},{88,36,-6,20}}},
        {.role=4,.firstPersonVisible=0,.position={-90,550,0},.min={-102,-550,-128},.max={90,90,64},
         .uv={{96,6,6,20},{84,6,6,20},{96,6,-6,-6},{102,0,-6,6},{90,6,6,20},{102,6,6,20}}},
        {.role=5,.firstPersonVisible=0,.position={102,550,0},.min={-102,-550,-128},.max={90,90,64},
         .uv={{72,6,6,20},{60,6,6,20},{72,6,-6,-6},{78,0,-6,6},{66,6,6,20},{78,6,6,20}}}
    }};
}
