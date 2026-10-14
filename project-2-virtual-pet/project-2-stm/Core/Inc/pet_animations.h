#ifndef ANIMATION_H
#define ANIMATION_H

typedef enum {
    ANIMATION_IDLE,
    ANIMATION_FEEDING,
    ANIMATION_RAN_AWAY,
    ANIMATION_DEAD
} animationState;

typedef struct {
    int hunger;
    int happiness;
    animationState state;
    int updates_since_happiness;
} animationPet;

void Pet_Init(animationPet *pet);
void Pet_Update(animationPet *pet);
void Pet_Feed(animationPet *pet);
void Pet_Scare(animationPet *pet);

int Pet_GetHunger(animationPet *pet);
int Pet_GetHappiness(animationPet *pet);
animationState Pet_GetState(animationPet *pet);

#endif