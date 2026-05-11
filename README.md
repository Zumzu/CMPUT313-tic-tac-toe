# Assignment 1

```
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Name : Zane Crawford
# CCID : zcrawfor
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
```

# Protocol Overview  
This protocol is designed to support a concurrent TCP server to host simultaneous games of tic tac toe between human clients. The server manages up to 5 game rooms concurrently, each hosting exactly one game at a time. All communication follows the request response pattern `<OPERATION>;<body>`  
  
The philosophy here is to give the server maximum authority, and limiting the client to essentially just passing user messages to the server, and making server messages human readable. There is no local tic tac toe game running on the client, and therefor *no syncronization*. Signals from the client to the server are simply sent directly, with no local validation. 
  
This means that clients can send arbitrary data, and the server is responsible for any and all input validation. Server owners would be sure that none of the clients are cheating by modifying their client, since by definition they would not be bypassing any validation.  
  
# Connection Model  
Clients form TCP connections to the server on a configurable port. Each client connection is then handled by a dedicated server thread. The server maintains exactly five rooms, each capable of hosting zero, one, or two players. The server is configured to support 20 concurrent players.
  
- A room with zero or one player are waiting to begin a game
- A room with two players actively hosts a game
- Clients cannot be in multiple rooms at the same time  
- Clients cannot leave a room unless by forced disconnect  
- When a game ends, the room is cleared and becomes immediately available for reuse  
  
Client statistics (wins, loss, draw) persist for the duration of the TCP connection and are only transmitted upon explicit `QUIT;` signal.  

# Server Messages (Server -> Client)
All messages expect to be followed by an escape character: `\n`  
### `READY;`  
- Precondition: Client joined lobby
- Indicates the player has just entered the lobby, either by joining for the first time, or after returning to the lobby after a game resolves. After this signal the server expects `JOIN;<room>` or `QUIT;`. Client should prompt the user for input

### `JOINED;<room>,<player>`  
- Precondition: Client sent valid `JOIN;<room>` request for a room that can be joined
- Confirms join, client should display appropriate join message and wait for another signal

### `START;<player>`  
- Precondition: Client is in a room with a player count of 2 that does not yet have a running game
- Indicates a game is starting, if `<player>` is 1, the client should inform the user the game is starting and prompt them for input, if `<player>` is 2, the client should wait for another signal

### `BOARD;<c1>,<c2>, ... , <c9>`
- Precondition: Game is in progress and move has just been made, or a game has ended
- Transmits the complete board state, with each cell ci in `[' ','X','O']` which the client should format and show to the player, the client should then wait for another signal

### `ACT;`
- Precondition: Client is in an active game and its their turn to move
- Indicates the client should make a move, the client should prompt the user for input, server expects `MARK;<cell>`

### `END;<message>`
- Precondition: A game has ended
- Indicates the game is over with some result message, the client should print the message directly to the user and wait for another signal

### `STAT;<win>,<loss>,<draw>`
- Precondition: A user in the lobby has sent `QUIT;`
- Transmits the stats to the user, the client should respond by showing the formatted statistics to the user, then close the connection

### `ERR;<message>`  
- Precondition: None
- Indicates previous client operation was somehow invalid. Receiving client should display `<message>` directly to the user and prompt them for a corrected input
- *See error handling for list*

# Client Messages (Client -> Server)
Client messages generally do not have preconditions, and may be sent at any time a response is expected. The server should be able to parse and respond gracefully to arbitrary client signals. All messages expect to be followed by an escape character: `\n`  
  
### `JOIN;<room>`  
- Valid only if client is not in a room, the room is within [1-5], the room isnt full
- Indicates the player would like to attempt to join room `<room>` if available. The server should attempt to add them to that room if possible, and respond with an appropriate reason if not. 
- If allowed, the server should respond with `JOINED;<room>,<player>`, where player is 1 if the room was previously empty, and 2 if another player was already waiting. If this results in the number of players in that room to become 2, the server should additionally broadcast `START;<player>` to both players to begin the game
  
### `MARK;<cell>`  
- Valid only if client is in an active game, it is their turn, cell is within [1-9], cell isnt occupied
- Indicates the player would like to mark a cell in the tic tac toe game they are a part of. The server should attempt to fill that cell with the correct value 'X' for the player who joined first, 'O' for second, and respond with an appropriate reason if it cannot
- If allowed, and this does not result in an end state, the server should send `ACT;` to the other player (who should be waiting to receive a signal), and switch which symbol will be placed next
- If allowed, and this results in an end state, the server should respond to both players with the following series of messages: `END;<message>` -> `BOARD;<c1>,<c2>, ... , <c9>` -> `READY;`. This conveys the outcome of the game for each player, the final board state, and that the server has returned the clients to the lobby and is ready for more commands

### `QUIT;`  
- Valid only if client is not in a room
- Indicates the player would like gracefully close thier client, and request game statistics, the server should respond with `STAT;<win>,<loss>,<draw>`, which the client should await and handle before closing the connection

# Game State Management  
The server is completely authoritative, and handles game states with the given ttt.c implementation. Each connected client is assigned a player connection tied to its thread, which carry the win/loss/draw data. This connection is then added to and removed from game rooms, which are tracked as a pair of potentially empty player connections, and a tic tac toe board (and some extra validation used together with ttt instance).  
  
The general flow is for a game room is:
1. Player 1 joins the room
2. Player 2 joins the room, a ttt instance is created
3. Player 1 and 2 take turns marking a cell, the ttt instance is updated and checks for an end state each time
4. A player takes an aciton that causes an end state
5. Players are awarded win/loss/draw accordingly
6. Players are removed from the game room, the game room is reset for the next set of players

# Error Handling and Timeouts  
Generally when the server is given a malformed or otherwise invalid input, it will respond with an `ERR;<message>` signal, and expect a corrected input

### Valid error messages:
- `ERR;Room Full` - Attempted to join a room with two players
- `ERR;Invalid Room` - Room number not in [1-5]
- `ERR;Already in a room` - Client sent JOIN while already in a room
- `ERR;Not in active game` - Client sent MARK while not in a game
- `ERR;Not your turn` - Client sent MARK when it was opponents turn
- `ERR;Invalid cell number` - MARK position not in [1-9]
- `ERR;Occupied` - MARK position already occupied
- `ERR;Invalid operation` - Unknown operation
- `ERR;Not allowed` - Operation valid but not allowed in current context

### Disconnect handling  
If a player disconnects while a game isnt running or while not in a room the connection is simply closed. If a player disconnects or times out (after 15 seconds of being inactive while in a game), they will forfeit that game, the remaining player will be sent an appropriate message and awarded a win. The disconnecting players stats will be lost with the connection, and not transmitted.

### Note
This uses the provided `input_parser()` implementation, which seems to send all of stdin in seperate packets when you provide a really long command. This results in the client receiving several smaller commands as chunks of the given input, which causes the undesirable (but not breaking) ouput of many subsequent error messages instead of one. I opted to leave the implementation as is to avoid breaking anything, but in a real context this would be fixed.
