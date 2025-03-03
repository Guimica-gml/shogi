#ifndef SHOGI_H_
#define SHOGI_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#define SHOGI_BOARD_DIM 9

#define SHOGI_SV(cstr) ((Shogi_String_View) { .data = (cstr), .size = strlen(cstr) })
#define SHOGI_SV_STATIC(cstr) { .data = (cstr), .size = sizeof(cstr) - 1 }

#define SHOGI_SV_ARG(sv) (int) (sv).size, (sv).data
#define SHOGI_SV_FMT "%.*s"

typedef struct {
    const char *data;
    size_t size;
} Shogi_String_View;

typedef enum {
    SHOGI_BLACK,
    SHOGI_WHITE,
    SHOGI_COLOR_COUNT,
} Shogi_Color;

typedef enum {
    SHOGI_KING = 0,
    SHOGI_ROOK,
    SHOGI_BISHOP,
    SHOGI_GOLD,
    SHOGI_SILVER,
    SHOGI_KNIGHT,
    SHOGI_LANCE,
    SHOGI_PAWN,
    SHOGI_KIND_COUNT,
} Shogi_Kind;

typedef struct {
    Shogi_Color color;
    Shogi_Kind kind;
    bool is_promoted;
} Shogi_Piece;

typedef struct {
    bool contains_piece;
    Shogi_Piece piece;
} Shogi_Cell;

typedef struct {
    Shogi_Cell board[SHOGI_BOARD_DIM][SHOGI_BOARD_DIM];
    int32_t hands[SHOGI_COLOR_COUNT][SHOGI_KIND_COUNT];
    Shogi_Color turn;
} Shogi;

typedef struct {
    bool board[SHOGI_BOARD_DIM][SHOGI_BOARD_DIM];
} Shogi_Mask;

Shogi shogi_from_sfen(const char *sfen_cstr);
Shogi_Kind shogi_kind_from_char(char ch);

bool shogi_move_piece(Shogi *shogi, size_t from_x, size_t from_y, size_t to_x, size_t to_y);
bool shogi_is_move_legal(Shogi *shogi, size_t from_x, size_t from_y, size_t to_x, size_t to_y, bool allow_king_capture);
bool shogi_find_king(Shogi *shogi, Shogi_Color color, size_t *x, size_t *y);
Shogi_Mask shogi_piece_moves_at(Shogi *shogi, size_t x, size_t y, bool allow_king_capture);
Shogi_Mask shogi_color_moves(Shogi *shogi, Shogi_Color color, bool allow_king_capture);

Shogi_Mask shogi_king_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool allow_king_capture);
Shogi_Mask shogi_rook_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture);
Shogi_Mask shogi_bishop_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture);
Shogi_Mask shogi_gold_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool allow_king_capture);
Shogi_Mask shogi_silver_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture);
Shogi_Mask shogi_knight_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture);
Shogi_Mask shogi_lance_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture);
Shogi_Mask shogi_pawn_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture);

Shogi_Mask shogi_drop_piece_locations(Shogi *shogi, Shogi_Color color, Shogi_Kind kind);
bool shogi_drop_piece(Shogi *shogi, Shogi_Color color, Shogi_Kind kind, int32_t x, int32_t y);

void shogi_mask_add(Shogi_Mask *dst, Shogi_Mask src);

void shogi_hand_add(Shogi *shogi, Shogi_Color color, Shogi_Kind kind);
int32_t shogi_hand_piece_count(Shogi *shogi, Shogi_Color color, Shogi_Kind kind);
void shogi_hand_remove(Shogi *shogi, Shogi_Color color, Shogi_Kind kind);

#endif // SHOGI_H_

#ifdef SHOGI_IMPLEMENTATION

Shogi_String_View shogi_sv_chop(Shogi_String_View *sv, char ch) {
    Shogi_String_View subsv = {0};
    subsv.data = sv->data;
    while (sv->size > 0 && sv->data[0] != ch) {
        subsv.size += 1;
        sv->data += 1;
        sv->size -= 1;
    }
    if (sv->size > 0) {
        sv->data += 1;
        sv->size -= 1;
    }
    return subsv;
}

Shogi_Kind shogi_kind_from_char(char ch) {
    switch (ch) {
    case 'k': case 'K': return SHOGI_KING;
    case 'r': case 'R': return SHOGI_ROOK;
    case 'b': case 'B': return SHOGI_BISHOP;
    case 'g': case 'G': return SHOGI_GOLD;
    case 's': case 'S': return SHOGI_SILVER;
    case 'n': case 'N': return SHOGI_KNIGHT;
    case 'l': case 'L': return SHOGI_LANCE;
    case 'p': case 'P': return SHOGI_PAWN;
    default: return -1;
    }
}

int shogi_load_from_sfen(Shogi *shogi, const char *sfen_cstr) {
    Shogi_String_View sfen = SHOGI_SV(sfen_cstr);

    Shogi_String_View piece_placement = shogi_sv_chop(&sfen, ' ');
    Shogi_String_View turn = shogi_sv_chop(&sfen, ' ');
    Shogi_String_View pieces_in_hand = shogi_sv_chop(&sfen, ' ');

    if (piece_placement.size <= 0) {
        return -1;
    }

    size_t x = 0;
    size_t y = 0;
    bool promote_flag = false;
    for (size_t i = 0; i < piece_placement.size; ++i) {
        char ch = piece_placement.data[i];
        if (isdigit(ch)) {
            int n = ch - '0';
            x += n;
            if (x > SHOGI_BOARD_DIM) {
                return -1;
            }
        } else if (ch == '/') {
            if (x != SHOGI_BOARD_DIM) {
                return -1;
            }
            x = 0;
            y += 1;
            if (y > SHOGI_BOARD_DIM) {
                return -1;
            }
        } else if (ch == '+') {
            if (i == piece_placement.size - 1) {
                return -1;
            }
            if (!isalpha(piece_placement.data[i + 1])) {
                return -1;
            }
            promote_flag = true;
        } else {
            Shogi_Color color = (isupper(ch)) ? SHOGI_BLACK : SHOGI_WHITE;
            Shogi_Kind kind = shogi_kind_from_char(ch);
            if (kind < 0) {
                return -1;
            }
            Shogi_Piece piece = { color, kind, promote_flag };
            shogi->board[y][x].contains_piece = true;
            shogi->board[y][x].piece = piece;
            promote_flag = false;
            x += 1;
            if (x > SHOGI_BOARD_DIM) {
                return -1;
            }
        }
    }

    if (turn.size != 1) {
        return -1;
    } else if (turn.data[0] == 'b') {
        shogi->turn = SHOGI_BLACK;
    } else if (turn.data[0] == 'w') {
        shogi->turn = SHOGI_WHITE;
    } else {
        return -1;
    }

    for (size_t i = 0; i < pieces_in_hand.size; ++i) {
        char ch = pieces_in_hand.data[i];
        Shogi_Color color = (isupper(ch)) ? SHOGI_BLACK : SHOGI_WHITE;
        Shogi_Kind kind = shogi_kind_from_char(ch);
        if (kind < 0) {
            return -1;
        }
        shogi_hand_add(shogi, color, kind);
    }

    return 0;
}

bool shogi_move_piece(Shogi *shogi, size_t from_x, size_t from_y, size_t to_x, size_t to_y) {
    if (!shogi_is_move_legal(shogi, from_x, from_y, to_x, to_y, false)) {
        return false;
    }
    if (shogi->board[to_y][to_x].contains_piece) {
        Shogi_Piece piece = shogi->board[to_y][to_x].piece;
        shogi_hand_add(shogi, !piece.color, piece.kind);
    }
    shogi->board[to_y][to_x] = shogi->board[from_y][from_x];
    shogi->board[from_y][from_x].contains_piece = false;
    shogi->turn = !shogi->turn;
    return true;
}

bool shogi_is_move_legal(Shogi *shogi, size_t from_x, size_t from_y, size_t to_x, size_t to_y, bool allow_king_capture) {
    Shogi_Cell cell = shogi->board[from_y][from_x];
    if (!cell.contains_piece) {
        return false;
    }
    Shogi_Piece piece = cell.piece;
    if (piece.color != shogi->turn) {
        return false;
    }
    Shogi_Mask mask = shogi_piece_moves_at(shogi, from_x, from_y, true);
    if (!mask.board[to_y][to_x]) {
        return false;
    }
    if (allow_king_capture) {
        return true;
    }
    Shogi shogi_copy = *shogi;
    shogi_copy.board[to_y][to_x] = shogi_copy.board[from_y][from_x];
    shogi_copy.board[from_y][from_x].contains_piece = false;
    shogi_copy.turn = !shogi_copy.turn;

    size_t king_x, king_y;
    if (!shogi_find_king(&shogi_copy, piece.color, &king_x, &king_y)) {
        fprintf(stderr, "Error: there is a king missing on the board\n");
        exit(1);
    }

    Shogi_Mask opponent_moves = shogi_color_moves(&shogi_copy, !piece.color, true);
    return !opponent_moves.board[king_y][king_x];
}

Shogi_Mask shogi_piece_moves_at(Shogi *shogi, size_t x, size_t y, bool allow_king_capture) {
    if (!shogi->board[y][x].contains_piece) {
        return (Shogi_Mask) {0};
    }
    Shogi_Piece piece = shogi->board[y][x].piece;
    switch (piece.kind) {
    case SHOGI_KING: return shogi_king_moves_at(shogi, x, y, piece.color, allow_king_capture);
    case SHOGI_ROOK: return shogi_rook_moves_at(shogi, x, y, piece.color, piece.is_promoted, allow_king_capture);
    case SHOGI_BISHOP: return shogi_bishop_moves_at(shogi, x, y, piece.color, piece.is_promoted, allow_king_capture);
    case SHOGI_GOLD: return shogi_gold_moves_at(shogi, x, y, piece.color, allow_king_capture);
    case SHOGI_SILVER: return shogi_silver_moves_at(shogi, x, y, piece.color, piece.is_promoted, allow_king_capture);
    case SHOGI_KNIGHT: return shogi_knight_moves_at(shogi, x, y, piece.color, piece.is_promoted, allow_king_capture);
    case SHOGI_LANCE: return shogi_lance_moves_at(shogi, x, y, piece.color, piece.is_promoted, allow_king_capture);
    case SHOGI_PAWN: return shogi_pawn_moves_at(shogi, x, y, piece.color, piece.is_promoted, allow_king_capture);
    default: assert(0 && "unreachable");
    }
}

Shogi_Mask shogi_color_moves(Shogi *shogi, Shogi_Color color, bool allow_king_capture) {
    Shogi_Mask mask = {0};
    for (size_t y = 0; y < SHOGI_BOARD_DIM; ++y) {
        for (size_t x = 0; x < SHOGI_BOARD_DIM; ++x) {
            if (shogi->board[y][x].contains_piece && shogi->board[y][x].piece.color == color) {
                shogi_mask_add(
                    &mask,
                    shogi_piece_moves_at(shogi, x, y, allow_king_capture)
                );
            }
        }
    }
    return mask;
}

bool shogi_find_king(Shogi *shogi, Shogi_Color color, size_t *rx, size_t *ry) {
    for (size_t y = 0; y < SHOGI_BOARD_DIM; ++y) {
        for (size_t x = 0; x < SHOGI_BOARD_DIM; ++x) {
            if (shogi->board[y][x].contains_piece) {
                Shogi_Piece piece = shogi->board[y][x].piece;
                if (piece.kind == SHOGI_KING && piece.color == color) {
                    if (rx != NULL) *rx = x;
                    if (ry != NULL) *ry = y;
                    return true;
                }
            }
        }
    }
    return false;
}

bool shogi_can_piece_occupy(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color) {
    if (x < 0 || y < 0 || x >= SHOGI_BOARD_DIM || y >= SHOGI_BOARD_DIM) {
        return false;
    }
    Shogi_Cell cell = shogi->board[y][x];
    if (!cell.contains_piece) {
        return true;
    }
    Shogi_Piece piece = cell.piece;
    return piece.color != color;
}

Shogi_Mask shogi_walk_piece(Shogi *shogi, int32_t x, int32_t y, int32_t dirx, int32_t diry, Shogi_Color color, bool allow_king_capture) {
    Shogi_Mask mask = {0};
    int32_t mx = x + dirx;
    int32_t my = y + diry;
    while (shogi_can_piece_occupy(shogi, mx, my, color)) {
        if (allow_king_capture || shogi_is_move_legal(shogi, x, y, mx, my, false)) {
            mask.board[my][mx] = true;
        }
        if (shogi->board[my][mx].contains_piece) {
            break;
        }
        mx += dirx;
        my += diry;
    }
    return mask;
}

Shogi_Mask shogi_check_positions(Shogi *shogi, int32_t x, int32_t y, int32_t (*positions)[2], size_t pos_count, Shogi_Color color, bool allow_king_capture) {
    Shogi_Mask mask = {0};
    for (size_t i = 0; i < pos_count; ++i) {
        int32_t dx = positions[i][0];
        int32_t dy = positions[i][1];
        if (shogi_can_piece_occupy(shogi, x + dx, y + dy, color)) {
            if (allow_king_capture || shogi_is_move_legal(shogi, x, y, x + dx, y + dy, false)) {
                mask.board[y + dy][x + dx] = true;
            }
        }
    }
    return mask;
}

Shogi_Mask shogi_king_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool allow_king_capture) {
    Shogi_Mask mask = {0};
    for (int32_t dy = -1; dy < 2; ++dy) {
        for (int32_t dx = -1; dx < 2; ++dx) {
            if ((dx != 0 || dy != 0) && shogi_can_piece_occupy(shogi, x + dx, y + dy, color)) {
                if (allow_king_capture || shogi_is_move_legal(shogi, x, y, x + dx, y + dy, false)) {
                    mask.board[y + dy][x + dx] = true;
                }
            }
        }
    }
    return mask;
}

Shogi_Mask shogi_rook_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture) {
    Shogi_Mask mask = {0};
    shogi_mask_add(&mask, shogi_walk_piece(shogi, x, y, 1, 0, color, allow_king_capture));
    shogi_mask_add(&mask, shogi_walk_piece(shogi, x, y, 0, 1, color, allow_king_capture));
    shogi_mask_add(&mask, shogi_walk_piece(shogi, x, y, -1, 0, color, allow_king_capture));
    shogi_mask_add(&mask, shogi_walk_piece(shogi, x, y, 0, -1, color, allow_king_capture));
    if (is_promoted) {
        shogi_mask_add(&mask, shogi_king_moves_at(shogi, x, y, color, allow_king_capture));
    }
    return mask;
}

Shogi_Mask shogi_bishop_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture) {
    Shogi_Mask mask = {0};
    shogi_mask_add(&mask, shogi_walk_piece(shogi, x, y, 1, 1, color, allow_king_capture));
    shogi_mask_add(&mask, shogi_walk_piece(shogi, x, y, 1, -1, color, allow_king_capture));
    shogi_mask_add(&mask, shogi_walk_piece(shogi, x, y, -1, 1, color, allow_king_capture));
    shogi_mask_add(&mask, shogi_walk_piece(shogi, x, y, -1, -1, color, allow_king_capture));
    if (is_promoted) {
        shogi_mask_add(&mask, shogi_king_moves_at(shogi, x, y, color, allow_king_capture));
    }
    return mask;
}

Shogi_Mask shogi_gold_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool allow_king_capture) {
    int32_t dir = (color == SHOGI_BLACK) ? -1 : 1;
    int table[6][2] = {
        {  0,  1 * dir },
        {  1,  1 * dir },
        { -1,  1 * dir },
        {  0, -1 * dir },
        {  1,  0 },
        { -1,  0 },
    };
    return shogi_check_positions(shogi, x, y, table, 6, color, allow_king_capture);
}

Shogi_Mask shogi_silver_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture) {
    if (is_promoted) {
        return shogi_gold_moves_at(shogi, x, y, color, allow_king_capture);
    }
    int32_t dir = (color == SHOGI_BLACK) ? -1 : 1;
    int table[5][2] = {
        {  0,  1 * dir },
        {  1,  1 * dir },
        { -1,  1 * dir },
        { -1, -1 * dir },
        {  1, -1 * dir },
    };
    return shogi_check_positions(shogi, x, y, table, 5, color, allow_king_capture);
}

Shogi_Mask shogi_knight_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture) {
    if (is_promoted) {
        return shogi_gold_moves_at(shogi, x, y, color, allow_king_capture);
    }
    int32_t dir = (color == SHOGI_BLACK) ? -1 : 1;
    int table[2][2] = {
        {  1, 2 * dir },
        { -1, 2 * dir },
    };
    return shogi_check_positions(shogi, x, y, table, 2, color, allow_king_capture);
}

Shogi_Mask shogi_lance_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture) {
    if (is_promoted) {
        return shogi_gold_moves_at(shogi, x, y, color, allow_king_capture);
    }
    int32_t dir = (color == SHOGI_BLACK) ? -1 : 1;
    return shogi_walk_piece(shogi, x, y, 0, dir, color, allow_king_capture);
}

Shogi_Mask shogi_pawn_moves_at(Shogi *shogi, int32_t x, int32_t y, Shogi_Color color, bool is_promoted, bool allow_king_capture) {
    if (is_promoted) {
        return shogi_gold_moves_at(shogi, x, y, color, allow_king_capture);
    }
    Shogi_Mask mask = {0};
    int32_t dir = (color == SHOGI_BLACK) ? -1 : 1;
    if (shogi_can_piece_occupy(shogi, x, y + dir, color)) {
        if (allow_king_capture || shogi_is_move_legal(shogi, x, y, x, y + dir, false)) {
            mask.board[y + dir][x] = true;
        }
    }
    return mask;
}

Shogi_Mask shogi_drop_piece_locations(Shogi *shogi, Shogi_Color color, Shogi_Kind kind) {
    Shogi_Mask mask = {0};
    for (size_t y = 0; y < SHOGI_BOARD_DIM; ++y) {
        for (size_t x = 0; x < SHOGI_BOARD_DIM; ++x) {
            Shogi_Cell cell = shogi->board[y][x];
            mask.board[y][x] = !cell.contains_piece;
        }
    }

    size_t row = (color == SHOGI_WHITE) ? SHOGI_BOARD_DIM - 1 : 0;
    if (kind == SHOGI_PAWN) {
        memset(&mask.board[row], 0, SHOGI_BOARD_DIM);
        for (size_t x = 0; x < SHOGI_BOARD_DIM; ++x) {
            bool contains_pawn = false;
            for (size_t y = 0; y < SHOGI_BOARD_DIM; ++y) {
                if (shogi->board[y][x].contains_piece) {
                    Shogi_Piece piece = shogi->board[y][x].piece;
                    if (piece.kind == SHOGI_PAWN && piece.color == color) {
                        contains_pawn = true;
                        break;
                    }
                }
            }
            if (contains_pawn) {
                for (size_t y = 0; y < SHOGI_BOARD_DIM; ++y) {
                    mask.board[y][x] = false;
                }
            }
        }
    } else if (kind == SHOGI_LANCE) {
        memset(&mask.board[row], 0, SHOGI_BOARD_DIM);
    } else if (kind == SHOGI_KNIGHT) {
        size_t dir = (color == SHOGI_WHITE) ? -1 : 1;
        memset(&mask.board[row], 0, SHOGI_BOARD_DIM);
        memset(&mask.board[row + dir], 0, SHOGI_BOARD_DIM);
    }

    return mask;
}

bool shogi_drop_piece(Shogi *shogi, Shogi_Color color, Shogi_Kind kind, int32_t x, int32_t y) {
    if (shogi->turn != color) {
        return false;
    }

    if (shogi->hands[color][kind] <= 0) {
        return false;
    }

    Shogi_Mask drop_positions = shogi_drop_piece_locations(shogi, color, kind);
    if (!drop_positions.board[y][x]) {
        return false;
    }

    shogi->hands[color][kind] -= 1;
    shogi->board[y][x].contains_piece = true;
    shogi->board[y][x].piece = (Shogi_Piece) { color, kind, false };
    shogi->turn = !shogi->turn;
    return true;
}

void shogi_mask_add(Shogi_Mask *dst, Shogi_Mask src) {
    for (size_t y = 0; y < SHOGI_BOARD_DIM; ++y) {
        for (size_t x = 0; x < SHOGI_BOARD_DIM; ++x) {
            dst->board[y][x] = dst->board[y][x] || src.board[y][x];
        }
    }
}

void shogi_hand_add(Shogi *shogi, Shogi_Color color, Shogi_Kind kind) {
    shogi->hands[color][kind] += 1;
}

int32_t shogi_hand_piece_count(Shogi *shogi, Shogi_Color color, Shogi_Kind kind) {
    return shogi->hands[color][kind];
}

void shogi_hand_remove(Shogi *shogi, Shogi_Color color, Shogi_Kind kind) {
    assert(shogi->hands[color][kind] > 0);
    shogi->hands[color][kind] -= 1;
}

#endif // SHOGI_IMPLEMENTATION
