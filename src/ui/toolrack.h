/* toolrack.h — the survey tool rack, variant B.
 *
 * A 1:1 port of js/dashboard.html lines 21-712 against
 * docs/CANVAS2D_PORT_SPEC.md. Every draw goes through c2d.h; nothing here
 * calls raylib directly. The gap inventory the spec requires is
 * docs/design/prospecting/toolrack-inventory.md.
 *
 * Variant B only -- the dashboard never calls drawRack/drawScene. See the
 * inventory's scope note.
 *
 *   ToolRackData rack = ToolRack_Demo();
 *   ToolRack_DrawB(&rack, x, y, NULL);
 *   int slot = ToolRack_HitTestB(px - x, py - y, rack.slots);
 */
#ifndef TOOLRACK_H
#define TOOLRACK_H

#include "c2d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TR_SLOTS_MAX 8

/* `icon` indexes ICONS in the JS, which is a string key. An enum here: the
 * set is closed and a typo should not silently draw nothing. */
typedef enum TRIcon {
    TR_ICON_NONE = 0,
    TR_ICON_DRILL_STRIPED,
    TR_ICON_SEISMIC_WIDE,
    TR_ICON_SONAR,
    TR_ICON_ROVER,          /* defined in variant A, used by variant B's data */
} TRIcon;

typedef struct ToolRackTool {
    bool        present;    /* a null entry in the JS array               */
    const char *name;
    const char *type;
    TRIcon      icon;
    bool        active;
    bool        selected;

    /* crossfade (dashboard.html:95): while a slot changes state the previous
     * state is drawn underneath and the new one faded in over it. fade < 0
     * means "not transitioning". */
    float       fade;
    bool        prevActive;
    bool        prevSelected;
} ToolRackTool;

typedef struct ToolRackData {
    const char  *header;
    int          slots;
    ToolRackTool tools[TR_SLOTS_MAX];
} ToolRackData;

typedef struct ToolRackOpts {
    int  level;        /* 1..6 as the JS; 6 is everything. 0 -> 6         */
    bool grain;        /* level >= 6 only, and off for the visual diff    */
} ToolRackOpts;

/* The rack's own extent in design units, for callers placing it. */
#define TR_W 395.0f
#define TR_H 750.0f

void ToolRack_DrawB(const ToolRackData *rack, float x, float y,
                    const ToolRackOpts *opts);

/* (x, y) in rack-local units -> slot index, or -1. */
int  ToolRack_HitTestB(float x, float y, int slots);

/* The dashboard's own rack contents (dashboard.html:1634), so the port can
 * be rendered against the reference without inventing data. */
ToolRackData ToolRack_Demo(void);

#ifdef __cplusplus
}
#endif

#endif /* TOOLRACK_H */
