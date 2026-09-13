/* survey_dash.h — the survey console, assembled from the ported modules.
 *
 * Design space is 1536x1024 (docs/PORT_PROMPTS.md), letterboxed onto whatever
 * region the game gives it by c2d_present. Placement and camera come from
 * js/dashboard.html's own DASH.layout, so the console is laid out exactly
 * where the reference puts it.
 *
 * INCOMPLETE BY DESIGN: only ToolRack and Holo3D are ported. The Dashboard
 * chrome around them -- panels, the layers blurb, the drill bar, the stats
 * blocks and the log -- is js/dashboard.html 1290-1670 and is not ported yet,
 * so the space it would occupy is empty.
 */
#ifndef SURVEY_DASH_H_INCLUDED
#define SURVEY_DASH_H_INCLUDED

#include "raylib.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SURVEY_DASH_DESIGN_W 1536
#define SURVEY_DASH_DESIGN_H 1024

/* Idempotent; safe to call every frame. Returns false if the GL resources
 * could not be created, in which case Draw is a no-op. */
bool SurveyDash_Init(void);
void SurveyDash_Shutdown(void);

/* Draws into the design surface and blits it, letterboxed, into `region`. */
void SurveyDash_Draw(Rectangle region, float dt);

/* Screen -> design space, honouring the same letterbox Draw used. */
Vector2 SurveyDash_ToDesign(Rectangle region, Vector2 screenPt);

/* Input, in SCREEN coordinates -- the conversion happens inside, because
 * hit-testing in screen space is the mistake the spec calls out by name. */
void SurveyDash_Press  (Rectangle region, Vector2 screenPt);
void SurveyDash_Drag   (Rectangle region, Vector2 delta);
void SurveyDash_Release(Rectangle region, Vector2 screenPt);

#ifdef __cplusplus
}
#endif

#endif /* SURVEY_DASH_H_INCLUDED */
