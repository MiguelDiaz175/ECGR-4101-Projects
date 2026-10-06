#include "pet.h"

static int hunger = 100;
static int happiness = 100;

void Pet_Init(void)
{
    hunger = 100;
    happiness = 100;
}

void Pet_Feed(void)
{
    hunger += 10;

    if (hunger > 100)
        hunger = 100;
}

int Pet_GetHunger(void)
{
    return hunger;
}

void Pet_Update(void)
{
    if (hunger > 0)
        hunger--;
}