#include "move_selectors.h"

#include <array>
#include "bitposition.h"


// Sort moves in descending order.
void sort_moves(ScoredMove *begin, ScoredMove *end)
{
    for (ScoredMove *sortedEnd = begin, *p = begin + 1; p < end; ++p)
    {
        ScoredMove tmp = *p, *q;
        *p = *++sortedEnd;
        for (q = sortedEnd; q != begin && *(q - 1) < tmp; --q)
            *q = *(q - 1);
        *q = tmp;
    }
}

// Assigns a score to each move, used for sorting.
void ABMoveSelectorNotCheck::score()
{
    for (auto &move : *this)
        move.score = pos.aBMoveValue(move);
}
void QSMoveSelectorNotCheck::score()
{
    for (auto &move : *this)
        move.score = pos.qSMoveValue(move);
}

// This never returns the TT move, as it was emitted before.
Move QSMoveSelectorCheck::select_legal()
{
    for (; cur < endMoves; ++cur)
    {
        // We only set blockers once if there is a legal move
        if (pos.hasBlockersUnset())
            pos.setBlockersPinsAndCheckBitsInQS();
        // If move is not legal we skip it
        if (pos.isCaptureLegal(cur))
        {
            return *cur++;
        }
    }

    return Move(0);
}
Move QSMoveSelectorNotCheck::select_legal()
{
    while (cur < endMoves)
    {
        // --- find best remaining move in [cur, endMoves) -------------------
        ScoredMove *best = cur;
        for (ScoredMove *p = cur + 1; p < endMoves; ++p)
            if (p->score > best->score)
                best = p;

        // move it to the front (stable O(1) for already-best case)
        if (best != cur)
            std::swap(*best, *cur);

        // --- legality test -------------------------------------------------
        if (pos.hasBlockersUnset())
            pos.setBlockersPinsAndCheckBitsInQS();

        if (pos.isCaptureLegal(cur))
            return *cur++; // success → advance and return

        ++cur; // illegal → drop & try next best
    }

    return Move(0); // no legal capture left
}
Move QSMoveSelectorCheckNonCaptures::select_legal()
{
    for (; cur < endMoves; ++cur)
    {
        // If move is not legal we skip it
        if (pos.isLegal(cur))
        {
            // std::cout << cur->toString() << "\n";
            return *cur++;
        }
        // else
        //     std::cout << cur->toString() << "\n";
    }
    return Move(0);
}
Move QSMoveSelectorNotCheckNonCaptures::select_legal()
{
    for (; cur < endMoves; ++cur)
        if (*cur != ttMove)
        {
            // If move is not legal we skip it
            if (pos.isLegal(cur))
                return *cur++;
        }
    return Move(0);
}
Move ABMoveSelectorCheck::select_legal()
{
    for (; cur < endMoves; ++cur)
        if (*cur != ttMove)
        {
            // If move is not legal we skip it
            if (pos.isLegal(cur))
                return *cur++;
        }
    return Move(0);
}
Move ABMoveSelectorNotCheck::select_legal()
{
    for (; cur < endMoves; ++cur)
        if (*cur != ttMove)
        {
            // If move is not legal we skip it
            if (pos.isLegal(cur))
                return *cur++;
        }
    return Move(0);
}

// Qscence Search
void QSMoveSelectorNotCheck::init()
{
    cur = endMoves = moves;

    // generate every capture
    endMoves = pos.pawnCapturesAndQueenProms(endMoves);
    endMoves = pos.knightCaptures(endMoves);
    endMoves = pos.bishopCaptures(endMoves);
    endMoves = pos.rookCaptures(endMoves);
    endMoves = pos.queenCaptures(endMoves);
    endMoves = pos.kingCaptures(endMoves);

    // write scores but DON’T sort – we’ll pick lazily
    // for (ScoredMove *p = moves; p < endMoves; ++p)
    //     p->score = pos.qSMoveValue(*p);
}
void QSMoveSelectorCheck::init()
{
    cur = endMoves = moves;
    if (pos.moreThanOneCheck())
        endMoves = pos.kingCaptures(endMoves);
    else
        endMoves = pos.inCheckOrderedCaptures(endMoves);
}
void QSMoveSelectorNotCheckNonCaptures::init()
{
    cur = endMoves = moves;
    endMoves = pos.pawnNonCapturesNonQueenProms(endMoves);
    endMoves = pos.knightNonCaptures(endMoves);
    endMoves = pos.bishopNonCaptures(endMoves);
    endMoves = pos.rookNonCaptures(endMoves);
    endMoves = pos.queenNonCaptures(endMoves);
    endMoves = pos.kingNonCaptures(endMoves);
}
void QSMoveSelectorCheckNonCaptures::init()
{
    cur = endMoves = moves;
    if (pos.moreThanOneCheck())
        endMoves = pos.kingNonCapturesInCheck(endMoves);
    else
    {
        if (pos.sliderChecking())
        {
            endMoves = pos.inCheckPawnBlocksNonQueenProms(endMoves);
            endMoves = pos.inCheckPawnCapturesNonQueenProms(endMoves);
            endMoves = pos.inCheckKnightBlocks(endMoves);
            endMoves = pos.inCheckBishopBlocks(endMoves);
            endMoves = pos.inCheckRookBlocks(endMoves);
            endMoves = pos.inCheckQueenBlocks(endMoves);
            endMoves = pos.kingNonCapturesInCheck(endMoves);
        }
        else // Passant captures and king moves
        {
            endMoves = pos.inCheckPawnCapturesNonQueenProms(endMoves);
            endMoves = pos.inCheckPassantCaptures(endMoves);
            endMoves = pos.kingNonCapturesInCheck(endMoves);
        }
    }
}
// AB Search (PV nodes)
void ABMoveSelectorNotCheck::init_all()
{
    cur = endMoves = moves;
    endMoves = pos.pawnAllMoves(endMoves);
    endMoves = pos.knightAllMoves(endMoves);
    endMoves = pos.bishopAllMoves(endMoves);
    endMoves = pos.rookAllMoves(endMoves);
    endMoves = pos.queenAllMoves(endMoves);
    endMoves = pos.kingAllMoves(endMoves);
    score();
    sort_moves(cur, endMoves);
}
// AB Search (Non PV nodes)
void ABMoveSelectorNotCheck::init_refutations()
{
    cur = endMoves = moves;
    endMoves = pos.setRefutationMovesOrdered(endMoves);
}
void ABMoveSelectorNotCheck::init_good_captures()
{
    cur = endMoves = moves;
    endMoves = pos.setGoodCapturesOrdered(endMoves);
}
void ABMoveSelectorNotCheck::init_rest()
{
    endMoves = pos.pawnRestMoves(endMoves);
    endMoves = pos.knightRestMoves(endMoves);
    endMoves = pos.bishopRestMoves(endMoves);
    endMoves = pos.rookRestMoves(endMoves);
    endMoves = pos.queenRestMoves(endMoves);
    endMoves = pos.kingNonCapturesAndPawnCaptures(endMoves);
}
// AB Search in check
void ABMoveSelectorCheck::init()
{
    cur = endMoves = moves;
    if (pos.moreThanOneCheck()) // Only king moves
        endMoves = pos.kingAllMovesInCheck(endMoves);
    else
    {
        if (pos.sliderChecking()) // Captures in check, king moves and blocks
        {
            endMoves = pos.inCheckOrderedCapturesAndKingMoves(endMoves);
            endMoves = pos.inCheckPawnBlocks(endMoves);
            endMoves = pos.inCheckKnightBlocks(endMoves);
            endMoves = pos.inCheckBishopBlocks(endMoves);
            endMoves = pos.inCheckRookBlocks(endMoves);
            endMoves = pos.inCheckQueenBlocks(endMoves);
        }
        else // Captures in check and king moves
        {
            endMoves = pos.inCheckOrderedCapturesAndKingMoves(endMoves);
        }
    }
}