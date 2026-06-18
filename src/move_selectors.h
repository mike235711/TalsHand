#ifndef MOVE_SELECTORS_H
#define MOVE_SELECTORS_H

#include "bitposition.h"
#include "move.h"

class BitPosition;

//////////////////////////////////////
// Quiescence Search Move Selectors
//////////////////////////////////////

class QSMoveSelectorCheck
{
public:
    QSMoveSelectorCheck(const QSMoveSelectorCheck &) = delete;
    QSMoveSelectorCheck &operator=(const QSMoveSelectorCheck &) = delete;
    QSMoveSelectorCheck(BitPosition & p): pos(p){};
    // Qscence Search
    void init();
    Move select_legal();

private:
    Move *begin() { return cur; }
    Move *end() { return endMoves; }

    BitPosition &pos;
    Move *cur, *endMoves;
    bool pinsReady = false;
    Move moves[64];
};

class QSMoveSelectorNotCheck
{

public:
    QSMoveSelectorNotCheck(const QSMoveSelectorNotCheck &) = delete;
    QSMoveSelectorNotCheck &operator=(const QSMoveSelectorNotCheck &) = delete;
    QSMoveSelectorNotCheck(BitPosition & p) : pos(p) {};
    // Qscence Search
    void init();
    Move select_legal();

private:
    void score();
    ScoredMove *begin() { return cur; }
    ScoredMove *end() { return endMoves; }

    BitPosition &pos;
    ScoredMove *cur, *endMoves;
    bool pinsReady = false;
    uint64_t needLegalityMask = 0; // pinned pieces | side-to-move king: only these need full isCaptureLegal
    ScoredMove moves[128];
};

//////////////////////////////////////
// Alpha-Beta Search Move Selectors
//////////////////////////////////////

class ABMoveSelectorCheck
{
public:
    ABMoveSelectorCheck(const ABMoveSelectorCheck &) = delete;
    ABMoveSelectorCheck &operator=(const ABMoveSelectorCheck &) = delete;
    ABMoveSelectorCheck(BitPosition &p, Move m) : pos(p), ttMove(m) {};
    // AB Search (PV nodes)
    void init();
    Move select_legal();
private:
    Move *begin() { return cur; }
    Move *end() { return endMoves; }

    BitPosition &pos;
    Move ttMove;
    Move *cur, *endMoves;
    Move moves[64];
};

class ABMoveSelectorNotCheck
{
public:
    ABMoveSelectorNotCheck(const ABMoveSelectorNotCheck &) = delete;
    ABMoveSelectorNotCheck &operator=(const ABMoveSelectorNotCheck &) = delete;
    ABMoveSelectorNotCheck(BitPosition & p, Move m, Move k0 = Move(0), Move k1 = Move(0),
                           const int32_t (*hist)[64] = nullptr)
        : pos(p), ttMove(m), killer0(k0), killer1(k1), history(hist) {};
    // AB Search (PV nodes)
    void init_all();
    Move select_legal();

private:
    void score();
    ScoredMove *begin() { return cur; }
    ScoredMove *end() { return endMoves; }

    BitPosition &pos;
    Move ttMove;
    Move killer0, killer1; // quiet killer moves for this ply (Move(0) = none)
    const int32_t (*history)[64]; // butterfly-history sub-table for the side to move (nullptr = none)
    uint64_t needLegalityMask = 0; // pinned pieces | side-to-move king: only these need full isLegal
    int stage = 0;                 // 0 = captures, 1 = quiets (generated lazily), 2 = exhausted
    ScoredMove *cur, *endMoves;
    ScoredMove moves[256];
};

#endif // #ifndef MOVE_SELECTORS_H