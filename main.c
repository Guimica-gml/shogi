#define SHOGI_IMPLEMENTATION
#include "./shogi.h"

// TODO:
// force promotion when piece has no legal moves
// stop pawn drop check mates

#include <stdio.h>
#include <stdint.h>
#include <raylib.h>
#include <assert.h>

#define BACKGROUND_COLOR BLACK
#define BOARD_COLOR ((Color){0xFC, 0xBC, 0x54, 0xFF})
#define LINE_COLOR ((Color){0x5B, 0x27, 0x0B, 0xFF})
#define SELECTED_PIECE_COLOR ((Color){0x35, 0x4A, 0x21, 0xAA})
#define MOVE_HIGHLIGHT_COLOR ((Color){0x35, 0x4A, 0x21, 0xAA})

#define PIECE_WIDTH 81.0f
#define PIECE_HEIGHT 90.0f

#define HAND_SIZE 0.15f // takes 15% of window (both hands)
#define HAND_X_PAD 6.0f
#define HAND_Y_PAD 0.0f

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

typedef enum {
    STATE_IDLE,
    STATE_SELECT_MOVE,
    STATE_SELECT_DROP,
} UI_State;

typedef struct {
    UI_State state;

    // STATE_SELECT_MOVE
    Vector2 selected_piece;
    Shogi_Mask moves;

    // STATE_SELECT_DROP
    Shogi_Kind drop_kind;
    Shogi_Mask drop_positions;
} UI;

Rectangle get_atlas_texture_rect(Shogi_Piece piece) {
    if (piece.kind == SHOGI_KING) {
        int32_t y = (piece.color == SHOGI_WHITE) ? 2 : 1;
        return (Rectangle) { 0, y * PIECE_HEIGHT, PIECE_WIDTH, PIECE_HEIGHT };
    }

    int32_t x = piece.kind;
    int32_t y = (piece.color == SHOGI_WHITE) ? 2 : 0;
    if (piece.kind != SHOGI_GOLD && piece.is_promoted) {
        y += 1;
    }
    return (Rectangle) { x * PIECE_WIDTH, y * PIECE_HEIGHT, PIECE_WIDTH, PIECE_HEIGHT };
}

void DrawLineThick(
    float x1, float y1, float x2, float y2,
    float thickness, Color color)
{
    Vector2 start_pos = { x1, y1 };
    Vector2 end_pos = { x2, y2 };
    DrawLineEx(start_pos, end_pos, thickness, color);
}

bool point_in_rect(float x, float y, float w, float h, float px, float py) {
    return px > x && py > y && px < x + w && py < y + h;
}

Shogi_Kind DrawHand(
    Shogi *shogi, UI *ui, Texture atlas, Shogi_Color color,
    float x, float y, float board_height)
{
    Shogi_Kind selected_kind = 0;

    assert(PIECE_WIDTH <= 100.0f && PIECE_HEIGHT <= 100.0f);
    float dst_height = board_height * 0.09f;
    float dst_width = dst_height * (PIECE_WIDTH/100.0f);

    float start_x = (color == SHOGI_BLACK) ? x : x - dst_width;
    float start_y = (color == SHOGI_BLACK) ? y - dst_height : y;
    float dir  = (color == SHOGI_BLACK) ? 1 : -1;

    ClearBackground(BACKGROUND_COLOR);
    for (size_t kind = SHOGI_KIND_COUNT - 1; kind > 0; --kind) {
        Shogi_Piece piece = { color, kind, false };
        Rectangle src = get_atlas_texture_rect(piece);

        Rectangle dst;
        dst.x = start_x;
        dst.y = start_y - (dst_height * (SHOGI_KIND_COUNT - kind - 1)) * dir;
        dst.width = dst_width;
        dst.height = dst_height;

        int32_t count = shogi_hand_piece_count(shogi, color, kind);
        if (count > 0) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 mouse = GetMousePosition();
                if (point_in_rect(dst.x, dst.y, dst.width, dst.height, mouse.x, mouse.y)) {
                    selected_kind = kind;
                }
            }

            if (ui->state == STATE_SELECT_DROP &&
                ui->drop_kind == kind && shogi->turn == color)
            {
                DrawRectangle(dst.x, dst.y, dst.width, dst.height, MOVE_HIGHLIGHT_COLOR);
            }

            for (int32_t j = count; j > 0; --j) {
                Rectangle actual_dst = dst;
                actual_dst.x += (dst_width * 0.2f) * (j - 1) * dir;
                DrawTexturePro(atlas, src, actual_dst, (Vector2){0}, 0.0f, WHITE);
            }
        } else {
            Color blend = {0xFF,0xFF,0xFF,0x55};
            DrawTexturePro(atlas, src, dst, (Vector2){0}, 0.0f, blend);
        }
    }

    return selected_kind;
}

void DrawShogi(Shogi *shogi, UI *ui, Texture atlas, float width, float height) {
    float stand_size = min(width, height);
    float horizontal_gap = stand_size * HAND_SIZE;
    float board_size = stand_size - horizontal_gap;
    float board_x = (width - board_size) / 2;
    float board_y = (height - board_size) / 2;
    float cell_size = board_size / SHOGI_BOARD_DIM;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        if (point_in_rect(board_x, board_y, board_size, board_size, mouse.x, mouse.y)) {
            size_t x = (mouse.x - board_x) / cell_size;
            size_t y = (mouse.y - board_y) / cell_size;
            Shogi_Cell cell = shogi->board[y][x];
            if (ui->state == STATE_SELECT_DROP && ui->drop_positions.board[y][x]) {
                bool drop_allowed = shogi_drop_piece(shogi, shogi->turn, ui->drop_kind, x, y);
                assert(drop_allowed);
                ui->state = STATE_IDLE;
            } else if (ui->state == STATE_SELECT_MOVE && ui->moves.board[y][x]) {
                bool move_allowed = shogi_move_piece(
                    shogi, ui->selected_piece.x, ui->selected_piece.y, x, y
                );
                assert(move_allowed);
                ui->state = STATE_IDLE;
            } else if (cell.contains_piece && cell.piece.color == shogi->turn) {
                ui->selected_piece = (Vector2) {x, y};
                ui->moves = shogi_piece_moves_at(shogi, x, y, false);
                ui->state = STATE_SELECT_MOVE;
            } else {
                ui->state = STATE_IDLE;
            }
        } else {
            ui->state = STATE_IDLE;
        }
    }

    {
        float hand_x = board_x + board_size + HAND_X_PAD;
        float hand_y = board_y + board_size + HAND_Y_PAD;
        Shogi_Kind kind = DrawHand(shogi, ui, atlas, SHOGI_BLACK, hand_x, hand_y, board_size);
        if (kind > 0 && shogi->turn == SHOGI_BLACK) {
            ui->drop_kind = kind;
            ui->drop_positions = shogi_drop_piece_locations(shogi, SHOGI_BLACK, kind);
            ui->state = STATE_SELECT_DROP;
        }
    }

    {
        float hand_x = board_x - HAND_X_PAD;
        float hand_y = board_y - HAND_Y_PAD;
        Shogi_Kind kind = DrawHand(shogi, ui, atlas, SHOGI_WHITE, hand_x, hand_y, board_size);
        if (kind > 0 && shogi->turn == SHOGI_WHITE) {
            ui->drop_kind = kind;
            ui->drop_positions = shogi_drop_piece_locations(shogi, SHOGI_WHITE, kind);
            ui->state = STATE_SELECT_DROP;
        }
    }

    DrawRectangle(board_x, board_y, board_size, board_size, BOARD_COLOR);
    if (ui->state == STATE_SELECT_MOVE) {
        DrawRectangle(
            board_x + ui->selected_piece.x * cell_size,
            board_y + ui->selected_piece.y * cell_size,
            cell_size, cell_size, SELECTED_PIECE_COLOR
        );
    }

    for (size_t y = 0; y < SHOGI_BOARD_DIM; ++y) {
        for (size_t x = 0; x < SHOGI_BOARD_DIM; ++x) {
            Shogi_Cell cell = shogi->board[y][x];
            float cx = board_x + (cell_size * x) + (cell_size / 2);
            float cy = board_y + (cell_size * y) + (cell_size / 2);
            if (cell.contains_piece) {
                Rectangle src = get_atlas_texture_rect(cell.piece);

                assert(PIECE_WIDTH <= 100.0f && PIECE_HEIGHT <= 100.0f);
                float dst_width = cell_size * (PIECE_WIDTH/100.0f);
                float dst_height = cell_size * (PIECE_HEIGHT/100.0f);

                Rectangle dst;
                dst.x = cx - dst_width / 2;
                dst.y = cy - dst_height / 2;
                dst.width = dst_width;
                dst.height = dst_height;

                DrawTexturePro(atlas, src, dst, (Vector2){0}, 0.0f, WHITE);
            }

            if ((ui->state == STATE_SELECT_MOVE && ui->moves.board[y][x]) ||
                (ui->state == STATE_SELECT_DROP && ui->drop_positions.board[y][x]))
            {
                DrawCircle(cx, cy, cell_size * 0.2, MOVE_HIGHLIGHT_COLOR);
            }
        }
    }

    float thickness = 3.0f;
    for (size_t i = 1; i < SHOGI_BOARD_DIM; ++i) {
        DrawLineThick(
            board_x, board_y + (cell_size * i),
            board_x + board_size, board_y + (cell_size * i),
            thickness, LINE_COLOR
        );
        DrawLineThick(
            board_x + (cell_size * i), board_y,
            board_x + (cell_size * i), board_y + board_size,
            thickness, LINE_COLOR
        );
    }
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(960, 720, "Shogi");
    SetWindowMinSize(400, 300);
    SetTargetFPS(60);

    const char *sfen = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b LNSGKRBP";

    Texture atlas = LoadTexture("./shogi-pieces.png");
    SetTextureFilter(atlas, TEXTURE_FILTER_BILINEAR);

    UI ui = {0};
    Shogi shogi = {0};
    if (shogi_load_from_sfen(&shogi, sfen) < 0) {
        fprintf(stderr, "Error: incorrect sfen\n");
        exit(1);
    }

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);
        float width = (float) GetScreenWidth();
        float height = (float) GetScreenHeight();
        DrawShogi(&shogi, &ui, atlas, width, height);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
