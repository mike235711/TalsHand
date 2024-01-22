#ifndef MAGICS_H
#define MAGICS_H

#include <vector>
#include <map>
#include <cstdint>

std::vector<std::map<uint64_t, uint64_t>> getBishopLongPrecomputedTable();
std::vector<std::map<uint64_t, uint64_t>> getRookLongPrecomputedTable();

#endif // MAGICS_H