#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

#include "engine.h"
#include "bitposition.h"
#include "move_selectors.h"
#include "accumulation.h"

namespace
{
    // Small helpers for trimming input strings
    inline std::string ltrim(std::string s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch)
                                        { return !std::isspace(ch); }));
        return s;
    }
    inline std::string rtrim(std::string s)
    {
        s.erase(std::find_if(s.rbegin(), s.rend(),
                             [](unsigned char ch)
                             { return !std::isspace(ch); })
                    .base(),
                s.end());
        return s;
    }
    inline std::string trim(std::string s) { return ltrim(rtrim(std::move(s))); }

    Move findMoveFromString(std::string moveString, BitPosition &position)
    {
        if (position.getIsCheck())
        {
            position.setCheckInfo();
            position.setBlockersPinsAndCheckBitsInQS();
            // Captures
            Move move;
            QSMoveSelectorCheck move_selector_1(position);
            move_selector_1.init();
            while ((move = move_selector_1.select_legal()) != Move(0))
            {
                if (move.toString() == moveString)
                    return move;
            }
            // Non Captures
            QSMoveSelectorCheckNonCaptures move_selector_2(position);
            move_selector_2.init();
            while ((move = move_selector_2.select_legal()) != Move(0))
            {
                if (move.toString() == moveString)
                    return move;
            }
        }
        else
        {
            position.setBlockersPinsAndCheckBitsInQS();
            // Captures
            Move move;
            QSMoveSelectorNotCheck move_selector_1(position);
            move_selector_1.init();
            while ((move = move_selector_1.select_legal()) != Move(0))
            {
                if (move.toString() == moveString)
                    return move;
            }
            // Non Captures
            QSMoveSelectorNotCheckNonCaptures move_selector_2(position, Move(0));
            move_selector_2.init();
            while ((move = move_selector_2.select_legal()) != Move(0))
            {
                if (move.toString() == moveString)
                    return move;
            }
        }
        return Move(0);
    }

} // namespace

int THEngine::perftTest(int depth, bool quiescent)
{
    if (depth == 0)
    {
        return 1;
    }
    int nodes = 0;
    Move move;
    StateInfo st;

    if (quiescent)
    {
        pos.setBlockersPinsAndCheckBitsInQS();
        if (pos.getIsCheck())
        {
            QSMoveSelectorCheck move_selector(pos);
            move_selector.init();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                pos.makeMove(move, st);
                nodes += perftTest(depth - 1, quiescent);
                pos.unmakeMove(move);
            }
        }
        else
        {
            QSMoveSelectorNotCheck move_selector(pos);
            move_selector.init();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                pos.makeMove(move, st);
                nodes += perftTest(depth - 1, quiescent);
                pos.unmakeMove(move);
            }
        }
    }
    else
    {
        pos.setBlockersAndPinsInAB(); // For discovered checks and move generators
        pos.setCheckBits();           // For direct checks

        // Check if we have stored this position in ttable
        TTEntry *ttEntry = tt.probe(pos.getZobristKey());
        Move tt_move{0};

        // If position is stored in ttable
        if (ttEntry != nullptr)
        {
            tt_move = ttEntry->getMove();
        }

        // Transposition table move search
        if (tt_move.getData() != 0)
        {
            pos.makeMove(tt_move, st);
            nodes += perftTest(depth - 1, quiescent);
            pos.unmakeMove(tt_move);
        }

        // We only search if tt_move didn't produce a cutoff in the search tree
        if (not pos.getIsCheck()) // Not in check
        {
            Move move;
            ABMoveSelectorNotCheck move_selector(pos, tt_move);
            move_selector.init_all();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                pos.makeMove(move, st);
                nodes += perftTest(depth - 1, quiescent);
                pos.unmakeMove(move);
            }
        }
        else // In check
        {
            pos.setCheckInfo();
            Move move;
            ABMoveSelectorCheck move_selector(pos, tt_move);
            move_selector.init();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                pos.makeMove(move, st);
                nodes += perftTest(depth - 1, quiescent);
                pos.unmakeMove(move);
            }
        }
        // Saving a tt value
        tt.save(pos.getZobristKey(), 0, depth, move, true);
    }
    return nodes;
}

constexpr auto STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr int MaxHashMB = 33554432;
std::size_t HardwareCores = std::max<std::size_t>(std::thread::hardware_concurrency(), 1);

int HardThreadCap = 64; // absolute upper bound
int MaxThreads = std::min<int>(HardThreadCap, int(HardwareCores * 4)); // 4× oversubscription at most

constexpr auto DefaultNNUEFile = "models/NNUEU_quantized_model_v4_param_350_epoch_10/";
constexpr std::size_t DefaultHashMB = 16; // Stockfish defaults to 16 MB

THEngine::THEngine(std::optional<std::string> path)
    : pos(), stateInfos(std::make_unique<std::deque<StateInfo>>(1)),
      timeLeft(0), threadpool(), tt(), network(),
      transformer(std::make_unique<NNUEU::Transformer>()),
      numThreads(std::clamp<int>(int(HardwareCores), 1, MaxThreads)),
      ttSize(std::min<std::size_t>(DefaultHashMB, MaxHashMB)),
      ponder(false), NNUEUFile(DefaultNNUEFile)
{
    std::error_code ec;
    if (path && !path->empty()) 
        execDir = std::filesystem::canonical(*path, ec).parent_path();
    if (ec || execDir.empty()) // fall back to CWD on error
        execDir = std::filesystem::current_path();
        pos.fromFen(STARTFEN, &stateInfos->back());
    resizeThreads();
    setTTSize();
    loadNNUEU();
}

void THEngine::readUci()
{
    std::string token;
    while (std::cin >> token)
    {
        if (token == "uci")
        {
            std::cout << "id name TalsHand\n"
                      << "id author Miguel Córdoba\n"
                      << "uciok\n"
                      << std::flush;
        }
        else if (token == "isready")
        {
            std::cout << "readyok\n"
                      << std::flush;
        }
        else if (token == "setoption")
        {
            // Grab the whole line after the word "setoption"
            std::string rest;
            std::getline(std::cin >> std::ws, rest); // ws eats the \n

            // rest now looks like:  "name Hash value 256"
            auto posValue = rest.find(" value ");
            std::string name = trim(rest.substr(5, posValue - 5)); // skip "name "
            std::string value = posValue == std::string::npos ? ""
                                                              : trim(rest.substr(posValue + 7)); // skip " value "

            if (name == "Threads")
            {
                numThreads = std::stoi(value);
                resizeThreads();
            }
            else if (name == "Hash")
            {
                ttSize = std::stoul(value);
                setTTSize();
            }
            else if (name == "EvalFile")
            {
                NNUEUFile = value;
                loadNNUEU();
            }
        }
        else if (token == "position")
        {
            // Entire rest of the line belongs to ‘position’
            std::string rest;
            std::getline(std::cin, rest);
            std::istringstream iss(rest);

            std::string sub;
            iss >> sub;
            if (sub == "startpos")
            {
                std::vector<std::string> moves;
                std::string movesTok;
                if (iss >> movesTok && movesTok == "moves")
                {
                    while (iss >> movesTok)
                        moves.push_back(movesTok);
                }
                setPosition(STARTFEN, moves);
            }
            else if (sub == "fen")
            {
                // FEN = 6 space‑separated fields
                std::string fenPart, fenString;
                int fields = 0;
                while (fields < 6 && iss >> fenPart)
                {
                    fenString += fenPart;
                    if (++fields < 6)
                        fenString += ' ';
                }

                std::vector<std::string> moves;
                std::string movesTok;
                if (iss >> movesTok && movesTok == "moves")
                {
                    while (iss >> movesTok)
                        moves.push_back(movesTok);
                }
                setPosition(trim(fenString), moves);
            }
        }
        else if (token == "go")
        {
            int ourTime = 0;    // milliseconds left on our clock
            int ourInc = 0;     // increment per move

            std::string rest;
            std::getline(std::cin >> std::ws, rest); // read the rest of the line
            std::istringstream iss(rest);

            std::string kw; // keyword inside the “go …” line
            while (iss >> kw)
            {
                if (kw == "wtime" && pos.getTurn())
                {
                    iss >> ourTime;
                }
                else if (kw == "btime" && not pos.getTurn())
                {
                    iss >> ourTime;
                }
                else if (kw == "winc" && pos.getTurn())
                {
                    iss >> ourInc;
                }
                else if (kw == "binc" && not pos.getTurn())
                {
                    iss >> ourInc;
                }
            }
            settimeLeft(ourTime, ourInc);
            goSearch();
        }
        else if (token == "stop")
        {
            stopSearch();
        }
        else if (token == "quit")
        {
            stopSearch();
            break; // Exit the loop and let main() return
        }
        else
        {
            // Unknown token – ignore for forward compatibility
            std::string discard;
            std::getline(std::cin, discard);
        }
    }
}

void THEngine::setPosition(const std::string &fen, const std::vector<std::string> &moves)
{
    // Drop the old state and create a new one
    stateInfos = std::make_unique<std::deque<StateInfo>>(1);
    pos.fromFen(fen, &stateInfos->back());
    bool reseterMove = false;

    for (const auto &move : moves)
    {
        auto m = findMoveFromString(move, pos);

        if (m == Move(0))
            break;
        if (pos.moveIsReseter(m))
            reseterMove = true;
        else
            reseterMove = false;

        stateInfos->emplace_back();
        pos.makeMove(m, stateInfos->back());
    }

    // Transposition table will not be as useful after reseter moves, hence we reset it
    if (reseterMove)
        setTTSize();
}

void THEngine::waitToFinishSearch() 
{ 
}

void THEngine::resizeThreads()
{
    threadpool.waitToFinishSearch();
    threadpool.set(numThreads, tt, network, *transformer);
}

void THEngine::setTTSize()
{
    waitToFinishSearch();
    tt.resize(ttSize);
}

void THEngine::loadNNUEU()
{
    namespace fs = std::filesystem;
    fs::path p{NNUEUFile};
    // If the user gave a relative path, anchor it to the executable’s dir
    if (!p.is_absolute())
        p = execDir / ".." / p; // exe/../models/…
    if (!fs::exists(p))
    {
        std::cerr << "Error: NNUEU weights not found: " << p << '\n';
        std::exit(EXIT_FAILURE);
    }
    transformer->load(p.string());
    network.load(p.string());
    threadpool.clear();
}

void THEngine::settimeLeft(int ourTime, int ourInc)
{
    timeLeft = ourTime + ourInc;
}

void THEngine::goSearch()
{
    resizeThreads();
    threadpool.startThinking(pos, stateInfos, timeLeft, ponder, 99);
}

void THEngine::stopSearch()
{
    threadpool.stop = true;
}

std::string THEngine::searchFixedDepth(int8_t depth)
{
    std::pair<Move, int16_t> result = threadpool.startThinking(pos, stateInfos, timeLeft = 999999, ponder, depth);
    return result.first.toString();
}