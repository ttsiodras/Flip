(* for RGB data of the image *)
open Bigarray

(* The board is g_boardSize x g_boardSize tiles *)
let g_boardSize = 5

(* Debugging mode *)
let g_debug = ref false

(* Limiting the board space: only consider boards
 * with less than this many set tiles
 *)
let g_limit = ref 6

(* Very useful syntactic sugar *)
let ( |> ) x fn = fn x

(* This emulates the [X .. Y] construct of F# *)
let (--) i j =
    let rec aux n acc =
        if n < i then acc else aux (n-1) (n :: acc)
    in aux j []

let arrayDeepCopy matrix = Array.map Array.copy matrix

(* The tile "bodies" information - filled by detectTileBodies below,
   via heuristics on the center pixel of the tiles *)
type tileKind = Empty | Full

type move = { _x: int; _y: int; }

let toggleTile = function
| Empty -> Full
| Full  -> Empty

let get board y x =
    board.(y).(x)

let playMove board yy xx =
    let newBoard = arrayDeepCopy board in
    let toggleBoard y x =
        if x>=0 && x<g_boardSize && y>=0 && y<g_boardSize then
            newBoard.(y).(x) <- toggleTile newBoard.(y).(x) in
    toggleBoard yy xx ;
    toggleBoard (1+yy) xx ;
    toggleBoard yy (1+xx) ;
    toggleBoard (yy-1) xx ;
    toggleBoard yy (xx-1) ;
    newBoard

let totalSet board =
    let count = ref 0 in
    for i=0 to g_boardSize-1 do
        for j=0 to g_boardSize-1 do 
            if board.(i).(j) = Full then
                count := !count + 1
        done
    done ;
    !count

let solvedBoard = Array.make_matrix g_boardSize g_boardSize Empty

let printBoard tileArray =
    let tileChar = function
    | Empty -> ' '
    | Full  -> 'X' in (
        Printf.printf "+-----------+\n";
        for i=0 to g_boardSize-1 do (
            Printf.printf "|"  ;
            for j=0 to g_boardSize-1 do
                Printf.printf " %c" (tileChar tileArray.(i).(j))
            done ;
            Printf.printf " |\n" 
        ) done ;
        Printf.printf "+-----------+\n";
    ) ;
    ()

let printBoardMove tileArray move =
    let tileChar = function
    | Empty -> ' '
    | Full  -> 'X' in (
        Printf.printf "+----------------+\n";
        for i=0 to g_boardSize-1 do (
            Printf.printf "|"  ;
            for j=0 to g_boardSize-1 do
                if i == move._y && j == move._x then
                    Printf.printf "[%c]" (tileChar (get tileArray i j))
                else
                    Printf.printf " %c " (tileChar (get tileArray i j))
            done ;
            Printf.printf " |\n" 
        ) done ;
        Printf.printf "+----------------+\n";
    ) ;
    ()
        
(* When we find the solution, we also need to backtrack
 * to display the moves we used to get there...
 *)
module H = Hashtbl.Make(
    struct
    type t = tileKind array array
    let equal = (=)
    let hash = Hashtbl.hash_param 3000 3000
    end)

(* The brains of the operation - basically a Breadth-First-Search
   of the problem space:
       http://en.wikipedia.org/wiki/Breadth-first_search *)
let solveBoard tileArray =
    Printf.printf "\nSearching for a solution with maximum set tiles: %d\n" !g_limit;
    (* We need to store the last move that got us to a specific *)
    (*  board state - that way we can backtrack from a final board *)
    (*  state to the list of moves we used to achieve it. *)

    let previousMoves = Hashtbl.create 1000000 in
    let dummyMove = { _x = -1; _y = -1; } in
    (*  Start by storing a "sentinel" value, for the initial board *)
    (*  state - we used no Move to achieve it, so store a block id *)
    (*  of -1 to mark it: *)
    Hashtbl.add previousMoves (tileArray, -1) dummyMove;
    (*  We must not revisit board states we have already examined, *)
    (*  so we need a 'visited' set: *)
    let visited = H.create 100000 in
    (*  Now, to implement Breadth First Search, all we need is a Queue *)
    (*  storing the states we need to investigate - so it needs to *)
    (*  be a list of board states... i.e. a list of tile Arrays! *)
    let queue = Queue.create () in
    (*  Jumpstart the Q with initial board state and a dummy move *)
    Queue.add (1, dummyMove, tileArray) queue;
    let currentLevel = ref 0 in
    while not (Queue.is_empty queue) do
        (*  Extract first element of the queue *)
        let level, move, board = Queue.take queue in
        if !g_debug then (
            print_endline "Got from Q:";
            printBoard board
        ) ;
        if level > !currentLevel then (
            currentLevel := level ;
            if not !g_debug then (
                (* Printf.printf "\b\b\b%3d%!" level; *)
                Printf.printf "Depth reached: %d, in Q: %d" level (Queue.length queue) ;
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
                printBoard board ;
                Printf.printf "Verified: %B\n" (H.mem visited board)
            ) ;

            (* Store board,level,move - so we can backtrack *)
            Hashtbl.replace previousMoves (board, level) move;
            (*  Check if this board state is a winning state: *)
            if board = solvedBoard then (
                (*  Yes, we did it! *)
                print_endline "\n\nSolved!";
                (*  To print the Moves we used in normal order, we will *)
                (*  backtrack through the board states to store in a Stack *)
                (*  the Move we used at each step... *)
                let solution = Stack.create () in
                let currentBoard = ref (arrayDeepCopy board) in
                let currentLevel = ref level in
                let foundSentinel = ref false in
                while not !foundSentinel &&
                        Hashtbl.mem previousMoves
                            (!currentBoard, !currentLevel) do
                    let itMove =
                        Hashtbl.find previousMoves
                            (!currentBoard, !currentLevel) in
                    if itMove._x = -1 then
                        (*  Sentinel - reached starting board *)
                        foundSentinel := true
                    else (
                        (*  Add this board to the front of the list... *)
                        currentBoard := playMove !currentBoard itMove._y itMove._x ;
                        Stack.push (arrayDeepCopy !currentBoard, itMove) solution ;
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
            ) else (
                (*  Nope, tiles still left. *)
                (*  *)
                (*  Add all potential states arrising from immediate *)
                (*  possible moves to the end of the queue. *)
                for i=0 to g_boardSize-1 do (
                    for j=0 to g_boardSize-1 do (
                        if board.(i).(j) = Full ||
                                (i<g_boardSize-1 && board.(i+1).(j) = Full) ||
                                (j<g_boardSize-1 && board.(i).(j+1) = Full) ||
                                (i>0 && board.(i-1).(j) = Full) ||
                                (j>0 && board.(i).(j-1) = Full) then (
                            let newBoard = playMove board i j in
                            if totalSet newBoard < !g_limit then (
                                if !g_debug then (
                                    print_endline "\nThis next move..." ;
                                    printBoard newBoard ;
                                    Printf.printf "is in visited: %B\n\n" (H.mem visited newBoard) 
                                ) ;
                                if not (H.mem visited newBoard) then (
                                    if !g_debug then
                                        Printf.printf "Adding this to end of Q (%d,%d):\n" i j ;
                                    Queue.add (level+1, {_y=i; _x=j}, newBoard) queue;
                                
                                ) 
                            )
                        )
                    ) done
                    ;
                ) done
            )
        )
        (*  and go recheck the queue, from the top! *)
    done

let _ =
    (* let any = List.fold_left (||) false
     * ..is slower than ... *)
    let rec any l =
        match l with
        | []        -> false
        | true::xs  -> true
        | false::xs -> any xs in
    let inArgs args str =
        any(Array.to_list (Array.map (fun x -> (x = str)) args)) in
    g_debug := inArgs Sys.argv "-debug" ;
    let initialBoard = Array.make_matrix g_boardSize g_boardSize Empty in (
        initialBoard.(2).(3) <- Full ;
        initialBoard.(2).(4) <- Full ;
        initialBoard.(3).(0) <- Full ;
        for i=6 to 25 do 
            g_limit := i ;
            solveBoard initialBoard
        done
    )
