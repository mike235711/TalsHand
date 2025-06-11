#include <iostream>

#include "precomputed_moves.h"
#include "magicmoves.h"
#include "engine.h"
#include "zobrist_keys.h"

int main(int argc, char *argv[])
{
    // Initialize magic numbers
    initmagicmoves();

    // Initialize zobrist keys
    zobrist_keys::initializeZobristNumbers();
    // zobrist_keys::printAllZobristKeys();

    // Initialize engine object with chosen config and start uci loop
    THEngine THEngine(argv[0]);
    THEngine.readUci();

    return 0;
}
