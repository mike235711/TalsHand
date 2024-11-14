#ifndef TTABLE_H
#define TTABLE_H
#include "move.h"
#include <vector>
#include <cstring> // For std::memset

// The transposition table will store the zobrist keys of seen positions, the depth reached starting from that position, the
// best move found, the value found and the value type.
// 
// Value types can either be exact (no cutoff produced when the position was searched previously), 
// lower bounds (a beta cutoof was done when searching this position previously), or upper bounds
// (a alpha cutoff was done when searching this position previously).

// If in the algorithm we reach a position which is in the transposition table:
// + If the depth  is more than the one in the table, we return the best move found previously to start searching on that move.
// + If the depth we are going to search (from the position) is less or equal than the one in the table. We have three options:
//  - If the valueType is exact, return value and dont search anymore.
//  - If valueType is a lower bound,
//  - If valueType is a upper bound, 

// TTEntry struct is the transposition table entry, defined as below:
//
// key                                                              16 bit
// depth (max depth - current depth)                                8 bit
// value                                                            16 bit
// best move                                                        16 bit
// is exact (otherwise the turn determines if it lower or upper)    8 or 4 bit (boolean)

struct TTEntry
{

    Move getMove() const { return Move(move); }
    int16_t getValue() const { return value; }
    uint8_t getDepth() const { return depth; }
    int16_t getIsExact() const { return isExact; }
    // Implementation of TTEntry::save
    void save(uint64_t z_k, int16_t v, uint8_t d, Move m, bool type)
    {
        z_key = z_k;
        value = v;
        depth = d;
        move = m;
        isExact = type;
    }

private:
    friend class TranspositionTable;

    uint64_t z_key;
    uint8_t depth;
    Move move;
    int16_t value;
    bool isExact;
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
    TTEntry *probe(uint64_t z_key) const
    {
        if (table == nullptr)
            return nullptr;

        size_t index = z_key % tableSize;
        return (table[index].z_key == z_key) ? &table[index] : nullptr;
    }

    // Save a new entry to the table
    void save(uint64_t z_key, int16_t value, uint8_t depth, Move move, bool isExact)
    {
        size_t index = z_key % tableSize;

        // If the position was already stored we only replace by a deeper depth
        if (table[index].z_key != 0 && table[index].depth < depth) 
            {
                table[index].z_key = z_key;
                table[index].depth = depth;
                table[index].value = value;
                table[index].move = move;
                table[index].isExact = isExact;
            }
        // If the position was not stored, we store it regardless the depth
        else if (table[index].z_key == 0)
        {
            table[index].z_key = z_key;
            table[index].depth = depth;
            table[index].value = value;
            table[index].move = move;
            table[index].isExact = isExact;
        }
    }

    void printTableMemory() const
    {
        size_t entriesInUse = 0;
        for (size_t i = 0; i < tableSize; ++i)
        {
            if (table[i].z_key != 0)
            { // Assuming an unused entry has a key of 0
                ++entriesInUse;
            }
        }

        std::cout << "Table memory: " << tableSize * sizeof(TTEntry) << " bytes\n";
        std::cout << "Entries in use: " << entriesInUse << " out of " << tableSize << "\n";
        std::cout << "Active memory usage: " << entriesInUse * sizeof(TTEntry) << " bytes\n";
    }

private:
    size_t tableSize; // The total number of entries in the table
    TTEntry *table;   // Dynamic array of TTEntry
};

#endif