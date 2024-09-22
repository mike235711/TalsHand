#ifndef MOVE_H
#define MOVE_H

#include <cstdint>
#include <string>

// Helper functions
static unsigned short algebraicToSquare(const std::string &square)
{
    char file = square[0]; // 'a' to 'h'
    char rank = square[1]; // '1' to '8'

    unsigned short fileIndex = file - 'a'; // 0 to 7
    unsigned short rankIndex = '8' - rank; // 0 to 7, since rank 1 is the 8th row in 0-based index

    return rankIndex * 8 + fileIndex; // Convert to 0-based index
}

static std::string squareToAlgebraic(unsigned short square)
{
    std::string file(1, "abcdefgh"[square % 8]);
    std::string rank = std::to_string(square / 8 + 1);
    return file + rank;
}

// Moves are represented by 16bit integers
//
// bit  0-5: origin square (from 0 to 63)                                  0000000000111111
// bit  6-11: destination square (from 0 to 63)                            0000111111000000
// bit 12-13: promotion (00 knight, 01 bishop, 10 rook, 11 queen)          0011000000000000
// bit 14-15: special move flag (passant/promotion/castling 01, check 10)  1100000000000000

class Move
{
protected:

    uint16_t data;

public:
    Move() : data{0} {}
    explicit Move(uint16_t value) : data{value} {}

    // Neither checks or promotion
    explicit Move(unsigned short origin, unsigned short destination)
        : data{static_cast<uint16_t>(origin | (destination << 6))}
    {
    }
    // Promotions/passant/castling without check
    explicit Move(unsigned short origin, unsigned short destination, unsigned short promotionPiece)
    {
        data = static_cast<uint16_t>(origin | (destination << 6) | (promotionPiece << 12) | 0x4000);
    }
    uint16_t getData() const { return data; }

    unsigned short getOriginSquare() const { return data & 63; }
    unsigned short getDestinationSquare() const { return (data >> 6) & 63; }
    unsigned short getPromotingPiece() const { return (data >> 12) & 3; }

    std::string toString() const
    {
        // Castling move
        if (data == 16772 || data == 16516 || data == 20412 || data == 20156)
        {
            return squareToAlgebraic(getOriginSquare()) + squareToAlgebraic(getDestinationSquare());
        }
        else if ((data & 0x4000) == 0x4000)
        {
            if (getDestinationSquare() <= 7 || getDestinationSquare() >= 56) // Promotions (non passant)
            {
                char promotionChar = 'n';
                switch ((data >> 12) & 0b11)
                {
                case 0b00:
                    promotionChar = 'n';
                    break;
                case 0b01:
                    promotionChar = 'b';
                    break;
                case 0b10:
                    promotionChar = 'r';
                    break;
                case 0b11:
                    promotionChar = 'q';
                    break;
                }
                return squareToAlgebraic(getOriginSquare()) + squareToAlgebraic(getDestinationSquare()) + promotionChar;
            }
        }
        // Rest of moves
        return squareToAlgebraic(getOriginSquare()) + squareToAlgebraic(getDestinationSquare());
    }
};

// Moves which contain a score
struct ScoredMove : public Move
{
    int8_t score;

    // This is so that we can assign efficiently Move object to a *ScoredMove object
    void operator=(Move m) { data = m.getData(); }

    // Constructor to initialize both data (from Move) and score
    ScoredMove(int dataValue = 0, int8_t scoreValue = 0)
        : Move(dataValue), score(scoreValue) {}

};

// For move ordering
inline bool operator<(const ScoredMove &a, const ScoredMove &b) { return a.score < b.score; }

#endif
