#include "app.h"

#include "debug_screens.h"
#include "joystick_input.h"
#include "menu_ui.h"
#include "pet_game.h"
#include <stdbool.h>

typedef enum
{
  APP_MAIN_MENU,
  APP_PLAYING,
  APP_PAUSED,
  APP_GAME_OVER,
  APP_DEBUG_MENU,
  APP_DEBUG_SCREEN
} AppState;

static AppState state = APP_MAIN_MENU;
static MainMenuSelection mainSelection = MAIN_MENU_START_GAME;
static DebugMenuSelection debugSelection = DEBUG_MENU_MPU;
static GameOverSelection gameOverSelection = GAME_OVER_RESTART;
static PauseMenuSelection pauseSelection = PAUSE_RESUME;
static DebugScreen activeDebugScreen = DEBUG_SCREEN_MPU;

static uint8_t MoveSelection(uint8_t selection, uint8_t count, int8_t direction)
{
  if (direction < 0)
  {
    return (selection == 0U) ? (uint8_t)(count - 1U) : selection - 1U;
  }
  return (uint8_t)((selection + 1U) % count);
}

static void StartGame(uint32_t now)
{
  PetGame_Start(now);
  state = APP_PLAYING;
}

static void EnterDebugScreen(DebugScreen screen)
{
  activeDebugScreen = screen;
  state = APP_DEBUG_SCREEN;
  DebugScreens_Enter(screen);
}

static void ShowGameOver(void)
{
  GameSummary summary;

  PetGame_GetSummary(&summary);
  gameOverSelection = GAME_OVER_RESTART;
  state = APP_GAME_OVER;
  MenuUI_DrawGameOver(&summary, gameOverSelection);
}

void App_Init(void)
{
  state = APP_MAIN_MENU;
  mainSelection = MAIN_MENU_START_GAME;
  debugSelection = DEBUG_MENU_MPU;
  JoystickInput_Init();
  MenuUI_DrawMain(mainSelection);
}

void App_Update(uint32_t now)
{
  uint8_t pressed = JoystickInput_GetPressed(now);

  switch (state)
  {
    case APP_MAIN_MENU:
      if ((pressed & JOY_UP_MASK) != 0U)
      {
        mainSelection = (MainMenuSelection)MoveSelection(
            mainSelection, MAIN_MENU_ITEM_COUNT, -1);
        MenuUI_DrawMainItems(mainSelection);
      }
      if ((pressed & JOY_DOWN_MASK) != 0U)
      {
        mainSelection = (MainMenuSelection)MoveSelection(
            mainSelection, MAIN_MENU_ITEM_COUNT, 1);
        MenuUI_DrawMainItems(mainSelection);
      }
      if ((pressed & JOY_CENTER_MASK) != 0U)
      {
        if (mainSelection == MAIN_MENU_START_GAME)
        {
          StartGame(now);
        }
        else
        {
          debugSelection = DEBUG_MENU_MPU;
          state = APP_DEBUG_MENU;
          MenuUI_DrawDebug(debugSelection);
        }
      }
      break;

    case APP_PLAYING:
      if ((pressed & JOY_CENTER_MASK) != 0U)
      {
        pauseSelection = PAUSE_RESUME;
        state = APP_PAUSED;
        MenuUI_DrawPause(pauseSelection);
      }
      else if (PetGame_Update(now, (pressed & JOY_RIGHT_MASK) != 0U) ==
               PET_GAME_OVER_READY)
      {
        ShowGameOver();
      }
      break;

    case APP_PAUSED:
      if ((pressed & JOY_UP_MASK) != 0U)
      {
        pauseSelection = (PauseMenuSelection)MoveSelection(
            pauseSelection, PAUSE_ITEM_COUNT, -1);
        MenuUI_DrawPauseItems(pauseSelection);
      }
      if ((pressed & JOY_DOWN_MASK) != 0U)
      {
        pauseSelection = (PauseMenuSelection)MoveSelection(
            pauseSelection, PAUSE_ITEM_COUNT, 1);
        MenuUI_DrawPauseItems(pauseSelection);
      }
      if ((pressed & JOY_CENTER_MASK) != 0U)
      {
        if (pauseSelection == PAUSE_RESUME)
        {
          PetGame_Resume(now);
          state = APP_PLAYING;
        }
        else
        {
          state = APP_MAIN_MENU;
          MenuUI_DrawMain(mainSelection);
        }
      }
      break;

    case APP_GAME_OVER:
      if ((pressed & (JOY_UP_MASK | JOY_DOWN_MASK |
                      JOY_LEFT_MASK | JOY_RIGHT_MASK)) != 0U)
      {
        gameOverSelection = (gameOverSelection == GAME_OVER_RESTART) ?
                            GAME_OVER_MAIN_MENU : GAME_OVER_RESTART;
        MenuUI_DrawGameOverOptions(gameOverSelection);
      }
      if ((pressed & JOY_CENTER_MASK) != 0U)
      {
        if (gameOverSelection == GAME_OVER_RESTART)
        {
          StartGame(now);
        }
        else
        {
          state = APP_MAIN_MENU;
          MenuUI_DrawMain(mainSelection);
        }
      }
      break;

    case APP_DEBUG_MENU:
      if ((pressed & JOY_UP_MASK) != 0U)
      {
        debugSelection = (DebugMenuSelection)MoveSelection(
            debugSelection, DEBUG_MENU_ITEM_COUNT, -1);
        MenuUI_DrawDebugItems(debugSelection);
      }
      if ((pressed & JOY_DOWN_MASK) != 0U)
      {
        debugSelection = (DebugMenuSelection)MoveSelection(
            debugSelection, DEBUG_MENU_ITEM_COUNT, 1);
        MenuUI_DrawDebugItems(debugSelection);
      }
      if ((pressed & JOY_CENTER_MASK) != 0U)
      {
        switch (debugSelection)
        {
          case DEBUG_MENU_MPU:
            EnterDebugScreen(DEBUG_SCREEN_MPU);
            break;
          case DEBUG_MENU_MICROPHONE:
            EnterDebugScreen(DEBUG_SCREEN_MICROPHONE);
            break;
          case DEBUG_MENU_LIGHT:
            EnterDebugScreen(DEBUG_SCREEN_LIGHT);
            break;
          case DEBUG_MENU_GRAPHICS:
            EnterDebugScreen(DEBUG_SCREEN_GRAPHICS);
            break;
          case DEBUG_MENU_BACK:
            state = APP_MAIN_MENU;
            MenuUI_DrawMain(mainSelection);
            break;
          default:
            break;
        }
      }
      break;

    case APP_DEBUG_SCREEN:
      if ((pressed & JOY_CENTER_MASK) != 0U)
      {
        state = APP_DEBUG_MENU;
        MenuUI_DrawDebug(debugSelection);
      }
      else
      {
        if ((pressed & JOY_UP_MASK) != 0U)
        {
          DebugScreens_HandleUp(activeDebugScreen);
        }
        DebugScreens_Update(activeDebugScreen, now);
      }
      break;

    default:
      state = APP_MAIN_MENU;
      MenuUI_DrawMain(mainSelection);
      break;
  }
}
