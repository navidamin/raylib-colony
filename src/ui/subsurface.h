/* subsurface.h — how deep the ground is.
 *
 * ONE NUMBER FOR THE WHOLE GAME. It is not the drill's and it is not the
 * knowledge model's; both read it, and so does the block. Before this it was
 * written down three times -- 120 m in the drill's strata, 120 m in
 * prospecting's layer table, and 2000 m in the ground the block is generated
 * from -- and the knowledge model's depth term was calibrated against the
 * third while being run on the first.
 *
 * The value is prospecting's, because that is the column the game already
 * prices, gates and cores: LAYER_THICKNESS_M sums to 120 m, and the drill's
 * four strata sit on exactly those boundaries. survey_constants.h derives the
 * same number from that table and static_asserts it against this one, so the
 * two cannot drift.
 *
 * Anything measured DOWN is expressed as a fraction of this, never as an
 * absolute: a constant in metres is a constant that silently means something
 * different when the column changes.
 */
#ifndef SUBSURFACE_H
#define SUBSURFACE_H

#define SUB_COLUMN_M 120.0f

#endif /* SUBSURFACE_H */
