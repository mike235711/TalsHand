#ifndef NNUE_TTABLE_H
#define NNUE_TTABLE_H

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring> // For std::memset

// The NNUE transposition table will store the zobrist keys of seen positions and the NNUE value.
//
// TTNNUEEntry struct is the transposition table entry, defined as below:
//
// key                                                              16 bit
// value                                                            float

struct TTNNUEEntry // DONE
{

    float getValue() const { return value; }
    // Implementation of TTEntry::save
    void save(uint64_t z_k, float v)
    {
        z_key = z_k;
        value = v;
    }

private:
    friend class TranspositionTableNNUE;

    uint64_t z_key;
    float value;
};

class TranspositionTableNNUE // DONE
{
public:
    TranspositionTableNNUE() : tableSize(0), table(nullptr) {}
    ~TranspositionTableNNUE() { delete[] table; }

    // Initializes or resizes the table to a number of entries, which is a power of two
    void resize(size_t newSize)
    {
        delete[] table;
        tableSize = newSize;
        table = new TTNNUEEntry[newSize];
        std::memset(table, 0, newSize * sizeof(TTNNUEEntry));
    }

    // Probes the table for a given key. Returns a pointer to the TTEntry if found, otherwise nullptr.
    TTNNUEEntry *probe(uint64_t z_key) const
    {
        if (table == nullptr)
            return nullptr;

        size_t index = z_key % tableSize;
        return (table[index].z_key == z_key) ? &table[index] : nullptr;
    }

    // Save a new entry to the table
    void save(uint64_t z_key, float value)
    {
        size_t index = z_key % tableSize;
        table[index].save(z_key, value);
    }

    void printTableMemory()
    {
        size_t entriesInUse = 0;
        for (size_t i = 0; i < tableSize; ++i)
        {
            if (table[i].z_key != 0)
            { // Assuming an unused entry has a key of 0
                ++entriesInUse;
            }
        }

        std::cout << "Table memory: " << tableSize * sizeof(TTNNUEEntry) << " bytes\n";
        std::cout << "Entries in use: " << entriesInUse << " out of " << tableSize << "\n";
        std::cout << "Active memory usage: " << entriesInUse * sizeof(TTNNUEEntry) << " bytes\n";
    }

private:
    size_t tableSize;   // The total number of entries in the table
    TTNNUEEntry *table; // Dynamic array of TTEntry
};

#endif // NNUE_TTABLE_H