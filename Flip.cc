//////////////////////////////
//
//      Configuration
//
//////////////////////////////

// I found out via the OCaml version that the puzzle space is big, and memory
// usage becomes an issue.  Defining NONOPTIMAL_CODE shows the issues that
// I faced in my original port to C++ - which more or less mirrored 
// the OCaml version of the code.
//
//#define NONOPTIMAL_CODE

#include <assert.h>

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>

#ifdef NONOPTIMAL_CODE
#include <set>
#else
#include <unordered_set>
#endif

#include <list>
#include <tuple>
#include <bitset>

using namespace std;

//////////////////////////////////
//
//      Constants,macros,offsets
//
//////////////////////////////////

// The board is SIZE x SIZE tiles
const int SIZE = 5U;

// Related helper macro to pass (y,x) in one step
#define OFS(y,x) (unsigned(y)*SIZE + unsigned(x))

// The offsets of tiles to toggle when clicking on a tile:
const pair<int,int> g_offsetsList[] = {
    {0,0}, {0,1}, {0,-1}, {1,0}, {-1,0}
};

//////////////////////////////
//
//      Types used
//
//////////////////////////////

// We will need to store the moves done so far, to avoid redoing them,
// Since memory is at a premium, we use a bitset to check for this
struct ListOfMoves : bitset<SIZE*SIZE> {
    ListOfMoves(unsigned long v=0):bitset<SIZE*SIZE>(v) {}

    void addMove(unsigned ofs)           { set(ofs); }
    bool moveAlreadyPlayed(unsigned ofs) { return test(ofs); }
};

#ifdef NONOPTIMAL_CODE
// In the unoptimized version, the board was represented as a vector
// of SIZE*SIZE TileKind instances:
enum TileKind {
    Empty = 0,
    Full  = 1
};

struct Board: public vector<TileKind> {
    Board():vector<TileKind>(SIZE*SIZE) {}
    inline bool test(unsigned ofs) const { return (*this)[ofs] == Full; }
    inline void set(unsigned ofs)        { (*this)[ofs] = Full; }
    inline void flip(unsigned ofs)       { (*this)[ofs] = test(ofs)?Empty:Full; }
};

// ...and each move was stored as the y,x coordinates of the tile toggled:
struct Move {
    enum { Sentinel=0xf };
    int _y, _x;
    inline Move(int yy, int xx):_y(yy), _x(xx) {}
    inline int y() const { return _y; }
    inline int x() const { return _x; }
};

// The state to store for each move in the Breadth-First-Search:

// - The depth in the move tree that we are currently in
// - The Move we performed to reach the current state
// - The Board's current state
// - The moves we've played to get here (bitset)
typedef tuple<unsigned char, Move, Board, ListOfMoves> State;

// Modify a board, by playing a move:
void playMove(Board& board, int y, int x)
{
    for (auto& step: g_offsetsList) {
        int yy = y + step.first;
        int xx = x + step.second;
        if (yy>=0 && yy<SIZE && xx>=0 && xx<SIZE) {
            board.flip(OFS(yy,xx));
        }
    }
}

#else

// In the optimized version, bitsets are used to represent boards. Basically,
// this means that an unsigned long stores the two possible tile states
// (Full/Empty) of each tile, storing each tile in one bit.
//
// The difference in memory usage is an order of magnitude...

struct Board: public bitset<SIZE*SIZE> {
    Board(unsigned long v=0):bitset<SIZE*SIZE>(v) {}

    // The Board will be used as a key in a set (visited board states)
    // In contrast to the vector used in the non-optimized version,
    // the bitset does not provide a default comparison operator,
    // so we had to provide our own:
    inline bool operator<(const Board& rhs) const {
        // Instead of comparing the bits one by one...
        //
        //   size_t i = SIZE*SIZE-1;
        //   while (i > 0) {
        //       if ((*this)[i-1] == rhs[i-1])
        //           i--;
        //       else
        //          return (*this)[i-1] < rhs[i-1];
        //   }
        //   return false;
        //
        // ... we can compare the contained unsigned long ;
        // this provided a 2x speedup!
        return to_ulong() < rhs.to_ulong();
    };
};

struct BoardHashFunction {
    size_t operator ()(const Board &board) const {
        return (size_t) board.to_ulong();
    }
};

// For this optimization via to_ulong() to work, the board size must fit
// within an unsigned long - verify this at compile-time:
static_assert(
    sizeof(Board)<=sizeof(unsigned long),
    "Uncomment the full comparison in Board::operator<");

// Squeeze some more memory out of the Move, by storing both
// x and y in a single unsigned char's 8 bits (4 for x, 4 for y)
struct Move {
    enum { Sentinel=0xf };
    unsigned char _yx;
    inline Move(int yy, int xx):_yx((unsigned char)((yy<<4)|xx)) {}
    inline Move(unsigned char yx):_yx(yx)       {}
    inline int y() const                        { return _yx>>4;  }
    inline int x() const                        { return _yx&0xf; }
};
// Verify the moves fit in 4 bits at compile time
static_assert(
    SIZE<Move::Sentinel,
    "Moves (x,y coordinates) are stored in 4 bits...");

// Finally, for the State placed in the Breadth-First-Search Queue, 
// I optimized memory use - packing all info in 2 unsigned longs:
struct State {
    unsigned long _u1, _u2;
    State(
        unsigned char level,
        const Move& move,
        const Board& board,
        const ListOfMoves& listOfMoves)
    {
        // First unsigned long's 32 bits:
        //
        //       <--the level->  <-- The 25 bits of the board -->
        // 31 30 29 28 27 26 25  24 23 22 21 20 19 18 17 16 ... 0
        // \___/
        //   |___ The two upper bits of move._yx
        //
        _u1 = (unsigned(move._yx) & 0xC0) << 24;
        _u1|= (unsigned(level&0x1f) << 25)       | board.to_ulong();

        // Second unsigned long's 32 bits:
        //
        //    <-- 6 low bits of move._yx -> <-- The 25 bits of listOfMoves -->
        // 31 30 29 ...               26 25 24 23 22 21 20 19 18 17 16 15... 0
        //
        _u2 = (unsigned(move._yx&0x3f) << 25)    | listOfMoves.to_ulong();
        //
        // Oh look - we have one bit to spare :-)
    }

    unsigned char getLevel()     { return (unsigned char)((_u1>>25)&0x1F); }
    Move getMove()               { return Move((unsigned char)(_u2>>25|((_u1&0xC0000000)>>24))); }

    Board getBoard()             { return Board      (_u1 & 0x1FFFFFF); }
    ListOfMoves getListOfMoves() { return ListOfMoves(_u2 & 0x1FFFFFF); }
};

// I generated a move table with this little Python segment:
// 
// g_moveOffsets = [(0,0), (0,1), (0,-1), (1,0), (-1,0)]
// 
// def flip(y,x,bitOffsets):
//     if y>=0 and y<=4 and x>=0 and x<=4:
//         bitOffsets[y*5+x] = 1
// 
// def binary(bitOffsets):
//     return ''.join(reversed(list('1' if ofs in bitOffsets else '0' for ofs in xrange(0,25))))
// 
// for y in xrange(0, 5):
//     for x in xrange(0, 5):
//         bitOffsets = {}
//         for ystep, xstep in g_moveOffsets:
//             flip(y+ystep, x+xstep, bitOffsets)
//         print "    // %d,%d -> %02d ->" % (y, x, y*5+x),
//         print binary(bitOffsets)
//         print "    %d," % int(binary(bitOffsets), 2)

unsigned long moveLookup[] = {
    // 0,0 -> 00 -> 0000000000000000000100011
    35,
    // 0,1 -> 01 -> 0000000000000000001000111
    71,
    // 0,2 -> 02 -> 0000000000000000010001110
    142,
    // 0,3 -> 03 -> 0000000000000000100011100
    284,
    // 0,4 -> 04 -> 0000000000000001000011000
    536,
    // 1,0 -> 05 -> 0000000000000010001100001
    1121,
    // 1,1 -> 06 -> 0000000000000100011100010
    2274,
    // 1,2 -> 07 -> 0000000000001000111000100
    4548,
    // 1,3 -> 08 -> 0000000000010001110001000
    9096,
    // 1,4 -> 09 -> 0000000000100001100010000
    17168,
    // 2,0 -> 10 -> 0000000001000110000100000
    35872,
    // 2,1 -> 11 -> 0000000010001110001000000
    72768,
    // 2,2 -> 12 -> 0000000100011100010000000
    145536,
    // 2,3 -> 13 -> 0000001000111000100000000
    291072,
    // 2,4 -> 14 -> 0000010000110001000000000
    549376,
    // 3,0 -> 15 -> 0000100011000010000000000
    1147904,
    // 3,1 -> 16 -> 0001000111000100000000000
    2328576,
    // 3,2 -> 17 -> 0010001110001000000000000
    4657152,
    // 3,3 -> 18 -> 0100011100010000000000000
    9314304,
    // 3,4 -> 19 -> 1000011000100000000000000
    17580032,
    // 4,0 -> 20 -> 0001100001000000000000000
    3178496,
    // 4,1 -> 21 -> 0011100010000000000000000
    7405568,
    // 4,2 -> 22 -> 0111000100000000000000000
    14811136,
    // 4,3 -> 23 -> 1110001000000000000000000
    29622272,
    // 4,4 -> 24 -> 1100010000000000000000000
    25690112
};

// Modify a board, by playing a move:
void playMove(Board& board, int y, int x)
{
    board = board.to_ulong() ^ moveLookup[y*SIZE+x];
}

#endif

//////////////////////////////
//
//    Functions
//
//////////////////////////////

// Pretty-print a board, highlighting a possible move:
void printBoard(const Board& board, Move move)
{
    cout << "+---------------+\n";
    for(int i=0; i<SIZE; i++) {
        cout << "|";
        for(int j=0; j<SIZE; j++) {
            char c = board.test(OFS(i,j))?'X':' ';
            if (move.y() == i && move.x() == j)
                cout << '[' << c << ']';
            else
                cout << ' ' << c << ' ';
        }
        cout << "|\n";
    }
    cout << "+---------------+\n";
}

// ...and finally, the brains of the operation:
// a Breadth-First-Search of the problem space.
//  ( http://en.wikipedia.org/wiki/Breadth-first_search )

void SolveBoard(Board& initialBoard)
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
    BoardAndLevel key(initialBoard, oldLevel);
    previousMoves.insert(
        pair<BoardAndLevel, Move>(key, Move(Move::Sentinel, Move::Sentinel)));

    // We must not revisit board states we have already examined,
    // so we need a 'visited' set:
#ifdef NONOPTIMAL_CODE
    set<Board> visited;
#else
    unordered_set<Board, BoardHashFunction> visited;
#endif

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
        State(0, Move(Move::Sentinel, Move::Sentinel), initialBoard, ListOfMoves(0)));

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
            cout << "Depth searched: " << setw(2) << int(level)
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
        BoardAndLevel key2(board, oldLevel);
        previousMoves.insert(pair<BoardAndLevel, Move>(key2, move));

        // Check if this board state is a winning state:
#ifdef NONOPTIMAL_CODE
        auto it = find_if(board.begin(), board.end(),
            [](TileKind& x) { return x == Full; });

        if (it == board.end()) {
#else
        if (board.to_ulong() == 0) {
#endif
            // Yes - we did it!
            cout << "\n\nSolved at depth " << int(level) << "!\n";

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

                if (movesSoFar.moveAlreadyPlayed(OFS(i,j)))
                    continue;

                // dont flip this tile unless there is at least one
                // tile set in the neighborhood...
                for (auto& step: g_offsetsList) {
                    int yy = i + step.first;
                    int xx = j + step.second;
                    if (yy>=0 && yy<SIZE && xx>=0 && xx<SIZE) {
                        if (board.test(OFS(yy,xx))) {
                            Board newBoard = board;
                            playMove(newBoard, i, j);

                            if (visited.find(newBoard) == visited.end()) {
                                /* Add to the end of the queue for further
                                 * investigation */
                                auto newMovesSoFar = movesSoFar;
                                newMovesSoFar.addMove(OFS(i,j));
                                queue.push_back(
                                    State(  (unsigned char)(level+1),
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
    //board.set(OFS(0,0));
    //board.set(OFS(0,4));
    //SolveBoard(board);
    //
    //board.set(OFS(0,1)); board.set(OFS(0,4));
    //board.set(OFS(1,3));
    //board.set(OFS(2,0)); board.set(OFS(2,2)); board.set(OFS(2,4)); board.set(OFS(3,0));
    //board.set(OFS(3,2)); board.set(OFS(3,3)); board.set(OFS(3,4));
    //board.set(OFS(4,1)); board.set(OFS(4,3));
    //SolveBoard(board);
    //
    //board.set(OFS(0,1)); board.set(OFS(0,2)); board.set(OFS(0,4));
    //board.set(OFS(1,0)); board.set(OFS(1,4));
    //board.set(OFS(2,0)); board.set(OFS(2,4));
    //board.set(OFS(3,0)); board.set(OFS(3,3)); board.set(OFS(3,4));
    //board.set(OFS(4,0)); board.set(OFS(4,1)); board.set(OFS(4,3)); board.set(OFS(4,4));
    //SolveBoard(board);
    //
    //board.set(OFS(0,0)); board.set(OFS(0,4));
    //board.set(OFS(1,2)); board.set(OFS(1,3)); board.set(OFS(1,4));
    //board.set(OFS(2,2)); board.set(OFS(2,4));
    //board.set(OFS(3,0)); board.set(OFS(3,2)); board.set(OFS(3,3)); board.set(OFS(3,4));
    //board.set(OFS(4,0)); board.set(OFS(4,1)); board.set(OFS(4,2)); board.set(OFS(4,4));
    //SolveBoard(board);
    //
    //board.set(OFS(4,2));
    //board.set(OFS(4,3));
    //board.set(OFS(4,4));
    //SolveBoard(board);
    //
    board.set(OFS(1,0));
    board.set(OFS(2,1));
    board.set(OFS(3,1));
    board.set(OFS(4,1));
    board.set(OFS(4,3));
    board.set(OFS(4,4));
    SolveBoard(board);

    // Depth 11
    //board.set(OFS(0,0));
    //board.set(OFS(0,1));
    //board.set(OFS(0,2));
    //board.set(OFS(0,4));
    //board.set(OFS(1,0));
    //board.set(OFS(2,1));
    //board.set(OFS(2,2));
    //board.set(OFS(2,3));
    //board.set(OFS(3,0));
    //board.set(OFS(3,1));
    //board.set(OFS(3,2));
    //board.set(OFS(4,2));
    //board.set(OFS(4,4));
    //SolveBoard(board);
}
