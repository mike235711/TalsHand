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
    {
        int s = pos.aBMoveValue(move);
        if (s == 0) // a plain quiet move: killers on top, then by butterfly history
        {
            if (move == killer0)
                s = BitPosition::KILLER_SCORE;
            else if (move == killer1)
                s = BitPosition::KILLER_SCORE - 1;
            else if (history)
                s = history[move.getOriginSquare()][move.getDestinationSquare()];
        }
        move.score = s;
    }
}
void QSMoveSelectorNotCheck::score()
{
    for (auto &move : *this)
        move.score = pos.qSMoveValue(move);
}

// This never returns the TT move, as it was emitted before.
Move QSMoveSelectorCheck::select_legal()
{
    if (cur == endMoves)
        return Move(0);

    // Pins/blockers/checkBits describe the current (parent) position and stay
    // valid across the balanced make/unmake in the QS loop, so compute once.
    if (!pinsReady)
    {
        pos.setBlockersPinsAndCheckBitsInQS();
        pinsReady = true;
    }
    for (; cur < endMoves; ++cur)
    {
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
    if (cur == endMoves)
        return Move(0);

    // Pins/blockers/checkBits describe the current (parent) position and stay
    // valid across the balanced make/unmake in the QS loop, so compute once.
    if (!pinsReady)
    {
        pos.setBlockersPinsAndCheckBitsInQS();
        // Only king and pinned captures can be illegal here; QS never generates
        // en passant, so every other capture is legal exactly as generated.
        needLegalityMask = pos.piecesNeedingLegalityCheck();
        pinsReady = true;
    }

    while (cur < endMoves)
    {
        // Find best remaining move in [cur, endMoves)
        ScoredMove *best = cur;
        for (ScoredMove *p = cur + 1; p < endMoves; ++p)
            if (p->score > best->score)
                best = p;

        // move it to the front (stable O(1) for already-best case)
        if (best != cur)
            std::swap(*best, *cur);

        // Fast legality: non-king, non-pinned captures are always legal.
        if (((1ULL << cur->getOriginSquare()) & needLegalityMask) == 0)
            return *cur++;
        if (pos.isCaptureLegal(cur))
            return *cur++; // success → advance and return

        ++cur; // illegal → drop & try next best
    }
    return Move(0); // no legal capture left
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
    for (;;)
    {
        while (cur < endMoves)
        {
            ScoredMove *m = cur++;
            if (*m == ttMove) // already searched before the selector was built
                continue;
            // Fast legality (Task-1): only king/castling and pinned origins, flagged in
            // needLegalityMask = pinnedPieces | kingBB, need the full isLegal check;
            // every other move is legal as generated (en passant pre-filtered at gen).
            if (((1ULL << m->getOriginSquare()) & needLegalityMask) == 0)
                return *m;
            if (pos.isLegal(m))
                return *m;
        }
        if (stage != 0) // quiets already produced (or there were none) → done
            return Move(0);

        // Deferred stage 2: no capture caused a cutoff, so generate the quiets now.
        // Regenerate the full all-moves list over the (finished) capture slots, drop
        // the captures / queen-promotions it repeats — already searched in stage 1 —
        // then score the surviving quiets (killers boosted) and sort just those.
        ScoredMove *raw = moves;
        raw = pos.pawnAllMoves(raw);
        raw = pos.knightAllMoves(raw);
        raw = pos.bishopAllMoves(raw);
        raw = pos.rookAllMoves(raw);
        raw = pos.queenAllMoves(raw);
        raw = pos.kingAllMoves(raw);
        ScoredMove *w = moves;
        for (ScoredMove *p = moves; p < raw; ++p)
        {
            if (pos.isCaptureStageMove(*p))
                continue; // already searched as a capture in stage 1
            int s = pos.aBMoveValue(*p);
            if (s == 0) // plain quiet: killers on top, then by butterfly history
            {
                if (*p == killer0)
                    s = BitPosition::KILLER_SCORE;
                else if (*p == killer1)
                    s = BitPosition::KILLER_SCORE - 1;
                else if (history)
                    s = history[p->getOriginSquare()][p->getDestinationSquare()];
            }
            *w = *p;
            w->score = s;
            ++w;
        }
        sort_moves(moves, w);
        cur = moves;
        endMoves = w;
        stage = 1;
    }
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
// AB Search (PV nodes) — staged: captures first, quiets generated lazily.
void ABMoveSelectorNotCheck::init_all()
{
    // Cache the pieces that still need a full legality check (king + pinned);
    // every other generated move is legal as-is and skips isLegal in select_legal.
    needLegalityMask = pos.piecesNeedingLegalityCheck();
    cur = endMoves = moves;
    // Stage 1: captures + queen promotions only. Quiets are deferred to select_legal,
    // so a node that cuts off on a capture never generates / scores / sorts them.
    endMoves = pos.pawnCapturesAndQueenProms(endMoves);
    endMoves = pos.knightCaptures(endMoves);
    endMoves = pos.bishopCaptures(endMoves);
    endMoves = pos.rookCaptures(endMoves);
    endMoves = pos.queenCaptures(endMoves);
    endMoves = pos.kingCaptures(endMoves);
    // The capture generators write the QS score; re-score with the AB value so the
    // capture ordering matches the old eager selector, then sort the small list.
    for (ScoredMove *p = cur; p < endMoves; ++p)
        p->score = pos.aBMoveValue(*p);
    sort_moves(cur, endMoves);
    stage = 0;
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