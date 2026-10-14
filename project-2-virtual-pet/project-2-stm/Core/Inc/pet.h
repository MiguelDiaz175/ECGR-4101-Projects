#ifndef PET_H
#define PET_H

typedef enum {
    PET_IDLE,
    PET_FEEDING,
    PET_RAN_AWAY,
    PET_DEAD
} petState;

typedef struct {
    int hunger;
    int happiness;
    petState state;
    int updates_since_happiness;
} Pet;

void Pet_Init(Pet *pet);
void Pet_Update(Pet *pet);
void Pet_Feed(Pet *pet);
void Pet_Scare(Pet *pet);
void Pet_DarknessTick(Pet *pet);

int Pet_GetHunger(Pet *pet);
int Pet_GetHappiness(Pet *pet);
petState Pet_GetState(Pet *pet);

#endif
