#include "ariel.h"

void InitAriel(Ariel *ai) {
    ai->corruptionLevel = 100;
}

const char* GetArielDialogue(Ariel *ai, int lives) {
    if (lives == 3) return "A.R.1.3.L: Monitorando parametros da intrusa...";
    if (lives == 2) return "A.R.1.3.L: Erro detectado. Reduzindo nivel de tolerancia.";
    return "A.R.1.3.L: Ultima tentativa. Bloqueio iminente.";
}