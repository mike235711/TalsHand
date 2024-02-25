# Tals Hand [In progress]
In this project I will create a chess engine from scratch. Classical chess engines usually consist of 3 main parts:

* A move generator, generating all legal moves in a chess position
* A function that evaluates positions (Possibly a neural network)
* An algorithm that searches in the variations and chooses the best move

I have created my own move generator, taking into account all the rules of chess and implementing bitboards (representing chess boards using length 64 bits). Speed in this part of the engine is essential because the engine will have to calculate many variations and chess is usually played with time limtis.

When evaluating positions, my idea is to use a neural network. Since speed is very important we can't use any neural network to evaluate positions, there are a special type of nerual networks called efiiciently updatable neural networks (NNUE) which are fast enough for chess engines and can be trained to evaluate positions to a very high level. I am currently on the training phase.

There are many choices of search algorithms for chess engines (in general most 2 player games), however alpha-beta pruning is the most efficient, because it prunes branches of the tree search that one doesn't need to search. 

On top of the alpha beta search I have added a score to moves depending on capturing pieces, pawns and making checks. The algorithm is contructed such that it considers high score moves first, making it faster because it prunes more branches. 

I have also applied quiescence search so that in tactical positions (when there are possible captures) it makes captures (I want to include some checks) until a quiet position is reached.

The engine updates internally a Zobrist key each time a move is made, these keys correspond to the positions almost uniquely. The keys allow to efficiently check for threefold repetitions, and a transposition table in which the evaluation, best move and move type (see ttable.h) are stored for each zobrist key.

To play a game against the engine, download this repository and you can load the engine to any UCI compatible chess GUI (such as BANKSIAGUI).

Hope you enjoy and beat the engine :)


To do:
* NNUE (Training)
* Late move reductions
* Futility pruning
* Search extensions
* Pruning at shallow depth
* 50 move rule
* Killer moves
* Null move pruning
* Time manager
