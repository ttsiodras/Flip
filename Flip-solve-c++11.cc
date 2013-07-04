#include <assert.h>

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <list>
#include <tuple>
#include <vector>
#include <bitset>

using namespace std;

// The board is SIZE x SIZE tiles
#define SIZE 5

class Board: public bitset<SIZE*SIZE> {
public:
    inline bool get(int y, int x) const { return operator[](y*SIZE + x); }
    inline reference get(int y, int x) { return operator[](y*SIZE + x); }
    inline bool operator<(const Board& rhs) const {
#ifdef NONOPTIMAL_COMPARISON_PER_BIT
        size_t i = SIZE*SIZE-1;
        while (i > 0) {
            if ((*this)[i-1] == rhs[i-1])
                i--;
            else 
               return (*this)[i-1] < rhs[i-1];
        }
        return false;
#else
        static_assert(
            sizeof(Board)<=sizeof(unsigned long),
            "Board size requires #define NONOPTIMAL_COMPARISON_PER_BIT");
        return this->to_ulong() < rhs.to_ulong();
#endif
    };
};

struct Move {
    enum { Sentinel=0xf };
    unsigned char _yx;
    inline Move(int y, int x):_yx((y<<4)|x) {}
    inline Move(unsigned char yx):_yx(yx) {}
    inline int y() const { return _yx>>4; }
    inline int x() const { return _yx&0xf; }
};

struct ListOfMoves : bitset<SIZE*SIZE> {
    void addMove(int y, int x) {
        (*this)[y*SIZE + x] = true;
    }
    bool moveExists(int y, int x) {
        return (*this)[y*SIZE+x];
    }
};

const pair<int,int> g_offsetsList[] = {
    {0,0}, {0,1}, {0,-1}, {1,0}, {-1,0}
};

// This function pretty-prints a board,
// highlighting a possible move.
void printBoard(const Board& board, Move move)
{
    cout << "+---------------+\n";
    for(int i=0; i<SIZE; i++) {
        cout << "|";
        for(int j=0; j<SIZE; j++) {
            char c = board.get(i,j)?'X':' ';
            if (move.y() == i && move.x() == j)
                cout << '[' << c << ']';
            else
                cout << ' ' << c << ' ';
        }
        cout << "|\n";
    }
    cout << "+---------------+\n";
}

void playMove(Board& board, int y, int x)
{
    for (auto& step: g_offsetsList) {
        int yy = y + step.first;
        int xx = x + step.second;
        if (yy>=0 && yy<SIZE && xx>=0 && xx<SIZE) {
            board.get(yy, xx) = !board.get(yy, xx);
        }
    }
}

// The brains of the operation - basically a Breadth-First-Search
// of the problem space:
//    http://en.wikipedia.org/wiki/Breadth-first_search

//struct BFSNode {
//    unsigned long _u1, _u2;
//    BFSNode(int level, const Move& move, const Board& board, const ListOfMoves& listOfMoves) 
//    {
//        _u1 = board.to_ulong() || (level << 24);
//        _u2 = (move._yx << 24) || listOfMoves._yx;
//    }
//    int getLevel() { return _u1 >> 24; }
//    Move getMove() { return Move(unsigned char(_u2>>24)); }
//    ListOfMoves getListOfMoves() { return ListOfMoves(); }
//};

void SolveBoard(Board& board)
{
    cout << "\nSearching for a solution...";

    // We need to store the last move that got us to a specific
    // board state - that way we can backtrack from a final board
    // state to the list of moves we used to achieve it.
    typedef pair<Board, int> BoardAndLevel;
    map<BoardAndLevel, Move> previousMoves;
    // Start by storing a "sentinel" value, for the initial board
    // state - we used no Move to achieve it, so store a special Move
    // marked up with a sentinel value.
    int oldLevel = 0;
    BoardAndLevel key(board, oldLevel);
    previousMoves.insert(
        pair<BoardAndLevel, Move>(key, Move(Move::Sentinel, Move::Sentinel)));

    // We must not revisit board states we have already examined,
    // so we need a 'visited' set:
    set<Board> visited;

    // Now, to implement Breadth First Search, all we need is a Queue
    // storing the states we need to investigate - so it needs to
    // be a list of board states... We'll also be maintaining
    // the depth we traversed to reach this board state, and the
    // move used to reach it - so we end up with a tuple of
    // int (depth), Move, Board (state), and list of moves so far.
    // We need the last one to make sure we don't toggle the same cell
    // twice (and thus waste a move).
    typedef tuple<int, Move, Board, ListOfMoves> DepthAndMoveAndState;
    list<DepthAndMoveAndState> queue;

    // Start with our initial board state, a depth of 1, a sentinel Move
    // (we used no move to get here) and an empty list of moves so far.
    queue.push_back(
        DepthAndMoveAndState(
            0, Move(Move::Sentinel, Move::Sentinel), board, ListOfMoves()));

    while(!queue.empty()) {

        // Extract first element of the queue
        auto qtop       = *queue.begin();
        auto level      = get<0>(qtop);
        auto move       = get<1>(qtop);
        auto board      = get<2>(qtop);
        auto movesSoFar = get<3>(qtop);
        queue.pop_front();

        // Report depth increase when it happens
        if (level > oldLevel) {
            cout << "Depth searched:   " << level
                 << ", Q: " << queue.size() << endl;
            oldLevel = level;
        }

        // Have we seen this board before?
        if (visited.find(board) != visited.end())
            // Yep - skip it
            continue;

        // No, we haven't - store it so we avoid re-doing
        // the following work again in the future...
        visited.insert(board);

        /* Store board and move, so we can backtrack later */
        BoardAndLevel key(board, oldLevel);
        previousMoves.insert(pair<BoardAndLevel, Move>(key, move));

        // Check if this board state is a winning state:
        // auto it=find_if(board.begin(), board.end(),
        //     [](TileKind& x) { return x == full; });
        //
        // if (it == board.end()) {
        static bitset<SIZE*SIZE> empty;
        if (board == empty) {

            // Yes - we did it!
            cout << "\n\nSolved at depth " << level << "!\n";

            // To print the Moves we used in normal order, we will
            // backtrack through the board states to print
            // the Move we used at each one...
            typedef tuple<Board, Move> BreadCrumb;
            list<BreadCrumb> solution;
            auto itMove = previousMoves.find(BoardAndLevel(board, level));
            while (itMove != previousMoves.end()) {
                //cout << itMove->second.y() << "," << itMove->second.x() << endl;
                if (itMove->second.y() == Move::Sentinel)
                    // Sentinel - reached starting board
                    break;
                // Add this board to the front of the list...
                playMove(board, itMove->second.y(), itMove->second.x());
                solution.push_front(BreadCrumb(board, itMove->second));
                level--;
                itMove = previousMoves.find(BoardAndLevel(board, level));
            }
            // Now that we have the full list, emit it in order
            for(auto& boardAndMove: solution) {
                printBoard(get<0>(boardAndMove), get<1>(boardAndMove));
                cout << "Press ENTER for next move\n";
                cin.get();
            }
            cout << "All done! :-)\n";
            exit(0);
        }

        // Nope, not solved yet
        //
        // Add all potential states arrising from immediate
        // possible moves to the end of the queue.

        for(int i=0; i<SIZE; i++) {
            for(int j=0; j<SIZE; j++) {

                if (movesSoFar.moveExists(i,j))
                    continue;

                for (auto& step: g_offsetsList) {
                    int yy = i + step.first;
                    int xx = j + step.second;
                    if (yy>=0 && yy<SIZE && xx>=0 && xx<SIZE) {
                        if (board.get(yy, xx)) {
                            Board newBoard = board;
                            playMove(newBoard, i, j);

                            if (visited.find(newBoard) == visited.end()) {
                                /* Add to the end of the queue for further study */
                                auto newMovesSoFar = movesSoFar;
                                newMovesSoFar.addMove(i, j);
                                queue.push_back(
                                    DepthAndMoveAndState(
                                        level+1, Move(i, j), newBoard, newMovesSoFar));
                            }
                            break;
                        }
                    }
                }
            }
        }

        // and go recheck the queue, from the top!
    }
}

int main()
{
    Board board;
    //board.get(0,0) = true;
    //board.get(0,4) = true;
    //
    //board.get(0,1) = true; board.get(0,4) = true;
    //board.get(1,3) = true;
    //board.get(2,0) = true; board.get(2,2) = true; board.get(2,4) = true; board.get(3,0) = true;
    //board.get(3,2) = true; board.get(3,3) = true; board.get(3,4) = true;
    //board.get(4,1) = true; board.get(4,3) = true;
    //
    //board.get(0,1) = true; board.get(0,2) = true; board.get(0,4) = true;
    //board.get(1,0) = true; board.get(1,4) = true; 
    //board.get(2,0) = true; board.get(2,4) = true;
    //board.get(3,0) = true; board.get(3,3) = true; board.get(3,4) = true;
    //board.get(4,0) = true; board.get(4,1) = true; board.get(4,3) = true; board.get(4,4) = true;
    //
    //board.get(0,0) = true; board.get(0,4) = true;
    //board.get(1,2) = true; board.get(1,3) = true; board.get(1,4) = true;
    //board.get(2,2) = true; board.get(2,4) = true;
    //board.get(3,0) = true; board.get(3,2) = true; board.get(3,3) = true; board.get(3,4) = true;
    //board.get(4,0) = true; board.get(4,1) = true; board.get(4,2) = true; board.get(4,4) = true;
    //
    //board.get(4,2) = true;
    //board.get(4,3) = true;
    //board.get(4,4) = true;
    //
    board.get(4,0) = true;
    board.get(4,1) = true;
    board.get(4,3) = true;
    board.get(4,4) = true;

    SolveBoard(board);
}
