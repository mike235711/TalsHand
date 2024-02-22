#ifndef TTABLE_H
#define TTABLE_H
#include "move.h"
#include <vector>
#include <cstring> // For std::memset

// The transposition table will store the zobrist keys of seen positions, the depth reached starting from that position, the 
// best move found and the evaluation found.
// If in the algorithm we reach a position which is in the transposition table:
// + If the depth we are going to search (from the position) is less or equal than the one in the table. We return the evaluation and 
//   the alpha or beta, to change it if needed.
// + If the depth  is more than the one in the table, we return the best move found previously to start searching on that move.

// TTEntry struct is the transposition table entry, defined as below:
//
// key                                  16 bit
// depth (max depth - current depth)    8 bit
// value                                16 bit
// move                                 16 bit
// turn (engine or opponent)            8 bit
// alpha/beta                           16 bit

struct TTEntry
{

    Move getMove() const { return Move(move); }
    int16_t getValue() const { return value; }
    uint8_t getDepth() const { return depth; }
    bool getTurn() const { return turn; }
    int16_t getAlphaOrBeta() const { return alphaBeta; }
    // Implementation of TTEntry::save
    void save(uint16_t k, int16_t v, uint8_t d, Move m, bool t, int16_t ab)
    {
        key = k;
        value = v;
        depth = d;
        move = m;
        turn = t;
        alphaBeta = ab;
    }

private:
    friend class TranspositionTable;

    uint16_t key;
    uint8_t depth;
    bool turn;
    Move move;
    int16_t value;
    int16_t alphaBeta;
};


class TranspositionTable
{
public:
    TranspositionTable() : tableSize(0), table(nullptr) {}
    ~TranspositionTable() { delete[] table; }

    // Initializes or resizes the table to a number of entries, which is a power of two
    void resize(size_t newSize)
    {
        delete[] table;
        tableSize = newSize;
        table = new TTEntry[newSize];
        std::memset(table, 0, newSize * sizeof(TTEntry));
    }

    // Probes the table for a given key. Returns a pointer to the TTEntry if found, otherwise nullptr.
    TTEntry *probe(uint16_t key) const
    {
        if (table == nullptr)
            return nullptr;

        size_t index = key % tableSize;
        return (table[index].key == key) ? &table[index] : nullptr;
    }

    // Save a new entry to the table
    void save(uint16_t key, int16_t value, uint8_t depth, Move move, bool turn, int16_t alphaBeta)
    {
        size_t index = key % tableSize;

        // Example replacement strategy: always replace
        table[index].key = key;
        table[index].depth = depth;
        table[index].value = value;
        table[index].move = move;
        table[index].turn = turn;
        table[index].alphaBeta = alphaBeta;
    }

private:
    size_t tableSize; // The total number of entries in the table
    TTEntry *table;   // Dynamic array of TTEntry
};

#endif