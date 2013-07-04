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
        size_t i = SIZE*SIZE-1;
        while (i > 0) {
            if ((*this)[i-1] == rhs[i-1])
                i--;
            else 
               return (*this)[i-1] < rhs[i-1];
        }
        return false;
    };
};

//
// Older form, wasted memory (obsolete)
//
//struct Move {
//    int _y, _x;
//    Move(int y, int x):_y(y), _x(x) {}
//    int y() const { return _y; }
//    int x() const { return _x; }
//};
struct Move {
    static const int Sentinel;
    unsigned char _yx;
    inline Move(int y, int x):_yx((y<<4)|x) {}
    inline int y() const { return _yx>>4; }
    inline int x() const { return _yx&0xf; }
};
const int Move::Sentinel=0xf;

struct ListOfMoves {
    unsigned char _yx[SIZE*SIZE+1];
    ListOfMoves() {
        _yx[0] = 1;
    }
    void addMove(const Move& m) {
        unsigned idx = _yx[0];
        assert (idx<SIZE*SIZE+1);
        _yx[idx++] = m.y() << 4 | m.x();
        _yx[0] = idx;
    }
    bool moveExists(int y, int x) {
        for(int i=1; i<_yx[0]; i++) {
            if ((_yx[i]>>4) == y && (_yx[i]&0xf) == x)
                return true;

        }
        return false;
    }
};

const pair<int,int> g_offsetsList[] = {
    {0,0}, {0,1}, {0,-1}, {1,0}, {-1,0}
};

int g_limit = 6;

// This function pretty-prints a list of blocks
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
//
void SolveBoard(Board& board)
{
    cout << "\nSearching for a solution with up to ";
    cout << g_limit;
    cout << " tiles set..." << endl;

    // We need to store the last move that got us to a specific
    // board state - that way we can backtrack from a final board
    // state to the list of moves we used to achieve it.
    typedef pair<Board, int> BoardAndLevel;
    map<BoardAndLevel, Move> previousMoves;
    // Start by storing a "sentinel" value, for the initial board
    // state - we used no Move to achieve it, so store a block id
    // of -1 to mark it:
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
    // move to perform - so we end up with a tuple of
    // int (depth), Move, list of blocks (state).
    typedef tuple<int, Move, Board, ListOfMoves> DepthAndMoveAndState;
    list<DepthAndMoveAndState> queue;

    // Start with our initial board state, and playedMoveDepth set to 1
    queue.push_back(DepthAndMoveAndState(1, Move(Move::Sentinel, Move::Sentinel), board, ListOfMoves()));

    while(!queue.empty()) {

        // Extract first element of the queue
        auto qtop = *queue.begin();
        auto level = get<0>(qtop);
        auto move = get<1>(qtop);
        auto board = get<2>(qtop);
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
            cout << "\n\nSolved!\n";

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

                bool bFoundFullTileCloseBy = false;
                for (auto& step: g_offsetsList) {
                    int yy = i + step.first;
                    int xx = j + step.second;
                    if (yy>=0 && yy<SIZE && xx>=0 && xx<SIZE) {
                        if (board.get(yy, xx)) {
                            bFoundFullTileCloseBy = true;
                            break;
                        }
                    }
                }
                if (bFoundFullTileCloseBy) {
                    Board newBoard = board;
                    playMove(newBoard, i, j);

                    /*
                    int totalFull = 0;
                    for(int ii=0; ii<SIZE*SIZE; ii++)
                        totalFull += newBoard[ii]?1:0;
                    if (totalFull>=g_limit)
                        continue;
                    */

                    if (visited.find(newBoard) == visited.end()) {
                        /* Add to the end of the queue for further study */
                        auto newMovesSoFar = movesSoFar;
                        newMovesSoFar.addMove(Move(i, j));
                        queue.push_back(
                            DepthAndMoveAndState(
                                level+1, Move(i, j), newBoard, newMovesSoFar));
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
    board.get(0,0) = true; board.get(0,4) = true;
    board.get(1,2) = true; board.get(1,3) = true; board.get(1,4) = true;
    board.get(2,2) = true; board.get(2,4) = true;
    board.get(3,0) = true; board.get(3,2) = true; board.get(3,3) = true; board.get(3,4) = true;
    board.get(4,0) = true; board.get(4,1) = true; board.get(4,2) = true; board.get(4,4) = true;

    for (g_limit=6; g_limit<SIZE*SIZE; g_limit++)
        SolveBoard(board);
}
