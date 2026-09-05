#include <string.h>
#include "entitymodeldefinition.h"
bool ModelDefinition_Validate(int id, const ModelDefinition *d) {
    if (!d || id < 1 || id > 255 || !d->name[0] || !memchr(d->name, 0, 65) ||
        d->texture > 1 || !d->partCount || d->partCount > ENTITY_MODEL_MAX_PARTS) return false;
    for (int i = 0; i < d->partCount; i++) {
        const ModelPartDefinition *p = &d->parts[i];
        if (p->role > 5 || p->firstPersonVisible > 1) return false;
        for (int a = 0; a < 3; a++) if (p->min[a] >= p->max[a]) return false;
        for (int f = 0; f < 6; f++) {
            if (!p->uv[f][2] || !p->uv[f][3]) return false;
            if (p->uv[f][0] < 0 || p->uv[f][1] < 0 ||
                p->uv[f][0] + p->uv[f][2] < 0 || p->uv[f][1] + p->uv[f][3] < 0) return false;
        }
    }
    return true;
}
