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

static unsigned short pieceTypeFromChar(char promotionChar)
{
    switch (promotionChar)
    {
    case 'n':
        return 0b00; // Knight
    case 'b':
        return 0b01; // Bishop
    case 'r':
        return 0b10; // Rook
    case 'q':
        return 0b11; // Queen
    default:
        return 0;
    }
}

// Moves are represented by 16bit integers
//
// bit  0-5: origin square (from 0 to 63)                                            0000000000111111
// bit  6-11: destination square (from 0 to 63)                                      0000111111000000
// bit 12-13: promotion (00 knight, 01 bishop, 10 rook, 11 queen)                    0011000000000000
// bit 14-15: special move flag (promotion/castling 01, pawn double 10, capture 11)  1100000000000000

class Move
{
private:
    uint16_t data;

    static std::string squareToAlgebraic(unsigned short square)
    {
        std::string file(1, "abcdefgh"[square % 8]);
        std::string rank = std::to_string(square / 8 + 1);
        return file + rank;
    }

public:
    Move() : data{0} {}
    explicit Move(uint16_t value) : data{value} {}
    explicit Move(unsigned short origin, unsigned short destination)
        : data{static_cast<uint16_t>(origin | (destination << 6))}
    {
    }
    explicit Move(unsigned short origin, unsigned short destination, unsigned short promotionPiece)
    {
        data = static_cast<uint16_t>(origin | (destination << 6) | (promotionPiece << 12) | 0x4000);
    }

    uint16_t getData() const { return data; }

    unsigned short getOriginSquare() const { return data & 63; }
    unsigned short getDestinationSquare() const { return (data >> 6) & 63; }

    std::string toString() const
    {
        // Castling move
        if (data == 16772 || data == 16516 || data == 20412 || data == 20156)
        {
            return squareToAlgebraic(getOriginSquare()) + squareToAlgebraic(getDestinationSquare());
        }
        // Promotion move
        else if ((data & 0xC000) == 0x4000)
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
        // Rest of moves
        return squareToAlgebraic(getOriginSquare()) + squareToAlgebraic(getDestinationSquare());
    }
};

// Captures are represented by 16bit integers
//
// bit  0-5: origin square (from 0 to 63)           0000000000111111
// bit  6-11: destination square (from 0 to 63)     0000111111000000
// bit 12-13: moving piece/promoting piece          0111000000000000
// (000 pawn, 001 knight, 010 bishop, 011 rook, 100 queen, 101 king)
// bit 14-15: promotion flag                        1000000000000000
class Capture
{
private:
    uint16_t data;

    static std::string squareToAlgebraic(unsigned short square)
    {
        std::string file(1, "abcdefgh"[square % 8]);
        std::string rank = std::to_string(square / 8 + 1);
        return file + rank;
    }

public:
    Capture() : data{0} {}
    explicit Capture(uint16_t value) : data{value} {}
    explicit Capture(unsigned short origin, unsigned short destination, unsigned short movingPiece)
        : data{static_cast<uint16_t>(origin | (destination << 6) | (movingPiece << 12))}
    {
    }
    explicit Capture(unsigned short origin, unsigned short destination, unsigned short movingPiece, unsigned short promotionFlag)
        : data{static_cast<uint16_t>(origin | (destination << 6) | (movingPiece << 12) | (promotionFlag << 15))}
    {
    }

    uint16_t getData() const { return data; }

    unsigned short getOriginSquare() const { return data & 63; }
    unsigned short getDestinationSquare() const { return (data >> 6) & 63; }
    unsigned short getMovingOrPromotingPiece() const { return (data >> 12) & 7; }
    bool isPromotion() const { return (data >> 15) & 1; }

    std::string toString() const
    {
        if (isPromotion())
        {
            char promotionChar = 'n';
            switch (getMovingOrPromotingPiece())
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
        return squareToAlgebraic(getOriginSquare()) + squareToAlgebraic(getDestinationSquare());
    }
};

#endif
