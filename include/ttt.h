#pragma once
#include <stdbool.h>

typedef struct TicTacToe {
    char board[3][3];
    char currentPlayer;
    int turnCount;
} TicTacToe;

TicTacToe* ttt_new();
void ttt_destroy(TicTacToe* t);
void ttt_reset(TicTacToe** t);
void ttt_drawBoard(TicTacToe* t);
bool ttt_makeMove(TicTacToe* t, int move);
bool ttt_hasWinner(TicTacToe* t);
bool ttt_isDraw(TicTacToe* t);
void ttt_switchPlayer(TicTacToe* t);
char ttt_getCurrentPlayer(TicTacToe* t);
char ttt_getBoardCell(TicTacToe* t, int move);
bool ttt_checkWin(TicTacToe* t, char player);
