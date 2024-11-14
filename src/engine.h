#ifndef ENGINE_H
#define ENGINE_H
#include "bitposition.h"
#include <vector>
#include <utility> // For std::pair
#include <limits>  // For numeric limits
#include <array>
#include <cstdint>
#include <chrono>
#include <algorithm> // For std::max
#include "ttable.h"
#include <memory>
#include "position_eval.h"


extern TranspositionTable globalTT;
extern int OURTIME;
extern int OURINC;
extern std::chrono::time_point<std::chrono::high_resolution_clock> STARTTIME;

std::pair<Move, int16_t> iterativeSearch(BitPosition position, int8_t start_depth, int8_t fixed_max_depth = 100);
#endif
