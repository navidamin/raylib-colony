#include "web_mouse.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

Vector2 ColonyGetMousePosition()
{
#ifdef __EMSCRIPTEN__
    float x = 0.0f;
    float y = 0.0f;
    int published = 0;

    // One call rather than one per component: this runs for every hit-test in
    // every panel, every frame. `published` is 0 with an older cached shell,
    // and on the frames before the first pointer event.
    EM_ASM({
        var m = window.__colonyMouse;
        if (m)
        {
            setValue($0, m.x, 'float');
            setValue($1, m.y, 'float');
            setValue($2, 1, 'i32');
        }
    }, &x, &y, &published);

    if (published) return Vector2{ x, y };
#endif

    return GetMousePosition();
}

bool ColonyMouseOver(Rectangle area)
{
    return CheckCollisionPointRec(ColonyGetMousePosition(), area);
}
