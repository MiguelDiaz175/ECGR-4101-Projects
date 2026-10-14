#include "pet.h"

void Pet_Init(Pet *pet)
{
    pet->hunger = 100;
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

     /* Overfeeding penalty when hunger is already full. */
    if (pet->hunger >= 100)
    {
        pet->happiness -= 10;

        /* Happiness cannot fall below 0. */
        if (pet->happiness < 0)
        {
            pet->happiness = 0;
            pet->state = PET_RAN_AWAY;
            return;
        }
    }
    else
    {
        /* Normal feeding increases hunger by 10. */
        pet->hunger += 10;

        if (pet->hunger > 100)
        {
            pet->hunger = 100;
        }
        /* Increase happiness by 3, capped at 100. */
        pet->happiness += 3;

        if (pet->happiness > 100)
        {
            pet->happiness = 100;
        }
    }
    /* Feeding is a temporary state. */
    pet->state = PET_FEEDING;
}

void Pet_Scare(Pet *pet)
{
    /* Dead or runaway pets cannot be scared. */
    if (pet->state == PET_DEAD ||
        pet->state == PET_RAN_AWAY)
    {
        return;
    }

    /* Loud noises decrease happiness by 10. */
    pet->happiness -= 10;

    if (pet->happiness <= 0)
    {
        pet->happiness = 0;
        pet->state = PET_RAN_AWAY;
    }
}

void Pet_Comfort(Pet *pet)
{
    if (pet->state == PET_DEAD ||
        pet->state == PET_RAN_AWAY)
    {
        return;
    }

    if (pet->happiness < 100)
    {
        pet->happiness++;
    }
}

void Pet_DarknessTick(Pet *pet, int happinessLoss)
{
    /* Darkness drains happiness gradually instead of applying the larger
     * one-time penalty used for a shake or loud-noise scare. */
    if (pet->state == PET_DEAD || pet->state == PET_RAN_AWAY)
    {
        return;
    }

    if (happinessLoss < 0)
    {
        return;
    }

    pet->happiness -= happinessLoss;

    if (pet->happiness < 0)
    {
        pet->happiness = 0;
    }

    if (pet->happiness == 0)
    {
        pet->state = PET_RAN_AWAY;
    }
}


void Pet_Update(Pet *pet)
{
      /* Terminal states do not change during normal gameplay. */
    if (pet->state == PET_DEAD ||
        pet->state == PET_RAN_AWAY)
    {
        return;
    }

    /* Finish feeding without skipping normal stat decreases. */
    if (pet->state == PET_FEEDING)
    {
        pet->state = PET_IDLE;
    }

    /* Normal gameplay: hunger decreases every update. */
    if (pet->hunger > 0)
    {
        pet->hunger--;
    }

    /* Happiness decreases every fourth update (half the previous rate). */
    pet->updates_since_happiness++;

    if (pet->updates_since_happiness >= 4)
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
    /* Runaway transition. */
    else if (pet->happiness == 0)
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
