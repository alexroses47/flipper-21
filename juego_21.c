#include <furi.h>
#include <gui/gui.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "Juego21"

typedef struct {
    const char* title;
    const char* rules1;
    const char* rules2;
    const char* rules3;
    const char* start;
    const char* mode_title;
    const char* mode_1p;
    const char* mode_2p;
    const char* who_first;
    const char* player_first;
    const char* ai_first;
    const char* p1_first;
    const char* p2_first;
    const char* your_turn;
    const char* ai_turn;
    const char* p1_turn;
    const char* p2_turn;
    const char* you_lose;
    const char* you_win;
    const char* p1_lose;
    const char* p2_lose;
    const char* new_game;
    const char* score_label;
    const char* p1_score;
    const char* p2_score;
} Lang;

static const Lang LANG_ES = {
    .title        = "JUEGO DEL 21",
    .rules1       = "Di 1, 2 o 3 numeros.",
    .rules2       = "Quien diga 21 pierde.",
    .rules3       = "Turno alterno.",
    .start        = "OK = Empezar",
    .mode_title   = "Modo de juego",
    .mode_1p      = "  1 Jugador",
    .mode_2p      = "  2 Jugadores",
    .who_first    = "Quien empieza?",
    .player_first = "  Yo empiezo",
    .ai_first     = "  Flipper empieza",
    .p1_first     = "  Jugador 1",
    .p2_first     = "  Jugador 2",
    .your_turn    = "Tu turno",
    .ai_turn      = "Flipper...",
    .p1_turn      = "Turno J1",
    .p2_turn      = "Turno J2",
    .you_lose     = "Dijiste 21! Perdiste!",
    .you_win      = "Flipper dijo 21! Ganas!",
    .p1_lose      = "J1 dijo 21! J2 gana!",
    .p2_lose      = "J2 dijo 21! J1 gana!",
    .new_game     = "OK=nuevo  Back=menu",
    .score_label  = "Tu",
    .p1_score     = "J1",
    .p2_score     = "J2",
};

static const Lang LANG_EN = {
    .title        = "GAME OF 21",
    .rules1       = "Say 1, 2 or 3 numbers.",
    .rules2       = "Who says 21 loses.",
    .rules3       = "Alternate turns.",
    .start        = "OK = Start",
    .mode_title   = "Game mode",
    .mode_1p      = "  1 Player",
    .mode_2p      = "  2 Players",
    .who_first    = "Who goes first?",
    .player_first = "  I go first",
    .ai_first     = "  Flipper goes first",
    .p1_first     = "  Player 1",
    .p2_first     = "  Player 2",
    .your_turn    = "Your turn",
    .ai_turn      = "Flipper...",
    .p1_turn      = "P1 turn",
    .p2_turn      = "P2 turn",
    .you_lose     = "You said 21! You lose!",
    .you_win      = "Flipper said 21! You win!",
    .p1_lose      = "P1 said 21! P2 wins!",
    .p2_lose      = "P2 said 21! P1 wins!",
    .new_game     = "OK=new  Back=menu",
    .score_label  = "You",
    .p1_score     = "P1",
    .p2_score     = "P2",
};

typedef enum {
    ScreenLang,
    ScreenRules,
    ScreenMode,
    ScreenWhoFirst,
    ScreenGame,
    ScreenResult,
} Screen;

typedef enum { TurnPlayer, TurnAI } Turn;

typedef struct {
    Screen      screen;
    Turn        turn;
    int         counter;
    int         menu_sel;
    int         player_wins;
    int         ai_wins;
    bool        player_starts;
    bool        two_player;
    uint32_t    ai_timer;
    bool        running;
    const Lang* L;

    FuriMutex*        mutex;
    ViewPort*         view_port;
    Gui*              gui;
    FuriMessageQueue* queue;
} Game21;

// ─── IA: busca posiciones ganadoras (multiplos de 4) ─────────────────────────
static int ai_choose(int current) {
    for(int n = 1; n <= 3; n++) {
        int r = current + n;
        if(r == 20 || r % 4 == 0) return n;
    }
    // Sin jugada ganadora: elegir al azar entre 1-3 sin pasarse de 21
    int max = 21 - current;
    if(max > 3) max = 3;
    return (rand() % max) + 1;
}

// ─── Barra de números horizontal ─────────────────────────────────────────────
static void draw_number_bar(Canvas* canvas, int current, int sel, bool player_turn) {
    int start = current - 2;
    if(start < 1)  start = 1;
    if(start > 14) start = 14;

    int x      = 4;
    int y_num  = 38;
    int y_line = 42;
    int y_ptr  = 28;

    canvas_draw_line(canvas, 0, y_line, 128, y_line);

    for(int n = start; n <= start + 9 && n <= 21; n++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", n % 100);
        int w = (n >= 10) ? 12 : 8;

        if(n == current) {
            canvas_draw_str(canvas, x, y_ptr, "|");
            canvas_draw_frame(canvas, x - 1, y_num - 8, w + 2, 10);
            canvas_draw_str(canvas, x, y_num, buf);
        } else if(player_turn && n > current && n <= current + sel) {
            canvas_draw_box(canvas, x - 1, y_num - 8, w + 2, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, y_num, buf);
            canvas_set_color(canvas, ColorBlack);
        } else if(n == 21) {
            canvas_draw_box(canvas, x - 1, y_num - 8, w + 2, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, y_num, buf);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, x, y_num, buf);
        }
        x += w + 3;
    }
}

// ─── DRAW: idioma ─────────────────────────────────────────────────────────────
static void draw_lang(Canvas* canvas, Game21* g) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 20, 12, "JUEGO DEL 21");
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 25, 27, "Idioma / Language");

    const char* opts[2] = {"  Espanol", "  English"};
    for(int i = 0; i < 2; i++) {
        int y = 42 + i * 14;
        if(g->menu_sel == i) {
            canvas_draw_box(canvas, 20, y - 9, 88, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 24, y, opts[i]);
        if(g->menu_sel == i) canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 50, 64, "OK");
}

// ─── DRAW: reglas ─────────────────────────────────────────────────────────────
static void draw_rules(Canvas* canvas, Game21* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 20, 10, L->title);
    canvas_draw_line(canvas, 0, 13, 128, 13);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 25, L->rules1);
    canvas_draw_str(canvas, 4, 37, L->rules2);
    canvas_draw_str(canvas, 4, 49, L->rules3);
    canvas_draw_line(canvas, 0, 55, 128, 55);
    canvas_draw_str(canvas, 30, 64, L->start);
}

// ─── DRAW: modo de juego ─────────────────────────────────────────────────────
static void draw_mode(Canvas* canvas, Game21* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 12, L->mode_title);
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    const char* opts[2] = {L->mode_1p, L->mode_2p};
    for(int i = 0; i < 2; i++) {
        int y = 34 + i * 16;
        if(g->menu_sel == i) {
            canvas_draw_box(canvas, 10, y - 9, 108, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 14, y, opts[i]);
        if(g->menu_sel == i) canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 50, 64, "OK");
}

// ─── DRAW: quien empieza ──────────────────────────────────────────────────────
static void draw_who_first(Canvas* canvas, Game21* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 12, L->who_first);
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    const char* opts[2];
    if(g->two_player) {
        opts[0] = L->p1_first;
        opts[1] = L->p2_first;
    } else {
        opts[0] = L->player_first;
        opts[1] = L->ai_first;
    }
    for(int i = 0; i < 2; i++) {
        int y = 34 + i * 16;
        if(g->menu_sel == i) {
            canvas_draw_box(canvas, 10, y - 9, 108, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 14, y, opts[i]);
        if(g->menu_sel == i) canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 50, 64, "OK");
}

// ─── DRAW: juego ──────────────────────────────────────────────────────────────
static void draw_game(Canvas* canvas, Game21* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);

    // Marcador
    char score[32];
    if(g->two_player) {
        snprintf(score, sizeof(score), "%s:%d  %s:%d",
                 L->p1_score, g->player_wins, L->p2_score, g->ai_wins);
    } else {
        snprintf(score, sizeof(score), "%s:%d  Flip:%d",
                 L->score_label, g->player_wins, g->ai_wins);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, score);

    const char* turn_str;
    if(g->two_player) {
        turn_str = (g->turn == TurnPlayer) ? L->p1_turn : L->p2_turn;
    } else {
        turn_str = (g->turn == TurnPlayer) ? L->your_turn : L->ai_turn;
    }
    canvas_draw_str(canvas, 80, 8, turn_str);
    canvas_draw_line(canvas, 0, 10, 128, 10);

    // Barra horizontal de números
    bool pt = (g->turn == TurnPlayer) || g->two_player;
    draw_number_bar(canvas, g->counter, g->menu_sel, pt);

    // Opciones abajo
    canvas_draw_line(canvas, 0, 50, 128, 50);
    canvas_set_font(canvas, FontSecondary);

    if(g->turn == TurnPlayer || g->two_player) {
        for(int i = 1; i <= 3; i++) {
            if(g->counter + i > 21) break;
            char opt[40];
            int a = (g->counter + 1) % 22;
            int b = (g->counter + 2) % 22;
            int c = (g->counter + 3) % 22;
            if(i == 1)
                snprintf(opt, sizeof(opt), "->%d", a);
            else if(i == 2)
                snprintf(opt, sizeof(opt), "->%d-%d", a, b);
            else
                snprintf(opt, sizeof(opt), "->%d-%d-%d", a, b, c);

            // Ancho dinamico segun cantidad de digitos
            int box_w = (i == 1) ? 28 : (i == 2) ? 40 : 52;
            int offsets[3] = {2, 32, 74};
            int ox   = offsets[i - 1];
            bool sel = (g->menu_sel == i);
            if(sel) {
                canvas_draw_box(canvas, ox - 1, 52, box_w, 11);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, ox, 61, opt);
            if(sel) canvas_set_color(canvas, ColorBlack);
        }
    }
}

// ─── DRAW: resultado ──────────────────────────────────────────────────────────
static void draw_result(Canvas* canvas, Game21* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);

    bool player_lost = (g->turn == TurnPlayer);

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, 50, 30, "21");

    canvas_set_font(canvas, FontSecondary);
    const char* msg;
    if(g->two_player) {
        msg = player_lost ? L->p1_lose : L->p2_lose;
    } else {
        msg = player_lost ? L->you_lose : L->you_win;
    }
    canvas_draw_str(canvas, 4, 42, msg);

    char score[32];
    if(g->two_player) {
        snprintf(score, sizeof(score), "%s:%d  %s:%d",
                 L->p1_score, g->player_wins, L->p2_score, g->ai_wins);
    } else {
        snprintf(score, sizeof(score), "%s:%d  Flipper:%d",
                 L->score_label, g->player_wins, g->ai_wins);
    }
    canvas_draw_str(canvas, 4, 54, score);
    canvas_draw_line(canvas, 0, 56, 128, 56);
    canvas_draw_str(canvas, 4, 64, L->new_game);
}

// ─── Draw callback ────────────────────────────────────────────────────────────
static void draw_cb(Canvas* canvas, void* ctx) {
    Game21* g = ctx;
    furi_mutex_acquire(g->mutex, FuriWaitForever);
    switch(g->screen) {
    case ScreenLang:     draw_lang(canvas, g);      break;
    case ScreenRules:    draw_rules(canvas, g);     break;
    case ScreenMode:     draw_mode(canvas, g);      break;
    case ScreenWhoFirst: draw_who_first(canvas, g); break;
    case ScreenGame:     draw_game(canvas, g);      break;
    case ScreenResult:   draw_result(canvas, g);    break;
    }
    furi_mutex_release(g->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    Game21* g = ctx;
    furi_message_queue_put(g->queue, event, 0);
}

static void start_game(Game21* g) {
    g->counter  = 0;
    g->menu_sel = 1;
    g->screen   = ScreenGame;
    g->turn     = g->player_starts ? TurnPlayer : TurnAI;
    if(!g->two_player && g->turn == TurnAI) g->ai_timer = furi_get_tick();
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int32_t juego_21_app(void* p) {
    UNUSED(p);
    srand(furi_get_tick());

    Game21* g = malloc(sizeof(Game21));
    memset(g, 0, sizeof(Game21));
    g->running       = true;
    g->screen        = ScreenLang;
    g->L             = &LANG_ES;
    g->menu_sel      = 0;
    g->player_starts = true;
    g->two_player    = false;

    g->mutex     = furi_mutex_alloc(FuriMutexTypeNormal);
    g->queue     = furi_message_queue_alloc(8, sizeof(InputEvent));
    g->view_port = view_port_alloc();
    view_port_draw_callback_set(g->view_port, draw_cb, g);
    view_port_input_callback_set(g->view_port, input_cb, g);
    g->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(g->gui, g->view_port, GuiLayerFullscreen);

    InputEvent ev;

    while(g->running) {
        if(furi_message_queue_get(g->queue, &ev, 100) == FuriStatusOk) {
            if(ev.type != InputTypeShort) continue;

            furi_mutex_acquire(g->mutex, FuriWaitForever);

            switch(g->screen) {
            case ScreenLang:
                if(ev.key == InputKeyUp   && g->menu_sel > 0) g->menu_sel--;
                if(ev.key == InputKeyDown && g->menu_sel < 1) g->menu_sel++;
                if(ev.key == InputKeyOk) {
                    g->L        = (g->menu_sel == 0) ? &LANG_ES : &LANG_EN;
                    g->screen   = ScreenRules;
                    g->menu_sel = 0;
                }
                if(ev.key == InputKeyBack) g->running = false;
                break;

            case ScreenRules:
                if(ev.key == InputKeyOk) {
                    g->screen   = ScreenMode;
                    g->menu_sel = 0;
                }
                if(ev.key == InputKeyBack) {
                    g->screen   = ScreenLang;
                    g->menu_sel = 0;
                }
                break;

            case ScreenMode:
                if(ev.key == InputKeyUp   && g->menu_sel > 0) g->menu_sel--;
                if(ev.key == InputKeyDown && g->menu_sel < 1) g->menu_sel++;
                if(ev.key == InputKeyOk) {
                    g->two_player = (g->menu_sel == 1);
                    g->screen     = ScreenWhoFirst;
                    g->menu_sel   = 0;
                }
                if(ev.key == InputKeyBack) {
                    g->screen   = ScreenRules;
                    g->menu_sel = 0;
                }
                break;

            case ScreenWhoFirst:
                if(ev.key == InputKeyUp   && g->menu_sel > 0) g->menu_sel--;
                if(ev.key == InputKeyDown && g->menu_sel < 1) g->menu_sel++;
                if(ev.key == InputKeyOk) {
                    g->player_starts = (g->menu_sel == 0);
                    start_game(g);
                }
                if(ev.key == InputKeyBack) {
                    g->screen   = ScreenMode;
                    g->menu_sel = 0;
                }
                break;

            case ScreenGame:
                if(ev.key == InputKeyBack) {
                    g->running = false;
                } else if(g->turn == TurnPlayer || g->two_player) {
                    if(ev.key == InputKeyLeft  && g->menu_sel > 1) g->menu_sel--;
                    if(ev.key == InputKeyRight && g->menu_sel < 3) {
                        if(g->counter + g->menu_sel + 1 <= 21) g->menu_sel++;
                    }
                    if(ev.key == InputKeyOk) {
                        int n = g->menu_sel;
                        if(g->counter + n > 21) n = 21 - g->counter;
                        g->counter += n;
                        if(g->counter >= 21) {
                            // Quien dijo 21 pierde
                            if(g->turn == TurnPlayer)
                                g->ai_wins++;
                            else
                                g->player_wins++;
                            g->counter = 21;
                            g->screen  = ScreenResult;
                        } else {
                            // Cambio de turno
                            g->turn = (g->turn == TurnPlayer) ? TurnAI : TurnPlayer;
                            g->menu_sel = 1;
                            if(!g->two_player && g->turn == TurnAI)
                                g->ai_timer = furi_get_tick();
                        }
                    }
                }
                break;

            case ScreenResult:
                if(ev.key == InputKeyOk) {
                    g->screen   = ScreenWhoFirst;
                    g->menu_sel = 0;
                }
                if(ev.key == InputKeyBack) {
                    g->screen   = ScreenLang;
                    g->menu_sel = 0;
                }
                break;
            }

            furi_mutex_release(g->mutex);
        }

        // IA juega con delay de 1.2s (solo en modo 1 jugador)
        furi_mutex_acquire(g->mutex, FuriWaitForever);
        if(g->screen == ScreenGame &&
           g->turn == TurnAI &&
           !g->two_player &&
           furi_get_tick() - g->ai_timer >= 1200) {

            int n = ai_choose(g->counter);
            if(g->counter + n > 21) n = 21 - g->counter;
            g->counter += n;

            if(g->counter >= 21) {
                g->player_wins++;
                g->counter = 21;
                g->screen  = ScreenResult;
            } else {
                g->turn     = TurnPlayer;
                g->menu_sel = 1;
            }
        }
        furi_mutex_release(g->mutex);

        view_port_update(g->view_port);
    }

    gui_remove_view_port(g->gui, g->view_port);
    view_port_free(g->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(g->queue);
    furi_mutex_free(g->mutex);
    free(g);
    return 0;
}
