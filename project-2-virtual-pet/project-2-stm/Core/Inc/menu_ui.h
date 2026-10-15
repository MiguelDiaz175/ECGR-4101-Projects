#ifndef MENU_UI_H
#define MENU_UI_H

#include <stdint.h>

#include "pet_game.h"

/* The numeric order of each enum matches the top-to-bottom drawing order.
 * ITEM_COUNT is not a real option; app.c uses it for wraparound math. */
typedef enum
{
  MAIN_MENU_START_GAME,
  MAIN_MENU_DEBUG,
  MAIN_MENU_ITEM_COUNT
} MainMenuSelection;

typedef enum
{
  DEBUG_MENU_MPU,
  DEBUG_MENU_MICROPHONE,
  DEBUG_MENU_LIGHT,
  DEBUG_MENU_GRAPHICS,
  DEBUG_MENU_BACK,
  DEBUG_MENU_ITEM_COUNT
} DebugMenuSelection;

typedef enum
{
  GAME_OVER_RESTART,
  GAME_OVER_MAIN_MENU
} GameOverSelection;

typedef enum
{
  PAUSE_RESUME,
  PAUSE_MAIN_MENU,
  PAUSE_ITEM_COUNT
} PauseMenuSelection;

void MenuUI_DrawMain(MainMenuSelection selection);
void MenuUI_DrawMainItems(MainMenuSelection selection);
void MenuUI_DrawDebug(DebugMenuSelection selection);
void MenuUI_DrawDebugItems(DebugMenuSelection selection);
void MenuUI_DrawGameOver(const GameSummary *summary,
                         GameOverSelection selection);
void MenuUI_DrawGameOverOptions(GameOverSelection selection);
void MenuUI_DrawPause(PauseMenuSelection selection);
void MenuUI_DrawPauseItems(PauseMenuSelection selection);

#endif
