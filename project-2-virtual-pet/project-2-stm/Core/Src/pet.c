#include "pet.h"

void Pet_Init(Pet *pet)
{
    pet->hunger = 2;
    pet->happiness = 100;
    pet->state = PET_IDLE;
    pet->updates_since_happiness = 0;
}

void Pet_Feed(Pet *pet)
{
    /* A dead or runaway pet cannot be fed. */
    if (pet->state == PET_DEAD ||
        pet->state == PET_RAN_AWAY)
    {
        return;
    }

    /* Increase hunger meter, capped at 100. */
    pet->hunger += 10;

    if (pet->hunger > 100)
    {
        pet->hunger = 100;
    }

    /* Feeding is a temporary state. */
    pet->state = PET_FEEDING;
}


void Pet_Update(Pet *pet)
{
    /* Terminal states do not change during normal gameplay. */
    if (pet->state == PET_DEAD ||
        pet->state == PET_RAN_AWAY)
    {
        return;
    }

    /* Finish the feeding action and return to idle. */
    if (pet->state == PET_FEEDING)
    {
        pet->state = PET_IDLE;
        return;
    }

    /* Normal idle gameplay: hunger decreases every update. */
    if (pet->hunger > 0)
    {
        pet->hunger--;
    }

    /* Happiness decreases every second update. */
    pet->updates_since_happiness++;

    if (pet->updates_since_happiness >= 2)
    {
        if (pet->happiness > 0)
        {
            pet->happiness--;
        }

        pet->updates_since_happiness = 0;
    }

    /* Death transition. */
    if (pet->hunger == 0)
    {
        pet->state = PET_DEAD;
    }

    /* Ranaway transition. */
    if (pet->happiness == 0)
    {
        pet->state = PET_RAN_AWAY;
    }
}

int Pet_GetHunger(Pet *pet)
{
    return pet->hunger;
}

int Pet_GetHappiness(Pet *pet)
{
    return pet->happiness;
}

petState Pet_GetState(Pet *pet)
{
    return pet->state;
}