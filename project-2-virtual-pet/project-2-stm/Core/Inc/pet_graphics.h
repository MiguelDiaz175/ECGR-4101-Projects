#ifndef PET_GRAPHICS_H
#define PET_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>
#include "pet.h"

/* all the guys animations */
typedef enum {
    PETGFX_ANIM_IDLE,      /* happy, bobbing, (can add blinking after test run)     */
    PETGFX_ANIM_EATING,    /* one-shot reaction after Pet_Feed()       */
    PETGFX_ANIM_SCARED,    /* one-shot scare or persistent darkness mood */
    PETGFX_ANIM_SAD,       /* hunger or happiness is getting low       */
    PETGFX_ANIM_SLEEPING,  /* reserved sleeping animation             */
    PETGFX_ANIM_DEAD,      /* he gone...              */
    PETGFX_ANIM_RAN_AWAY,  /* happiness hit 0; it uses dead fame currently. If time, create its own frames for running away */
    PETGFX_ANIM_COUNT      /* number of animations, not an actual animation */
} PetGfxAnim;

/* Draw the first frame. Call after the pet screen background is drawn
 * (i.e. at the end of PetScreen_Init). */
void PetGfx_Init(uint32_t now);

/* Call every pass of the main loop while playing. Picks the animation from
 * the pet's stats, advances frames on a timer and redraws only on change.
 * Never blocks. */
void PetGfx_Update(Pet *pet, uint32_t now);

/* Play a short reaction (EATING, SCARED) that overrides the mood
 * animation for durationMs. */
void PetGfx_PlayReaction(PetGfxAnim anim, uint32_t now, uint32_t durationMs);

/* Slide the pet by (dx, dy) pixels from its home spot, e.g. to follow the
 * accelerometer tilt. Keep dx within +/-48 and dy within +/-20 so the pet
 * stays on screen and above the stat bars. */
void PetGfx_SetTilt(int16_t dx, int16_t dy);

/* Keep the scared animation active while the pet is in darkness. */
void PetGfx_SetScared(bool isScared);

/* Enable or disable the reserved sleeping animation. */
void PetGfx_SetSleeping(bool sleeping);

#endif
