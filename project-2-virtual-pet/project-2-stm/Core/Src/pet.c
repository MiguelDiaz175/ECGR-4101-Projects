#include "pet.h"

static int hunger = 100;
static int happiness = 100;

void Pet_Init(Pet *pet)
{
    pet->hunger = 75;
    pet->happiness = 85;
    pet->state = PET_IDLE;
}

void Pet_Feed(Pet *pet)
{
    pet->hunger += 10;

    if (pet->hunger > 100)
        pet->hunger = 100;

    pet->state = PET_FEEDING;
}

int Pet_GetHunger(Pet *pet)
{
    return pet->hunger;
}

int Pet_GetHappiness(Pet *pet)
{
    return pet->happiness;
}

void Pet_Update(Pet *pet)
{
    if (pet->hunger > 0)
        pet->hunger--;
}