(* The board is g_boardSize x g_boardSize tiles *)
let g_boardSize = 5

(* Debugging mode *)
let g_debug = ref false

(* Very useful syntactic sugar *)
let ( |> ) x fn = fn x

(* Our board is comprised of these tiles *)
type tileKind = Empty | Full 

(* Toggle a tile *)
let toggleTile = function
    | Empty -> Full
    | Full  -> Empty

(* Each move is represented as the x,y coordinates of the tile toggled *)
type move = { _x: int; _y: int; }

(* And since each board is an array of g_boardSize * g_boardSize ... *)
let get board y x = board.(y*g_boardSize + x)

(* We want to avoid toggling the same tile twice (and thus waste a move) *)
(* We use an integer's 25 bits (g_boardSize * g_boardSize) to store 
 * whether we've played this tile or not in the past *)
type listOfMoves = int 
let addMove l y x = l lor (1 lsl (y*g_boardSize+x))
let moveNotAlreadyPlayed l y x = 
    0 = l land (1 lsl (y*g_boardSize+x))

(* play a move, by toggling the state of the 'cross' of tiles around it *)
let playMove board yy xx =
    let newBoard = Array.copy board in
    let toggleBoard y x =
        if x>=0 && x<g_boardSize && y>=0 && y<g_boardSize then
            newBoard.(y*g_boardSize + x) <- toggleTile (get newBoard y x) in
    toggleBoard yy xx ;
    toggleBoard (1+yy) xx ;
    toggleBoard yy (1+xx) ;
    toggleBoard (yy-1) xx ;
    toggleBoard yy (xx-1) ;
    newBoard

(* this is the board state we want to reach *)
let solvedBoard = Array.make (g_boardSize*g_boardSize) Empty

(* print the board state, indicating a potential move *)
let printBoardMove board move =
    let tileChar = function
    | Empty -> ' '
    | Full  -> 'X' in (
        Printf.printf "+----------------+\n";
        for i=0 to g_boardSize-1 do (
            Printf.printf "|"  ;
            for j=0 to g_boardSize-1 do
                if i == move._y && j == move._x then
                    Printf.printf "[%c]" (tileChar (get board i j))
                else
                    Printf.printf " %c " (tileChar (get board i j))
            done ;
            Printf.printf " |\n" 
        ) done ;
        Printf.printf "+----------------+\n";
    ) ;
    ()
        
(* When we find the solution, we also need to backtrack
 * to display the moves we used to get there...  *)
module H = Hashtbl.Make(
    struct
    type t = tileKind array
    let equal = (=)
    let hash = Hashtbl.hash_param 25 25
    end)

(* The brains of the operation - basically a Breadth-First-Search
   of the problem space: http://en.wikipedia.org/wiki/Breadth-first_search *)
let solveBoard initialBoard =
    Printf.printf "\nSearching for a solution...\n" ;
    (* We need to store the last move that got us to a specific *)
    (*  board state - so we can backtrack from a final board *)
    (*  state to the list of moves we used to achieve it. *)
    let previousMoves = Hashtbl.create 1000000 in
    let dummyMove = { _x = 15; _y = 15; } in
    (*  Start by storing a "sentinel" value, for the initial board *)
    (*  state - we used no Move to achieve it, so store a x,y value *)
    (*  of 15 to mark it: *)
    Hashtbl.add previousMoves (initialBoard, -1) dummyMove;
    (*  We must not revisit board states we have already examined, *)
    (*  so we need a 'visited' set: *)
    let visited = H.create 1000000 in
    (*  Now, to implement Breadth First Search, all we need is a Queue *)
    (*  storing the states we need to investigate - so it needs to *)
    (*  be a Queue of board states... containing a tuple of *)
    (*  depth, move, board, listOfMovesSoFar *)
    let queue = Queue.create () in
    (*  Jumpstart the Q with initial board state and a dummy move *)
    Queue.add (0, dummyMove, initialBoard, 0) queue;
    let currentLevel = ref 0 in
    while not (Queue.is_empty queue) do (
        (*  Extract first element of the queue *)
        let level, move, board, movesSoFar = Queue.take queue in
        if !g_debug then (
            print_endline "Got from Q:";
            printBoardMove board dummyMove
        ) ;
        if level > !currentLevel then (
            currentLevel := level ;
            if not !g_debug then (
                (* Printf.printf "\b\b\b%3d%!" level; *)
                Printf.printf "Depth reached: %2d, in Q: %d" level (Queue.length queue) ;
                print_endline ""
            )
        );
        (*  Have we seen this board before? *)
        if not (H.mem visited board) then (
            (*  No, we haven't - store it so we avoid re-doing *)
            (*  the following work again in the future... *)
            H.replace visited board 1;
            if !g_debug then (
                print_endline "\nVisited cache adding this:" ;
                printBoardMove board dummyMove;
                Printf.printf "Verified: %B\n" (H.mem visited board)
            ) ;

            (* Store board,level => move - so we can backtrack *)
            Hashtbl.replace previousMoves (board, level) move;
            (*  Check if this board state is a winning state: *)
            if board = solvedBoard then (
                (*  Yes, we did it! *)
                print_endline "\n\nSolved!";
                (*  To print the Moves we used in normal order, we will *)
                (*  backtrack through the board states to store in a Stack *)
                (*  the Move we used at each step... *)
                let solution = Stack.create () in
                let currentBoard = ref (Array.copy board) in
                let currentLevel = ref level in
                let foundSentinel = ref false in
                while not !foundSentinel &&
                        Hashtbl.mem previousMoves
                            (!currentBoard, !currentLevel) do
                    let itMove =
                        Hashtbl.find previousMoves
                            (!currentBoard, !currentLevel) in
                    if !currentBoard = initialBoard then
                        foundSentinel := true
                    else (
                        (*  Add this board to the front of the list... *)
                        currentBoard := playMove !currentBoard itMove._y itMove._x ;
                        Stack.push ((Array.copy !currentBoard), itMove) solution ;
                        currentLevel := !currentLevel - 1;
                    )
                done;
                (*  Now that we have the moves, emit them in order *)
                solution |> Stack.iter (fun (board, move) -> (
                    printBoardMove board move;
                    print_endline "Press ENTER to see next move";
                    let dummy = input_line stdin in
                    print_endline dummy))
                ;
                print_endline "All done! :-)";
                exit 0;
            ) else
                (*  Nope, tiles still left in Full state. *)
                (*  *)
                (*  Add all potential states arrising from immediate *)
                (*  possible moves to the end of the queue. *)
                for i=0 to g_boardSize-1 do (
                    for j=0 to g_boardSize-1 do (
                        (* if this tile hasn't been played so far, *)
                        (* and it has at least one neighbour that will be
                         * toggled off... *)
                        if ((moveNotAlreadyPlayed movesSoFar i j) &&
                               ((get board i j) = Full ||
                                (i<g_boardSize-1 && (get board (i+1) j) = Full) ||
                                (j<g_boardSize-1 && (get board i (j+1)) = Full) ||
                                (i>0             && (get board (i-1) j) = Full) ||
                                (j>0             && (get board i (j-1)) = Full))) then (
                            let newBoard = playMove board i j in
                            if !g_debug then (
                                print_endline "\nThis potential next move..." ;
                                printBoardMove newBoard dummyMove;
                                Printf.printf "is it in visited? %B\n\n" (H.mem visited newBoard) 
                            ) ;
                            if not (H.mem visited newBoard) then (
                                if !g_debug then
                                    Printf.printf "Adding it to end of Q (%d,%d):\n" i j ;
                                let newMovesSoFar = addMove movesSoFar i j in
                                Queue.add (level+1, {_y=i; _x=j}, newBoard, newMovesSoFar) queue;
                            )
                        )
                    ) done
                ) done
        )
        (*  and go recheck the queue, from the top! *)
    )
    done

let _ =
    let any = List.fold_left (||) false in
    let inArgs args str =
        any(Array.to_list (Array.map (fun x -> (x = str)) args)) in
    g_debug := inArgs Sys.argv "-debug" ;
    let initialBoard = Array.make (g_boardSize*g_boardSize) Empty in (
        initialBoard.(1*g_boardSize + 0) <- Full ;
        initialBoard.(2*g_boardSize + 1) <- Full ;
        initialBoard.(3*g_boardSize + 1) <- Full ;
        initialBoard.(4*g_boardSize + 1) <- Full ;
        initialBoard.(4*g_boardSize + 3) <- Full ;
        initialBoard.(4*g_boardSize + 4) <- Full ;
        solveBoard initialBoard
    )
