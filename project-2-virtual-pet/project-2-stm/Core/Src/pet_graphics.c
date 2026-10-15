#include "pet_graphics.h"
#include "pet_sprites.h"
#include "ili9341.h"
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Sprites
 * ---------------------------------------------------------------------------
 * Same "XPM-lite" approach as the course demo: each frame in pet_sprites.h
 * is a grid of characters, and SpriteColor() maps each character to an
 * RGB565 color using the demo's rgb565() helper. Each sprite pixel is drawn
 * as a SPR_SCALE x SPR_SCALE block.
 * ------------------------------------------------------------------------- */

#define SPR_SCALE     2U
#define SPRITE_PX     (SPR_W * SPR_SCALE)   /* 128 px on the LCD */

/* Pet area: everything above the HUNGER label (y = 218). */
#define AREA_BG       ILI9341_COLOR_BLACK
#define PET_X         ((ILI9341_WIDTH - SPRITE_PX) / 2U)   /* 56 */
#define PET_Y         60U                                  /* ends at y = 188 */
#define PET_TILT_X_LIMIT 48
/* PET_Y + 28 + SPRITE_PX = 216, leaving two rows before HUNGER at 218. */
#define PET_TILT_Y_LIMIT 28

/* Right of the sprite box, so redrawing the sprite never erases the Zs. */
#define ZZZ_X         (PET_X + SPRITE_PX + 2U)   /* 186 */
#define ZZZ_Y         (PET_Y + 10U)

/* A sprite is one 64-row character grid from pet_sprites.h. */
typedef const char *const *Sprite;

/* An animation is a list of frames played in order, then repeated
 * (or, if loops is false, left on the last frame). */
typedef struct {
    const Sprite *frames;
    uint8_t frameCount;
    uint16_t frameMs;      /* how long each frame stays on screen */
    uint16_t holdLastMs;   /* extra time on the last frame before looping */
    bool loops;            /* false: play once and stop on the last frame */
} PetAnimation;

static const Sprite IDLE_FRAMES[] = {
    PET_IDLE_1, PET_IDLE_2, PET_IDLE_3, PET_IDLE_4, PET_IDLE_5, PET_IDLE_6
};
static const Sprite EATING_FRAMES[] = {
    PET_EAT_1, PET_EAT_2, PET_EAT_3, PET_EAT_4, PET_EAT_5
};
static const Sprite SCARED_FRAMES[] = {
    PET_SCARED_1, PET_SCARED_2, PET_SCARED_3, PET_SCARED_4
};
static const Sprite SAD_FRAMES[] = {
    PET_SAD_1, PET_SAD_2, PET_SAD_3, PET_SAD_4, PET_SAD_5, PET_SAD_6
};
/* Forwards then backwards (1-5, then 4-2) so the loop back to 1 is smooth. */
static const Sprite SLEEPING_FRAMES[] = {
    PET_SLEEP_1, PET_SLEEP_2, PET_SLEEP_3, PET_SLEEP_4, PET_SLEEP_5,
    PET_SLEEP_4, PET_SLEEP_3, PET_SLEEP_2
};
static const Sprite DEAD_FRAMES[] = {
    PET_DEAD_1
};
/* Gets angrier (1-5), then flickers out and stays gone. 8 frames x 300 ms
 * finishes before the PET_ENDING_SCREEN_MS (3 s) game-over hand-off. */
static const Sprite RAN_AWAY_FRAMES[] = {
    PET_RUN_AWAY_1, PET_RUN_AWAY_2, PET_RUN_AWAY_3, PET_RUN_AWAY_4,
    PET_RUN_AWAY_5, PET_EMPTY, PET_RUN_AWAY_5, PET_EMPTY
};
/* Bounce up to the stretch (1-3-1), then wink and laugh (4-5-4). */
static const Sprite HAPPY_FRAMES[] = {
    PET_HAPPY_1, PET_HAPPY_2, PET_HAPPY_3, PET_HAPPY_2,
    PET_HAPPY_1, PET_HAPPY_4, PET_HAPPY_5, PET_HAPPY_4
};

#define COUNT_OF(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

/* Indexed by PetGfxAnim.          frames            count                        frameMs  holdLastMs  loops */
static const PetAnimation ANIMATIONS[PETGFX_ANIM_COUNT] = {
    [PETGFX_ANIM_IDLE]     = { IDLE_FRAMES,     COUNT_OF(IDLE_FRAMES),     10U,    10000U, true  },
    [PETGFX_ANIM_EATING]   = { EATING_FRAMES,   COUNT_OF(EATING_FRAMES),   100U,      0U, true  },
    [PETGFX_ANIM_SCARED]   = { SCARED_FRAMES,   COUNT_OF(SCARED_FRAMES),   100U,      0U, true  },
    [PETGFX_ANIM_SAD]      = { SAD_FRAMES,      COUNT_OF(SAD_FRAMES),      10U,    10000U, true  },
    [PETGFX_ANIM_SLEEPING] = { SLEEPING_FRAMES, COUNT_OF(SLEEPING_FRAMES), 200U,      0U, true  },
    [PETGFX_ANIM_DEAD]     = { DEAD_FRAMES,     COUNT_OF(DEAD_FRAMES),     500U,      0U, true  },
    [PETGFX_ANIM_RAN_AWAY] = { RAN_AWAY_FRAMES, COUNT_OF(RAN_AWAY_FRAMES), 300U,      0U, false },
    [PETGFX_ANIM_HAPPY]    = { HAPPY_FRAMES,    COUNT_OF(HAPPY_FRAMES),    150U,      0U, true  },
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
static bool scared = false;
static bool zzzShown = false;

/* Pixel offset from the accelerometer, set by PetGfx_SetTilt(). */
static int16_t tiltX = 0;
static int16_t tiltY = 0;

/* What is on the LCD right now, so we only redraw when something changes. */
static Sprite drawnSprite = NULL;
static int16_t drawnX = -1;
static int16_t drawnY = -1;

/* ---------------------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------------------- */

/* Pack 8-bit R, G, B into RGB565 (same helper as the course demo). */
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* Character -> color, like the demo's SpriteColor(). Colors match the
 * hand-colored artwork. */
static uint16_t SpriteColor(char c)
{
    switch (c)
    {
        case 'K': return rgb565(0x28, 0x1E, 0x3C);   /* outline, very dark purple */
        case 'B': return rgb565(0xFF, 0xDF, 0x00);   /* body, yellow */
        case 'W': return rgb565(0xFF, 0xFF, 0xFF);   /* eyes, teeth, tears */
        case 'R': return rgb565(0xBA, 0x37, 0x46);   /* inside of mouth, red */
        case '.':
        default:  return AREA_BG;                    /* background */
    }
}

/* Draw one sprite at (x, y). The demo sends every pixel separately; here a
 * run of same-colored characters in a row becomes one FillRect, which is
 * far fewer SPI transfers and keeps the main loop responsive. */
static void DrawSprite(Sprite sprite, uint16_t x, uint16_t y)
{
    for (uint16_t r = 0U; r < SPR_H; r++)
    {
        const char *row = sprite[r];
        uint16_t c = 0U;

        while (c < SPR_W)
        {
            uint16_t color = SpriteColor(row[c]);
            uint16_t run = 1U;

            while (((c + run) < SPR_W) && (SpriteColor(row[c + run]) == color))
            {
                run++;
            }

            ILI9341_FillRect((uint16_t)(x + c * SPR_SCALE),
                             (uint16_t)(y + r * SPR_SCALE),
                             (uint16_t)(run * SPR_SCALE), SPR_SCALE, color);
            c += run;
        }
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

/* When the pet slides, paint background over the strips it moved away
 * from so it doesn't leave a trail. */
static void EraseVacated(int16_t newX, int16_t newY)
{
    if (drawnX < 0)
    {
        return;   /* nothing drawn yet */
    }

    if (newX > drawnX)
    {
        ILI9341_FillRect((uint16_t)drawnX, (uint16_t)drawnY,
                         (uint16_t)(newX - drawnX), SPRITE_PX, AREA_BG);
    }
    else if (newX < drawnX)
    {
        ILI9341_FillRect((uint16_t)(newX + (int16_t)SPRITE_PX), (uint16_t)drawnY,
                         (uint16_t)(drawnX - newX), SPRITE_PX, AREA_BG);
    }

    if (newY > drawnY)
    {
        ILI9341_FillRect((uint16_t)drawnX, (uint16_t)drawnY,
                         SPRITE_PX, (uint16_t)(newY - drawnY), AREA_BG);
    }
    else if (newY < drawnY)
    {
        ILI9341_FillRect((uint16_t)drawnX, (uint16_t)(newY + (int16_t)SPRITE_PX),
                         SPRITE_PX, (uint16_t)(drawnY - newY), AREA_BG);
    }
}

static void Render(void)
{
    Sprite sprite = ANIMATIONS[currentAnim].frames[frameIndex];

    /* Terminal frames stay put. All living animations, including the
     * rocking-triggered sleeping animation, continue to follow tilt. */
    bool stayPut = (currentAnim == PETGFX_ANIM_DEAD) ||
                   (currentAnim == PETGFX_ANIM_RAN_AWAY);
    int16_t x = (int16_t)PET_X + (stayPut ? 0 : tiltX);
    int16_t y = (int16_t)PET_Y + (stayPut ? 0 : tiltY);

    if ((sprite == drawnSprite) && (x == drawnX) && (y == drawnY))
    {
        return;   /* same frame, same place: don't touch the LCD */
    }

    EraseVacated(x, y);
    DrawSprite(sprite, (uint16_t)x, (uint16_t)y);
    DrawZzz(currentAnim == PETGFX_ANIM_SLEEPING);
    drawnSprite = sprite;
    drawnX = x;
    drawnY = y;
}

/* ---------------------------------------------------------------------------
 * Animation logic
 * ------------------------------------------------------------------------- */
static PetGfxAnim ChooseAnimation(Pet *pet, uint32_t now)
{
    /* Endings beat everything else, including a reaction still playing. */
    if (Pet_GetState(pet) == PET_DEAD)
    {
        return PETGFX_ANIM_DEAD;
    }

    if (Pet_GetState(pet) == PET_RAN_AWAY)
    {
        return PETGFX_ANIM_RAN_AWAY;
    }

    /* Persistent darkness fear has priority over short action reactions.
     * Rocking and feeding may still change statistics while dark, but the
     * pet must remain visibly scared until the light returns. */
    if (scared)
    {
        return PETGFX_ANIM_SCARED;
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
    scared = false;
    zzzShown = false;
    tiltX = 0;
    tiltY = 0;

    /* The screen was just cleared, so nothing of ours is on it. */
    drawnSprite = NULL;
    drawnX = -1;
    drawnY = -1;

    Render();
}

void PetGfx_Update(Pet *pet, uint32_t now)
{
    PetGfxAnim wanted = ChooseAnimation(pet, now);
    const PetAnimation *anim = &ANIMATIONS[currentAnim];
    uint32_t showFor = anim->frameMs;

    /* The last frame stays up a little longer before the loop restarts. */
    if (frameIndex == (anim->frameCount - 1U))
    {
        showFor += anim->holdLastMs;
    }

    if (wanted != currentAnim)
    {
        currentAnim = wanted;
        frameIndex = 0U;
        lastFrameTime = now;
    }
    else if (!anim->loops && (frameIndex == (anim->frameCount - 1U)))
    {
        /* One-shot animation finished: stay on its last frame. */
    }
    else if ((now - lastFrameTime) >= showFor)
    {
        lastFrameTime = now;
        frameIndex = (uint8_t)((frameIndex + 1U) % anim->frameCount);
    }

    Render();
}

void PetGfx_PlayReaction(PetGfxAnim anim, uint32_t now, uint32_t durationMs)
{
    reactionAnim = anim;
    reactionEnd = now + durationMs;
    reactionActive = true;
}

void PetGfx_SetTilt(int16_t dx, int16_t dy)
{
    /* Enforce the renderer's safe drawing region even if a caller is tuned
     * incorrectly. Otherwise EraseVacated() can paint over the status text. */
    if (dx > PET_TILT_X_LIMIT)  { dx = PET_TILT_X_LIMIT; }
    if (dx < -PET_TILT_X_LIMIT) { dx = -PET_TILT_X_LIMIT; }
    if (dy > PET_TILT_Y_LIMIT)  { dy = PET_TILT_Y_LIMIT; }
    if (dy < -PET_TILT_Y_LIMIT) { dy = -PET_TILT_Y_LIMIT; }
    tiltX = dx;
    tiltY = dy;
}

void PetGfx_SetScared(bool isScared)
{
    scared = isScared;
}

void PetGfx_SetSleeping(bool isSleeping)
{
    sleeping = isSleeping;
}
