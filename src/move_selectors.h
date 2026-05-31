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
    ABMoveSelectorNotCheck(BitPosition & p, Move m) : pos(p), ttMove(m) {};
    // AB Search (PV nodes)
    void init_all();
    Move select_legal();

private:
    void score();
    ScoredMove *begin() { return cur; }
    ScoredMove *end() { return endMoves; }

    BitPosition &pos;
    Move ttMove;
    ScoredMove *cur, *endMoves;
    ScoredMove moves[256];
};

#endif // #ifndef MOVE_SELECTORS_H