#include "pet_graphics.h"
#include "pet_sprites.h"
#include "ili9341.h"
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Sprites
 * ---------------------------------------------------------------------------
 * The frames are the team's hand-drawn artwork (docs/artwork/masters),
 * converted to RGB565 arrays in pet_sprites.c by tools/sprite_convert.py.
 * Each frame is drawn with ILI9341_DrawRGB565Bitmap. The sprite's own
 * background pixels are the same color as AREA_BG, so no masking is needed.
 * ------------------------------------------------------------------------- */

/* Pet area: everything above the HUNGER label (y = 218). */
#define AREA_BG       ILI9341_COLOR_BLACK
#define HOME_X        ((ILI9341_WIDTH - PET_SPRITE_W) / 2U)   /* 56 */
#define HOME_Y        50U   /* lowest frame offset (+8) still ends above y = 218 */

/* Right of the sprite box, so redrawing the sprite never erases the Zs. */
#define ZZZ_X         (HOME_X + PET_SPRITE_W + 2U)   /* 186 */
#define ZZZ_Y         (HOME_Y + 10U)

/* One animation frame = which bitmap + where it sits. Offsets let a single
 * drawing bob, shake or float; add more drawings (idle_2.png, ...) to give
 * each animation real frame-to-frame changes. */
typedef struct {
    const uint16_t *bitmap;
    int8_t dx;   /* LCD pixel offset from HOME_X */
    int8_t dy;   /* LCD pixel offset from HOME_Y */
} PetFrame;

typedef struct {
    const PetFrame *frames;
    uint8_t frameCount;
    uint16_t frameMs;    /* how long each frame stays on screen */
} PetAnimation;

static const PetFrame IDLE_FRAMES[] = {
    { PET_SPR_IDLE_1, 0, 0 },
    { PET_SPR_IDLE_1, 0, 4 },
};
static const PetFrame EATING_FRAMES[] = {
    { PET_SPR_EAT_1, 0, 0 },
    { PET_SPR_EAT_1, 0, 5 },
};
static const PetFrame SCARED_FRAMES[] = {
    { PET_SPR_SCARED_1, -5, 0 },
    { PET_SPR_SCARED_1,  5, 0 },
};
static const PetFrame SAD_FRAMES[] = {
    { PET_SPR_SAD_1, 0, 6 },
    { PET_SPR_SAD_1, 0, 8 },
};
static const PetFrame SLEEPING_FRAMES[] = {
    { PET_SPR_SLEEP_1, 0, 6 },
    { PET_SPR_SLEEP_1, 0, 4 },
};
static const PetFrame DEAD_FRAMES[] = {
    { PET_SPR_DEAD_1, 0, 0 },
    { PET_SPR_DEAD_1, 0, -6 },
};

#define COUNT_OF(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

/* Indexed by PetGfxAnim. */
static const PetAnimation ANIMATIONS[PETGFX_ANIM_COUNT] = {
    [PETGFX_ANIM_IDLE]     = { IDLE_FRAMES,     COUNT_OF(IDLE_FRAMES),     500U },
    [PETGFX_ANIM_EATING]   = { EATING_FRAMES,   COUNT_OF(EATING_FRAMES),   200U },
    [PETGFX_ANIM_SCARED]   = { SCARED_FRAMES,   COUNT_OF(SCARED_FRAMES),   100U },
    [PETGFX_ANIM_SAD]      = { SAD_FRAMES,      COUNT_OF(SAD_FRAMES),      700U },
    [PETGFX_ANIM_SLEEPING] = { SLEEPING_FRAMES, COUNT_OF(SLEEPING_FRAMES), 900U },
    [PETGFX_ANIM_DEAD]     = { DEAD_FRAMES,     COUNT_OF(DEAD_FRAMES),     600U },
};

/* Below this, the pet looks sad. */
#define LOW_STAT_THRESHOLD 30

/* ---------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */
static PetGfxAnim currentAnim = PETGFX_ANIM_IDLE;
static uint8_t frameIndex = 0U;
static uint32_t lastFrameTime = 0U;

static PetGfxAnim reactionAnim = PETGFX_ANIM_IDLE;
static uint32_t reactionEnd = 0U;
static bool reactionActive = false;

static bool sleeping = false;
static bool zzzShown = false;

/* What is on the LCD right now, so we only redraw when something changed. */
static const PetFrame *drawnFrame = NULL;
static int16_t drawnX = -1;
static int16_t drawnY = -1;

/* ---------------------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------------------- */

/* Erase the strips the sprite moved away from, so it doesn't leave a trail. */
static void EraseVacated(int16_t newX, int16_t newY)
{
    if (drawnX < 0)
    {
        return;
    }

    if (newX > drawnX)
    {
        ILI9341_FillRect((uint16_t)drawnX, (uint16_t)drawnY,
                         (uint16_t)(newX - drawnX), PET_SPRITE_H, AREA_BG);
    }
    else if (newX < drawnX)
    {
        ILI9341_FillRect((uint16_t)(newX + (int16_t)PET_SPRITE_W), (uint16_t)drawnY,
                         (uint16_t)(drawnX - newX), PET_SPRITE_H, AREA_BG);
    }

    if (newY > drawnY)
    {
        ILI9341_FillRect((uint16_t)drawnX, (uint16_t)drawnY,
                         PET_SPRITE_W, (uint16_t)(newY - drawnY), AREA_BG);
    }
    else if (newY < drawnY)
    {
        ILI9341_FillRect((uint16_t)drawnX, (uint16_t)(newY + (int16_t)PET_SPRITE_H),
                         PET_SPRITE_W, (uint16_t)(drawnY - newY), AREA_BG);
    }
}

static void DrawZzz(bool show)
{
    if (show == zzzShown)
    {
        return;
    }

    ILI9341_DrawString(ZZZ_X, ZZZ_Y, show ? "Z" : " ",
                       ILI9341_COLOR_CYAN, AREA_BG, 3);
    ILI9341_DrawString(ZZZ_X + 20U, ZZZ_Y - 18U, show ? "Z" : " ",
                       ILI9341_COLOR_CYAN, AREA_BG, 2);
    zzzShown = show;
}

static void Render(void)
{
    const PetAnimation *anim = &ANIMATIONS[currentAnim];
    const PetFrame *frame = &anim->frames[frameIndex];
    int16_t x = (int16_t)HOME_X + frame->dx;
    int16_t y = (int16_t)HOME_Y + frame->dy;

    if ((frame == drawnFrame) && (x == drawnX) && (y == drawnY))
    {
        return;   /* nothing changed: don't touch the LCD */
    }

    EraseVacated(x, y);
    ILI9341_DrawRGB565Bitmap((uint16_t)x, (uint16_t)y,
                             PET_SPRITE_W, PET_SPRITE_H, frame->bitmap);
    DrawZzz(currentAnim == PETGFX_ANIM_SLEEPING);

    drawnFrame = frame;
    drawnX = x;
    drawnY = y;
}

/* ---------------------------------------------------------------------------
 * Animation logic
 * ------------------------------------------------------------------------- */
static PetGfxAnim ChooseAnimation(Pet *pet, uint32_t now)
{
    if (Pet_GetState(pet) == PET_DEAD)
    {
        return PETGFX_ANIM_DEAD;
    }

    if (reactionActive)
    {
        if ((int32_t)(now - reactionEnd) < 0)
        {
            return reactionAnim;
        }
        reactionActive = false;
    }

    if (sleeping)
    {
        return PETGFX_ANIM_SLEEPING;
    }

    if ((Pet_GetHunger(pet) < LOW_STAT_THRESHOLD) ||
        (Pet_GetHappiness(pet) < LOW_STAT_THRESHOLD))
    {
        return PETGFX_ANIM_SAD;
    }

    return PETGFX_ANIM_IDLE;
}

void PetGfx_Init(uint32_t now)
{
    currentAnim = PETGFX_ANIM_IDLE;
    frameIndex = 0U;
    lastFrameTime = now;
    reactionActive = false;
    zzzShown = false;

    /* The screen was just cleared, so forget what we think is drawn. */
    drawnFrame = NULL;
    drawnX = -1;
    drawnY = -1;

    Render();
}

void PetGfx_Update(Pet *pet, uint32_t now)
{
    PetGfxAnim wanted = ChooseAnimation(pet, now);

    if (wanted != currentAnim)
    {
        currentAnim = wanted;
        frameIndex = 0U;
        lastFrameTime = now;
    }
    else if ((now - lastFrameTime) >= ANIMATIONS[currentAnim].frameMs)
    {
        lastFrameTime = now;
        frameIndex = (uint8_t)((frameIndex + 1U) %
                               ANIMATIONS[currentAnim].frameCount);
    }

    Render();
}

void PetGfx_PlayReaction(PetGfxAnim anim, uint32_t now, uint32_t durationMs)
{
    reactionAnim = anim;
    reactionEnd = now + durationMs;
    reactionActive = true;
}

void PetGfx_SetSleeping(bool isSleeping)
{
    sleeping = isSleeping;
}
