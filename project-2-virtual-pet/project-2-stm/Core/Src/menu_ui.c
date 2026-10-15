#include "menu_ui.h"

#include "ili9341.h"
#include <stdio.h>

/*
 * Menu drawing is intentionally separate from menu behavior. This file only
 * paints pixels; app.c decides what a selection actually does.
 */
static void DrawMenuItem(uint8_t index, uint8_t selection, uint16_t y,
                         uint16_t textX, const char *label)
{
  /* The selected item gets a blue background. Every other item is black. */
  uint16_t background = (selection == index) ?
                        ILI9341_COLOR_BLUE : ILI9341_COLOR_BLACK;

  ILI9341_FillRect(20, y, 200, 36, background);
  ILI9341_DrawRect(20, y, 200, 36, ILI9341_COLOR_WHITE);
  ILI9341_DrawString(textX, y + 11, label,
                     ILI9341_COLOR_WHITE, background, 2);
}

void MenuUI_DrawMain(MainMenuSelection selection)
{
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(54, 12, "VIRTUAL PET",
                     ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(72, 38, "MAIN MENU",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  MenuUI_DrawMainItems(selection);
  ILI9341_DrawString(45, 282, "UP/DOWN + CENTER",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
}

void MenuUI_DrawMainItems(MainMenuSelection selection)
{
  /* These "Items" functions redraw only the buttons. App uses them when the
   * highlight moves so the unchanged title and instructions are not sent
   * over SPI again. */
  DrawMenuItem(MAIN_MENU_START_GAME, selection, 95, 60, "START GAME");
  DrawMenuItem(MAIN_MENU_DEBUG, selection, 155, 84, "DEBUG");
}

void MenuUI_DrawDebug(DebugMenuSelection selection)
{
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(54, 12, "DEBUG MENU",
                     ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 2);
  MenuUI_DrawDebugItems(selection);
  ILI9341_DrawString(45, 292, "UP/DOWN + CENTER",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
}

void MenuUI_DrawDebugItems(DebugMenuSelection selection)
{
  DrawMenuItem(DEBUG_MENU_MPU, selection, 40, 42, "MPU-9250 TEST");
  DrawMenuItem(DEBUG_MENU_MICROPHONE, selection, 84, 30, "MICROPHONE TEST");
  DrawMenuItem(DEBUG_MENU_LIGHT, selection, 128, 36, "LIGHT SENSOR");
  DrawMenuItem(DEBUG_MENU_GRAPHICS, selection, 172, 42, "LCD GRAPHICS");
  DrawMenuItem(DEBUG_MENU_BACK, selection, 216, 90, "BACK");
}

void MenuUI_DrawPause(PauseMenuSelection selection)
{
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(72, 10, "GAME PAUSED",
                     ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(20, 42, "RIGHT: FEED",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(20, 57, "ROCK: COMFORT/SLEEP",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(20, 72, "COVER LIGHT: SCARED",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  MenuUI_DrawPauseItems(selection);
  ILI9341_DrawString(45, 282, "UP/DOWN + CENTER",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
}

void MenuUI_DrawPauseItems(PauseMenuSelection selection)
{
  DrawMenuItem(PAUSE_RESUME, selection, 125, 78, "RESUME");
  DrawMenuItem(PAUSE_MAIN_MENU, selection, 185, 56, "MAIN MENU");
}

void MenuUI_DrawGameOver(const GameSummary *summary,
                         GameOverSelection selection)
{
  char line[24];

  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(38, 25, "GAME OVER",
                     ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 3);

  if (summary->endState == PET_DEAD)
  {
    ILI9341_DrawString(38, 72, "YOUR PET DIED",
                       ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
  }
  else
  {
    ILI9341_DrawString(8, 72, "YOUR PET RAN AWAY",
                       ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 2);
  }

  (void)snprintf(line, sizeof(line), "AVG HUNGER: %lu",
                 (unsigned long)summary->averageHunger);
  ILI9341_DrawString(12, 115, line,
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);

  (void)snprintf(line, sizeof(line), "AVG HAPPINESS: %lu",
                 (unsigned long)summary->averageHappiness);
  ILI9341_DrawString(12, 145, line,
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);

  MenuUI_DrawGameOverOptions(selection);
  ILI9341_DrawString(20, 300, "MOVE + CENTER TO SELECT",
                     ILI9341_COLOR_GRAY, ILI9341_COLOR_BLACK, 1);
}

void MenuUI_DrawGameOverOptions(GameOverSelection selection)
{
  uint16_t restartBackground = (selection == GAME_OVER_RESTART) ?
                               ILI9341_COLOR_BLUE : ILI9341_COLOR_BLACK;
  uint16_t menuBackground = (selection == GAME_OVER_MAIN_MENU) ?
                            ILI9341_COLOR_BLUE : ILI9341_COLOR_BLACK;

  ILI9341_FillRect(20, 205, 200, 35, restartBackground);
  ILI9341_DrawRect(20, 205, 200, 35, ILI9341_COLOR_WHITE);
  ILI9341_DrawString(56, 216, "PLAY AGAIN",
                     ILI9341_COLOR_WHITE, restartBackground, 2);

  ILI9341_FillRect(20, 250, 200, 35, menuBackground);
  ILI9341_DrawRect(20, 250, 200, 35, ILI9341_COLOR_WHITE);
  ILI9341_DrawString(56, 261, "MAIN MENU",
                     ILI9341_COLOR_WHITE, menuBackground, 2);
}
