#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define FRAME_DELAY 10

#define MIN_WIN_ROW 30
#define MIN_WIN_COL 70

#define JUMP_HEIGHT 10
#define JUMP_AIRTIME 8
#define JUMP_WIDTH 2 * JUMP_HEIGHT + JUMP_AIRTIME

#define OBSTACLE_LENGTH 6
#define OBSTACLE_WIDTH 11
#define OBSTACLE_RADIUS 5

#define GAME_LENGTH 4             // see declaration of obs_max_gen_gap
#define GAME_MAX_DIFF_SCORE 4000  // see updating game difficulty in game loop

typedef struct {
    int row_size;
    int col_size;
} WindowInfo_t;

static int i, j;

static const char obstacle_char_map[OBSTACLE_LENGTH][OBSTACLE_WIDTH] = {
    {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
    {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
    {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
    {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
    {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'},
    {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'}};

int random_range(int upper, int lower) {
    return rand() % (upper + 1 - lower) + lower;
}

void set_cursor(int row, int col) {
    printf("\033[%d;%dH", col, row);
}

void check_windows(int row, int col) {
    if (row < MIN_WIN_ROW || col < MIN_WIN_COL) {
        printf("Terminal window size too small to render on.\n");
        printf("Must be larger than %dx%d.\n", MIN_WIN_ROW, MIN_WIN_COL);
        exit(EXIT_SUCCESS);
    }
}

void get_terminal_window_dimensions(int *rows, int *cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
}

int space_key_pressed(void) {
    return (GetKeyState(' ') & 0x8000);
}

int is_player_hits_obstacle(char **map, int row, int col) {
    return (map[row][col] == '#' ||
            map[row - 1][col] == '#' ||
            map[row - 1][col - 1] == '#' ||
            map[row - 1][col + 1] == '#' ||
            map[row - 2][col] == '#');
}

void draw_player(char **map, int row, int col, int is_jumping) {
    map[row - 2][col] = '0';                        // head
    map[row - 1][col] = '|';                        // body
    map[row][col] = (rand() % 2 == 0) ? 'W' : 'M';  // legs
    // arms
    if (is_jumping) {
        map[row - 2][col - 1] = '\\';
        map[row - 2][col + 1] = '/';
    } else {
        map[row - 1][col - 1] = '/';
        map[row - 1][col + 1] = '\\';
    }
}

void sleep_for_millis(int millis) {
    // Solution from: https://stackoverflow.com/questions/5801813/c-usleep-is-obsolete-workarounds-for-windows-mingw
    __int64 micros = millis * 1000;
    HANDLE timer;
    LARGE_INTEGER ft;
    ft.QuadPart = -(10 * micros);
    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

int run_game(WindowInfo_t win_info, char **map, int difficulty) {
    int is_jumping = 0;

    int num_rows_for_sky = (int)((win_info.row_size / 2) - (win_info.row_size / 9));
    int num_rows_for_ground = (int)(win_info.row_size / 6);
    int ground_row = win_info.row_size - num_rows_for_ground - 1;

    int player_row = ground_row;
    int player_col = (int)(win_info.col_size / 5);
    int score = 0;

    int player_jump_timer = 0;

    int obs_min_gen_gap = JUMP_WIDTH;
    int obs_max_gen_gap = GAME_LENGTH * obs_min_gen_gap;

    srand(time(NULL));
    int obs_timer = random_range(obs_min_gen_gap, obs_max_gen_gap);
    int obs_col = win_info.col_size + OBSTACLE_RADIUS;

    for (j = 0; j < win_info.col_size; j++) map[win_info.row_size - num_rows_for_ground][j] = '#';

    while (1) {
        srand(time(NULL));
        set_cursor(0, 0);
        for (i = 0; i < win_info.row_size; i++) {
            for (j = 0; j < win_info.col_size; j++) {
                printf("%c", map[i][j]);
            }
        }

        // shift everything to the left
        for (i = 0; i < win_info.row_size - 1; i++) {
            for (j = 0; j < win_info.col_size - 1; j++) {
                map[i][j] = map[i][j + 1];
            }
        }
        // erase map middle to redraw player/obstacle
        for (i = num_rows_for_sky; i <= ground_row; i++) {
            for (j = 0; j < win_info.col_size; j++) {
                map[i][j] = ' ';
            }
        }
        // add new column of ground pattern to ground region
        map[ground_row + 1][win_info.col_size - 1] = '#';

        if (space_key_pressed()) {
            is_jumping = 1;
        }

        /*
         * Update and display score.
         */
        score += 2;
        char score_message[win_info.col_size];
        sprintf(score_message, " SCORE: %d", score);
        for (i = 0; i < strlen(score_message); i++) {
            map[win_info.row_size - 1][i] = score_message[i];
        }

        if (score < GAME_MAX_DIFF_SCORE) {
            difficulty = (score * ((obs_min_gen_gap * GAME_LENGTH) - obs_min_gen_gap + 1)) / GAME_MAX_DIFF_SCORE;
        }

        if (obs_timer != 0) {
            int num_visible_chars = 0;

            // draw entering
            if (obs_col >= win_info.col_size) {
                num_visible_chars = win_info.col_size - (obs_col - OBSTACLE_RADIUS);
                for (i = 0; i < OBSTACLE_LENGTH; i++) {
                    for (j = 0; j < num_visible_chars; j++) {
                        map[ground_row - i][win_info.col_size - num_visible_chars + j] = obstacle_char_map[i][j];
                    }
                }
            }
            // draw exitting
            else if (obs_col - OBSTACLE_RADIUS < 0) {
                num_visible_chars = obs_col + OBSTACLE_RADIUS;
                for (i = 0; i < OBSTACLE_LENGTH; i++) {
                    for (j = 0; j < num_visible_chars; j++) {
                        map[ground_row - i][j] = obstacle_char_map[i][OBSTACLE_WIDTH - num_visible_chars + j];
                    }
                }
            }
            // draw midway
            else {
                for (i = 0; i < OBSTACLE_LENGTH; i++) {
                    for (j = 0; j < OBSTACLE_WIDTH; j++) {
                        map[ground_row - i][obs_col - OBSTACLE_RADIUS + j] = obstacle_char_map[i][j];
                    }
                }
            }

            if (obs_col + OBSTACLE_RADIUS > 0) {
                obs_col--;
            }
            // obstacle no longer in frame
            else {
                obs_timer = random_range(obs_min_gen_gap, obs_max_gen_gap - difficulty);
                obs_col = win_info.col_size + OBSTACLE_RADIUS;
            }
        } else {
            obs_timer--;
        }

        if (is_jumping) {
            if (player_jump_timer < JUMP_HEIGHT) {  // ascending
                player_row--;
            } else if (player_jump_timer > JUMP_HEIGHT + JUMP_AIRTIME) {  // descending
                player_row++;
            }

            player_jump_timer++;  // airtime

            // end of jump
            if (player_jump_timer == JUMP_WIDTH) {
                is_jumping = 0;
                player_jump_timer = 0;
                player_row = ground_row;
            }
        }

        // GAMEOVER!
        if (is_player_hits_obstacle(map, player_row, player_col)) return score;

        draw_player(map, player_row, player_col, is_jumping);
        fflush(stdout);
        sleep_for_millis(FRAME_DELAY);
    }
}

int main(void) {
    WindowInfo_t win_info;
    get_terminal_window_dimensions(&win_info.row_size, &win_info.col_size);
    check_windows(win_info.row_size, win_info.col_size);

    int OUTPUT_BUFFER_SIZE = win_info.row_size * win_info.col_size;
    char new_buffer[OUTPUT_BUFFER_SIZE];
    for (i = 0; i < OUTPUT_BUFFER_SIZE; i++) {
        new_buffer[i] = ' ';
    }
    setvbuf(stdout, new_buffer, _IOFBF, OUTPUT_BUFFER_SIZE);

    /*
     * Allocate and initiate game map.
     */
    char **map = malloc(win_info.row_size * sizeof(char *));
    for (i = 0; i < win_info.row_size; i++) {
        map[i] = malloc(win_info.col_size);
        for (j = 0; j < win_info.col_size; j++) {
            map[i][j] = ' ';
        }
    }

    int difficulty = 0;
    int score = run_game(win_info, map, difficulty);

    set_cursor(0, 0);
    for (i = 0; i < win_info.row_size; i++) {
        for (j = 0; j < win_info.col_size; j++) {
            printf(" ");
        }
    }
    set_cursor(0, 0);

    printf("Game over :(\nFinal Score: %d\n", score);
    return 0;
}