#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <fstream>

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
        // Enumerate every legal move with the AB selectors (which emit the full
        // legal move set, including en-passant, castling and under-promotions) and
        // return the one whose UCI string matches.
        position.setBlockersAndPinsInAB();
        position.setCheckBits();
        Move move;
        if (position.getIsCheck())
        {
            position.setCheckInfo();
            ABMoveSelectorCheck move_selector(position, Move(0));
            move_selector.init();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                if (move.toString() == moveString)
                    return move;
            }
        }
        else
        {
            ABMoveSelectorNotCheck move_selector(position, Move(0));
            move_selector.init_all();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                if (move.toString() == moveString)
                    return move;
            }
        }
        return Move(0);
    }

} // namespace

namespace
{
    std::uint64_t perft_recursive(int depth, BitPosition& pos, TranspositionTable& tt, std::ofstream* outfile, const std::string& prefix)
    {
        // Base case: at depth 0 a single node is counted
        if (depth == 0)
            return 1;

        // Fast path: at depth==1 each legal move contributes exactly one node.
        // We can avoid make/unmake. If outfile is provided, write one line per move.
        if (depth == 1)
        {
            std::uint64_t leaf_nodes = 0;
            Move m;

            {
                pos.setBlockersAndPinsInAB();
                pos.setCheckBits();
                TTEntry *ttEntry = tt.probe(pos.getZobristKey());
                Move tt_move{0};
                if (ttEntry != nullptr) tt_move = ttEntry->getMove();
                if (tt_move.getData() != 0) {
                    if (outfile) {
                        (*outfile) << prefix << tt_move.toString() << ": 1" << std::endl;
                    }
                    ++leaf_nodes;
                }

                if (not pos.getIsCheck())
                {
                    ABMoveSelectorNotCheck sel(pos, tt_move);
                    sel.init_all();
                    while ((m = sel.select_legal()) != Move(0)) {
                        if (outfile) {
                            (*outfile) << prefix << m.toString() << ": 1" << std::endl;
                        }
                        ++leaf_nodes;
                    }
                }
                else
                {
                    pos.setCheckInfo();
                    ABMoveSelectorCheck sel(pos, tt_move);
                    sel.init();
                    while ((m = sel.select_legal()) != Move(0)) {
                        if (outfile) {
                            (*outfile) << prefix << m.toString() << ": 1" << std::endl;
                        }
                        ++leaf_nodes;
                    }
                }
            }

            // Avoid TT writes for pure counting; harmless if left in but slower
            // tt.save(pos.getZobristKey(), 0, depth, Move(0), true);
            return leaf_nodes;
        }

        std::uint64_t total_nodes = 0;
        Move move;
        StateInfo st;

        {
            pos.setBlockersAndPinsInAB();
            pos.setCheckBits();
            TTEntry *ttEntry = tt.probe(pos.getZobristKey());
            Move tt_move{0};
            if (ttEntry != nullptr) tt_move = ttEntry->getMove();
            if (tt_move.getData() != 0)
            {
                pos.makeMove(tt_move, st);

                std::uint64_t child_nodes = perft_recursive(depth - 1, pos, tt, outfile, prefix + tt_move.toString() + " ");
                
                pos.unmakeMove(tt_move);

                if (outfile) {
                    (*outfile) << prefix << tt_move.toString() << ": " << child_nodes << std::endl;
                }
                
                total_nodes += child_nodes;
            }

            if (not pos.getIsCheck())
            {
                ABMoveSelectorNotCheck move_selector(pos, tt_move);
                move_selector.init_all();
                while ((move = move_selector.select_legal()) != Move(0))
                {
                    pos.makeMove(move, st);

                    std::uint64_t child_nodes = perft_recursive(depth - 1, pos, tt, outfile, prefix + move.toString() + " ");
                    
                    pos.unmakeMove(move);

                    if (outfile) {
                        (*outfile) << prefix << move.toString() << ": " << child_nodes << std::endl;
                    }
                    
                    total_nodes += child_nodes;
                }
            }
            else // In check
            {
                pos.setCheckInfo();
                ABMoveSelectorCheck move_selector(pos, tt_move);
                move_selector.init();
                while ((move = move_selector.select_legal()) != Move(0))
                {
                    pos.makeMove(move, st);

                    std::uint64_t child_nodes = perft_recursive(depth - 1, pos, tt, outfile, prefix + move.toString() + " ");
                    
                    pos.unmakeMove(move);

                    if (outfile) {
                        (*outfile) << prefix << move.toString() << ": " << child_nodes << std::endl;
                    }
                    
                    total_nodes += child_nodes;
                }
            }
        }
        
        tt.save(pos.getZobristKey(), 0, depth, Move(0), BOUND_EXACT);
        
        return total_nodes;
    }
}

std::uint64_t THEngine::perftTest(int depth, const std::optional<std::string>& filename)
{
    // If a filename is provided, open the file and start the recursion.
    if (filename) {
        std::ofstream outfile_stream(*filename);
        if (!outfile_stream.is_open()) {
            std::cerr << "Error: Failed to open output file " << *filename << std::endl;
            return 0;
        }
        
        // Start the recursion with an empty prefix string ""
        std::uint64_t total_nodes = perft_recursive(depth, pos, tt, &outfile_stream, "");
        
        // Write the total at the end, just like the Python script.
        outfile_stream << "\nTotal: " << total_nodes << std::endl;
        
        std::cout << "Generated perft data for FEN '" << pos.toFenString() << "' at depth " << depth << " in '" << *filename << "'" << std::endl;
        std::cout << "Total nodes: " << total_nodes << std::endl;

        return total_nodes;
    }

    // If no filename, just run the counter without the file pointer and prefix.
    return perft_recursive(depth, pos, tt, nullptr, "");
}

namespace
{
    // Walk the full AB-legal tree and, at every node, assert that the quiescence
    // capture selectors emit exactly the AB-legal moves classified as captures or
    // queen promotions. Returns the perft node count; increments `mismatches` for
    // every node whose QS capture set differs from the reference.
    std::uint64_t qs_consistency_recursive(int depth, BitPosition &pos, std::uint64_t &mismatches)
    {
        if (depth == 0)
            return 1;

        StateInfo st;
        Move m;

        // Actual: moves emitted by the quiescence *capture* selectors.
        std::vector<std::uint16_t> actual;
        if (pos.getIsCheck())
        {
            pos.setCheckInfo();
            QSMoveSelectorCheck sel(pos);
            sel.init();
            while ((m = sel.select_legal()) != Move(0))
                actual.push_back(m.getData());
        }
        else
        {
            pos.setBlockersPinsAndCheckBitsInQS();
            QSMoveSelectorNotCheck sel(pos);
            sel.init();
            while ((m = sel.select_legal()) != Move(0))
                actual.push_back(m.getData());
        }

        // Reference: every AB-legal move. Set up for making moves last so the
        // recursion below descends with a consistent (AB) position state.
        pos.setBlockersAndPinsInAB();
        pos.setCheckBits();
        std::vector<Move> ab_legal;
        if (pos.getIsCheck())
        {
            pos.setCheckInfo();
            ABMoveSelectorCheck sel(pos, Move(0));
            sel.init();
            while ((m = sel.select_legal()) != Move(0))
                ab_legal.push_back(m);
        }
        else
        {
            ABMoveSelectorNotCheck sel(pos, Move(0));
            sel.init_all();
            while ((m = sel.select_legal()) != Move(0))
                ab_legal.push_back(m);
        }

        // Expected = AB-legal moves that are captures or queen promotions.
        std::vector<std::uint16_t> expected;
        for (Move mv : ab_legal)
            if (pos.isQSCaptureOrQueenProm(mv))
                expected.push_back(mv.getData());

        std::sort(actual.begin(), actual.end());
        std::sort(expected.begin(), expected.end());
        if (actual != expected)
        {
            ++mismatches;
            std::cerr << "QS/AB capture-set mismatch at FEN: " << pos.toFenString() << std::endl;
        }

        std::uint64_t count = 0;
        for (Move mv : ab_legal)
        {
            pos.makeMove(mv, st);
            count += qs_consistency_recursive(depth - 1, pos, mismatches);
            pos.unmakeMove(mv);
        }
        return count;
    }
}

THEngine::QSConsistencyResult THEngine::qsCaptureConsistencyTest(int depth)
{
    std::uint64_t mismatches = 0;
    std::uint64_t nodes = qs_consistency_recursive(depth, pos, mismatches);
    return {nodes, mismatches};
}

constexpr auto STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr int MaxHashMB = 33554432;
std::size_t HardwareCores = std::max<std::size_t>(std::thread::hardware_concurrency(), 1);

int HardThreadCap = 64; // absolute upper bound
int MaxThreads = std::min<int>(HardThreadCap, int(HardwareCores * 4)); // 4× oversubscription at most

// Default net tracks the build width: the width-32 baseline (w32_wdl0, v0.3.8)
// for the default build, or the proven width-8 v4 net (the 0.3.7 baseline) for
// legacy -DNNUEU_FIRST_OUT=8 builds, so a width-8 build reproduces 0.3.7's eval.
constexpr auto DefaultNNUEFile =
    (NNUEU::FIRST_OUT == 512) ? "models/n512_h32/"
    : (NNUEU::FIRST_OUT == 32) ? "models/w32_wdl0/"
                              : "models/NNUEU_quantized_model_v4_param_350_epoch_10/";
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
            std::cout << "id name La Mano de Miguelito\n"
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
            int goDepth = 0;    // "go depth N": fixed-depth search for the introspection harness

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
                else if (kw == "depth")
                {
                    iss >> goDepth;
                }
            }
            if (goDepth > 0)
                goSearchDepth(goDepth); // fixed-depth, prints per-depth info (harness)
            else
            {
                settimeLeft(ourTime, ourInc);
                goSearch();
            }
        }
        else if (token == "stop")
        {
            stopSearch();
        }
        else if (token == "eval")
        {
            // Static NNUEU eval of the current position (raw forwardPass minus the 2048 offset),
            // used to verify the C++ net against the PyTorch/numpy reference oracle.
            static NNUEU::AccumulatorStack evalStack;
            evalStack.reset(pos, *transformer);
            int16_t v = network.evaluate(pos, evalStack, *transformer);
            std::cout << "eval " << static_cast<int>(v) << std::endl;
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
    // ttSize is a hash size in MB (the UCI "Hash" option). resize() takes a *count of
    // entries*, so convert MB -> entries. Previously ttSize was passed straight to
    // resize(), allocating a 16-*entry* table instead of 16 MB — i.e. the transposition
    // table was effectively disabled (≈16 cutoffs in a 30 M-node search).
    size_t entries = (ttSize * 1024ULL * 1024ULL) / sizeof(TTEntry);
    if (entries < 1)
        entries = 1;
    tt.resize(entries);
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
    ourClock = ourTime;
}

void THEngine::goSearch()
{
    resizeThreads();
    // Per-move hard cap for the mid-search abort: never spend more than the remaining base
    // clock minus a safety buffer (max(50ms, 5%)), so the engine cannot flag. With no clock
    // given (analysis), leave it unbounded.
    const int maxMs = (ourClock > 0)
                          ? std::max(10, ourClock - std::max(50, ourClock / 20))
                          : 2147483647;
    threadpool.startThinking(pos, stateInfos, timeLeft, ponder, 99, maxMs);
}

void THEngine::goSearchDepth(int depth)
{
    // Fixed-depth search to exactly `depth` (no time / streak early-stop), used by the
    // search-tree / EBF comparison harness. iterativeSearch prints "info depth … nodes …"
    // per depth plus an "info string … betafirst …" pruning breakdown at the end.
    resizeThreads();
    threadpool.setMainNoEarlyStop(true);
    threadpool.startThinking(pos, stateInfos, std::numeric_limits<int>::max(), ponder, static_cast<int8_t>(depth));
    threadpool.setMainNoEarlyStop(false);
}

void THEngine::stopSearch()
{
    threadpool.stop = true;
}

std::string THEngine::searchFixedDepth(int8_t depth)
{
    std::pair<Move, int16_t> result = threadpool.startThinking(pos, stateInfos, std::numeric_limits<int>::max(), ponder, depth);
    return result.first.toString();
}

std::string THEngine::searchWithTimeConstraint(int timeLimitMs)
{
    std::pair<Move, int16_t> result = threadpool.startThinking(pos, stateInfos, timeLimitMs, ponder, 99);
    return result.first.toString();
}

std::pair<std::string, int16_t> THEngine::searchFixedDepthWithScore(int8_t depth)
{
    std::pair<Move, int16_t> result = threadpool.startThinking(pos, stateInfos, std::numeric_limits<int>::max(), ponder, depth);
    return {result.first.toString(), result.second};
}