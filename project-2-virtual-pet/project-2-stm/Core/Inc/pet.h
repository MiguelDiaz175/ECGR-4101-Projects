#ifndef PET_H
#define PET_H

typedef enum {
    PET_IDLE,
    PET_FEEDING,
    PET_DEAD
} petState;

typedef struct {
    int hunger;
    int happiness;
    petState state;
} Pet;

void Pet_Init(Pet *pet);
void Pet_Update(Pet *pet);
void Pet_Feed(Pet *pet);
int Pet_GetHunger(Pet *pet);
int Pet_GetHappiness(Pet *pet);

#endif