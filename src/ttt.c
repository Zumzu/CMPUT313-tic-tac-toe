#include <stdio.h>
#include <stdlib.h>
#include "../include/ttt.h"

static void convertMove(int move, int* row, int* col) {
    *row = (move - 1) / 3;
    *col = (move - 1) % 3;
}

TicTacToe* ttt_new() {
    TicTacToe* t = malloc(sizeof(TicTacToe));

    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 3; ii++) {
            t->board[i][ii] = ' ';
        }
    }

    t->currentPlayer = 'X';
    t->turnCount = 0;

    return t;
}

void ttt_destroy(TicTacToe* t) {
    free(t);
}

void ttt_reset(TicTacToe** t) {
    TicTacToe* t2 = ttt_new();
    ttt_destroy(*t);
    *t = t2;
}

void ttt_drawBoard(TicTacToe* t) {
    printf("-------------\n");

    for (int i = 0; i < 3; i++) {
        printf("| ");
        for (int ii = 0; ii < 3; ii++) {
            char c = t->board[i][ii];

            if (c == ' ') {
                c = '0' + (i * 3 + ii + 1);
            }
            printf("%c | ", c);
        }
        printf("\n-------------\n");
    }
}

bool ttt_makeMove(TicTacToe* t, int move) {
    int row, col;

    if (move < 1 || move > 9) {
        return false;
    }

    convertMove(move, &row, &col);

    if (t->board[row][col] != ' ') {
        return false;
    }

    t->board[row][col] = t->currentPlayer;
    t->turnCount += 1;

    return true;
}

bool ttt_hasWinner(TicTacToe* t) {
    return ttt_checkWin(t, t->currentPlayer);
}

bool ttt_isDraw(TicTacToe* t) {
    return t->turnCount == 9 && !ttt_hasWinner(t);
}

void ttt_switchPlayer(TicTacToe* t) {
    t->currentPlayer = (t->currentPlayer == 'X') ? 'O' : 'X';
}

char ttt_getCurrentPlayer(TicTacToe* t) {
    return t->currentPlayer;
}

char ttt_getBoardCell(TicTacToe* t, int move) {
    int row, col;
    convertMove(move, &row, &col);
    return t->board[row][col];
}

bool ttt_checkWin(TicTacToe* t, char player) {
    // Check rows and columns
    for (int i = 0; i < 3; i++) {
        bool is_win_row = true;
        bool is_win_col = true;

        for (int ii = 0; ii < 3; ii++) {
            is_win_row = is_win_row && t->board[i][ii] == player;
            is_win_col = is_win_col && t->board[ii][i] == player;
        }

        if (is_win_row || is_win_col) {
            return true;
        }
    }

    // Check diagonals
    bool is_win_fwd = true;
    bool is_win_bwd = true;

    for (int i = 0; i < 3; i++) {
        is_win_fwd = is_win_fwd && t->board[i][i] == player;
        is_win_bwd = is_win_bwd && t->board[2-i][i] == player;
    }

    return is_win_fwd || is_win_bwd;
}
