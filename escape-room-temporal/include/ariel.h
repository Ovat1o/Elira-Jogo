#ifndef ARIEL_H
#define ARIEL_H

typedef struct {
    int corruptionLevel;
} Ariel;

void InitAriel(Ariel *ai);
const char* GetArielDialogue(Ariel *ai, int lives);

#endif