#include <assert.h>

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <set>
#include <list>
#include <tuple>
#include <bitset>

using namespace std;

// The board is SIZE x SIZE tiles
const int SIZE = 5;

// I quickly found out that the puzzle space is big, and memory usage becomes
// an issue...  This define toggles the original unoptimized code back in:
//#define NONOPTIMAL_CODE

#ifdef NONOPTIMAL_CODE
// In the unoptimized version, the board was represented as a vector
// of SIZE*SIZE TileKind instances:
enum TileKind {
    Empty = 0,
    Full  = 1
};

struct Board: public vector<TileKind> {
    Board():vector<TileKind>(SIZE*SIZE) {}
    inline TileKind  get(int y, int x) const { return (*this)[y*SIZE + x]; }
    inline TileKind& get(int y, int x)       { return (*this)[y*SIZE + x]; }
};

// ...and each move was stored as the y,x coordinates of the tile toggled:
struct Move {
    enum { Sentinel=0xf };
    unsigned int _y, _x;
    inline Move(int y, int x):_y(y), _x(x) {}
    inline int y() const { return _y; }
    inline int x() const { return _x; }
};
#else

// In the optimized version, bitsets are used to represent boards. Basically,
// this means that an unsigned long stores the two possible tile states
// (Full/Empty) of each tile, storing each tile in one bit.
//
// The difference in memory usage is at least an order of magnitude...
struct Board: public bitset<SIZE*SIZE> {
    Board(unsigned long v=0):bitset<SIZE*SIZE>(v) {}
    inline bool get(int y, int x) const { return (*this)[y*SIZE + x]; }
    inline reference get(int y, int x)  { return (*this)[y*SIZE + x]; }

    // The Board will be used as a key in a set (visited)
    // In contrast to the vector used in the non-optimized version,
    // the bitset does not provide a default comparison operator,
    // so we had to provide our own:
    inline bool operator<(const Board& rhs) const {
        //size_t i = SIZE*SIZE-1;
        //while (i > 0) {
        //    if ((*this)[i-1] == rhs[i-1])
        //        i--;
        //    else
        //       return (*this)[i-1] < rhs[i-1];
        //}
        //return false;
        //
        // Instead of comparing the bits one by one, we can compare
        // the contained unsigned long - this gave a 100% speedup:
        return to_ulong() < rhs.to_ulong();
    };
};
// For this optimization via to_ulong() to work, the board size must fit
// within an unsigned long - verify this at compile-time:
static_assert(
    sizeof(Board)<=sizeof(unsigned long),
    "Uncomment the full comparison in Board::operator<");

// Finally, squeeze some more memory out of the Move, by storing both
// x and y in a single unsigned char's 8 bits (4 for x, 4 for y):
struct Move {
    enum { Sentinel=0xf };
    unsigned char _yx;
    inline Move(int y, int x):_yx((y<<4)|x) {}
    inline Move(unsigned char yx):_yx(yx)   {}
    inline int y() const                    { return _yx>>4;  }
    inline int x() const                    { return _yx&0xf; }
};
#endif

// We will need to store the moves done so far, to avoid redoing them,
// Since memory is at a premium, use a bitset:
struct ListOfMoves : bitset<SIZE*SIZE> {
    ListOfMoves(unsigned long v=0):bitset<SIZE*SIZE>(v) {}

    void addMove(int y, int x)           { (*this)[y*SIZE + x] = true; }
    bool moveAlreadyPlayed(int y, int x) { return (*this)[y*SIZE+x]; }
};

// The offsets of tiles to toggle when clicking on a tile:
const pair<int,int> g_offsetsList[] = {
    {0,0}, {0,1}, {0,-1}, {1,0}, {-1,0}
};

// Pretty-print a board, highlighting a possible move:
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

// Modify a board, by playing a move:
void playMove(Board& board, int y, int x)
{
    for (auto& step: g_offsetsList) {
        int yy = y + step.first;
        int xx = x + step.second;
        if (yy>=0 && yy<SIZE && xx>=0 && xx<SIZE) {
#ifdef NONOPTIMAL_CODE
            board.get(yy, xx) = board.get(yy, xx)==Full?Empty:Full;
#else
            board.get(yy, xx) = !board.get(yy, xx);
#endif
        }
    }
}

// The state to store for each move in the Breadth-First-Search:
#ifdef NONOPTIMAL_CODE
// Initially, I just used a tuple of 4 things:
// - The depth in the move tree that we are currently in
// - The Move we performed to reach the current state
// - The Board's current state
// - The moves we've played to get here (bitset)
typedef tuple<int, Move, Board, ListOfMoves> State;

#else
// Again, I had to optimize for memory use - packing all info in 2 ulong:
struct State {
    unsigned long _u1, _u2;
    State(
        int level,
        const Move& move,
        const Board& board,
        const ListOfMoves& listOfMoves)
    {
        _u1 = (unsigned(level&0x1f) << 25) | board.to_ulong();
        _u1 |= ((unsigned(move._yx) & 0x80) << 24);
        _u2 = (move._yx << 25)             | listOfMoves.to_ulong();
    }

    int getLevel()               { return (int)((_u1 >> 25)&0x1F); }
    Move getMove()               { return Move(_u2>>25 | ((_u1&0x80000000) >> 24)); }
    Board getBoard()             { return Board(_u1 & 0x1FFFFFF); }
    ListOfMoves getListOfMoves() { return ListOfMoves(_u2 & 0x1FFFFFF); }
};
#endif

// ...and finally, the brains of the operation:
// a Breadth-First-Search of the problem space.
//  ( http://en.wikipedia.org/wiki/Breadth-first_search )

void SolveBoard(Board& board)
{
    cout << "\nSearching for a solution...\n\n";

    // We need to store the last move that got us to a specific
    // board state - that way we can backtrack from a final board
    // state to the list of moves we used to achieve it.
    typedef pair<Board, int> BoardAndLevel;
    map<BoardAndLevel, Move> previousMoves;

    // We start by storing a "sentinel" value, for the initial board
    // state - we used no Move to achieve it, so store a special Move
    // marked up with the sentinel value for y,x:
    unsigned char oldLevel = 0;
    BoardAndLevel key(board, oldLevel);
    previousMoves.insert(
        pair<BoardAndLevel, Move>(key, Move(Move::Sentinel, Move::Sentinel)));

    // We must not revisit board states we have already examined,
    // so we need a 'visited' set:
    set<Board> visited;

    // Now, to implement Breadth First Search, all we need is a Queue
    // storing the states we need to investigate - so it needs to
    // be a list of board states... We'll be maintaining
    // the depth we traversed to reach this board state, the board itself,
    // and the move used to reach it - so we end up with a tuple of
    // int (depth), Move, Board. Since we don't want to re-toggle a tile we've
    // already toggled (and thus waste a move), we also need a list of moves
    // played so far.
    //
    // That's what the State class stores inside it:
    list<State> queue;
    // Start the Q with our initial board state, depth, a sentinel Move
    // (we used no move to get here) and an empty list of moves so far.
    queue.push_back(
        State(0, Move(Move::Sentinel, Move::Sentinel), board, ListOfMoves(0)));

    while(!queue.empty()) {

        // Extract first element of the queue
        auto qtop       = *queue.begin();
#ifdef NONOPTIMAL_CODE
        // The initial version used a tuple, so it uses the get<> template:
        auto level      = get<0>(qtop);
        auto move       = get<1>(qtop);
        auto board      = get<2>(qtop);
        auto movesSoFar = get<3>(qtop);
#else
        // The optimized version extracts the info from the bits of the two
        // unsigned longs kept inside each State instance:
        auto level      = qtop.getLevel();
        auto move       = qtop.getMove();
        auto board      = qtop.getBoard();
        auto movesSoFar = qtop.getListOfMoves();
#endif
        queue.pop_front();

        if (level > oldLevel) {
            // Report depth increase when it happens:
            cout << "Depth searched: " << setw(2) << level
                 << ", states to check in Q: " << queue.size() << endl;
            oldLevel = level;
        }

        // Have we seen this board before?
        if (visited.find(board) != visited.end())
            // Yep - skip it
            continue;

        // No, we haven't - store it so we avoid re-doing
        // the following work again in the future...
        visited.insert(board);

        // Store the move used to get this board, so we can backtrack later
        BoardAndLevel key(board, oldLevel);
        previousMoves.insert(pair<BoardAndLevel, Move>(key, move));

        // Check if this board state is a winning state:
#ifdef NONOPTIMAL_CODE
        auto it = find_if(board.begin(), board.end(),
            [](TileKind& x) { return x == Full; });

        if (it == board.end()) {
#else
        static bitset<SIZE*SIZE> empty;
        if (board == empty) {
#endif
            // Yes - we did it!
            cout << "\n\nSolved at depth " << level << "!\n";

            // To print the Moves we used in normal order, we will
            // backtrack through the board states to print
            // the Move we used at each one...
            typedef tuple<Board, Move> BreadCrumb;
            list<BreadCrumb> solution;
            auto itMove = previousMoves.find(BoardAndLevel(board, level));
            while (itMove != previousMoves.end()) {
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

                if (movesSoFar.moveAlreadyPlayed(i,j))
                    continue;

                for (auto& step: g_offsetsList) {
                    int yy = i + step.first;
                    int xx = j + step.second;
                    if (yy>=0 && yy<SIZE && xx>=0 && xx<SIZE) {
                        if (board.get(yy, xx)) {
                            Board newBoard = board;
                            playMove(newBoard, i, j);

                            if (visited.find(newBoard) == visited.end()) {
                                /* Add to the end of the queue for further
                                 * investigation */
                                auto newMovesSoFar = movesSoFar;
                                newMovesSoFar.addMove(i, j);
                                queue.push_back(
                                    State(  level+1,
                                            Move(i, j),
                                            newBoard,
                                            newMovesSoFar));
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
#ifdef NONOPTIMAL_CODE
    TileKind onValue = Full;
#else
    bool onValue = true;
#endif
    board.get(2,0) = onValue;
    board.get(3,0) = onValue;
    board.get(3,1) = onValue;
    board.get(4,1) = onValue;
    board.get(4,3) = onValue;
    board.get(4,4) = onValue;

    SolveBoard(board);
}
