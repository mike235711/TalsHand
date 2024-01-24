#ifndef MOVE_H
#define MOVE_H

#include <cstdint> // For fixed-sized integers

// Moves are represented by 16bit integers
//
// bit  0- 5: origin square (from 0 to 63)                            0000000000111111
// bit  6-11: destination square (from 0 to 63)                       0000111111000000
// bit 12-14: moving piece:
// (000 pawn, 001 knight, 010 bishop, 011 rook, 100 queen, 101 king)  0111000000000000
// bit 14-15: safe_move_flag (1 safe, 0 unsafe)                       1000000000000000

class Move
{
private:
    uint16_t data; // Store the move data

    static std::string squareToAlgebraic(unsigned short square)
    {
        std::string file(1, "abcdefgh"[square % 8]); // Convert the char to a std::string
        std::string rank = std::to_string(square / 8 + 1);
        return file + rank;
    }

public:
    Move() : data{0} {}             // Default constructor
    explicit Move(uint16_t value) : // Bit Constructor
                                    data{value}
    {
    }

    explicit Move(unsigned short origin, unsigned short destination, unsigned short moving_piece) : // Normal move constructor
                                                                                                    data{static_cast<uint16_t>(origin | (destination << 6) | (moving_piece << 12))}
    {
    }

    explicit Move(unsigned short origin, unsigned short destination, unsigned short moving_piece, unsigned short safety) : // Normal move constructor (Non promotion, capture, castling or double pawn move)
                                                                                                                           data{static_cast<uint16_t>(origin | (destination << 6) | (moving_piece << 12) | (safety << 16))}
    {
    }

    // Member functions

    uint16_t getData() const { return data; }

    unsigned short getOriginSquare() const
    {
        return data & 63;
    }
    unsigned short getDestinationSquare() const
    {
        return (data >> 6) & 63;
    }
    bool isSafe() const
    {
        return ((data & 0b1000000000000000) != 0);
    }
    unsigned short movingPiece() const
    {
        return ((data >> 12) & 0b0111);
    }
    std::string toString() const
    {
        return squareToAlgebraic(getOriginSquare()) + squareToAlgebraic(getDestinationSquare());
    }
};

// Captures are represented by 16bit integers
//
// bit  0- 5: origin square (from 0 to 63)                                  0000000000111111
// bit  6-11: destination square (from 0 to 63)                             0000111111000000
// bit 12-13: promotion (00 knight, 01 bishop, 10 rook, 11 queen)           0011000000000000
// bit 14-15: special move flag:
// (promotion 11, bad capture 00, good capture 10, promotion&capture 01)    1100000000000000

class Capture
{
private:
    uint16_t data; // Store the move data

    static std::string squareToAlgebraic(unsigned short square)
    {
        std::string file(1, "abcdefgh"[square % 8]); // Convert the char to a std::string
        std::string rank = std::to_string(square / 8 + 1);
        return file + rank;
    }

public:
    explicit Capture(uint16_t value) : // Bit Constructor
                                       data{value}
    {
    }

    explicit Capture(unsigned short origin, unsigned short destination, unsigned short flag) : data{static_cast<uint16_t>(origin | (destination << 6) | (flag << 16))}
    {
    }
    explicit Capture(unsigned short origin, unsigned short destination, unsigned short flag, unsigned short promotion_type) : data{static_cast<uint16_t>(origin | (destination << 6) | (flag << 16) | (promotion_type << 12))}
    {
    }

    // Member functions

    uint16_t getData() const { return data; }

    bool isntPromotion() const
    {
        return ((data & 16384) != 0);
    }

    unsigned short getPromotionType() const
    {
        return (data >> 14) & 0b011;
    }

    unsigned short getOriginSquare() const
    {
        return data & 63;
    }
    unsigned short getDestinationSquare() const
    {
        return (data >> 6) & 63;
    }
    std::string toString() const
    {
        return squareToAlgebraic(getOriginSquare()) + squareToAlgebraic(getDestinationSquare());
    }
};
#endif
