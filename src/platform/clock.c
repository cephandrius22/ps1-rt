#include "clock.h"
#include <SDL.h>

double clock_seconds(void) {
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

uint32_t clock_seed(void) {
    return (uint32_t)SDL_GetPerformanceCounter();
}
