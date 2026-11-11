
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>

#define MAX_HEAT 36
#define FRAME_DELAY 40000L

typedef enum {
    COLOR_CLASSIC,
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_PURPLE
} FireColor;

static struct termios original_termios;
static bool terminal_configured = false;

static uint8_t *fire = NULL;

static int width = 0;
static int height = 0;

static long frame_delay = FRAME_DELAY;

static FireColor fire_color = COLOR_CLASSIC;

static volatile sig_atomic_t running = 1;

/* Classic DOOM fire palette */

static const uint8_t palette[][3] = {
    {  0,   0,   0},
    { 25,   0,   0},
    { 50,   0,   0},
    { 75,   0,   0},
    {100,   0,   0},
    {125,   0,   0},
    {150,   0,   0},
    {175,   0,   0},
    {200,   0,   0},
    {225,   0,   0},
    {255,   0,   0},
    {255,  30,   0},
    {255,  60,   0},
    {255,  90,   0},
    {255, 120,   0},
    {255, 150,   0},
    {255, 180,   0},
    {255, 200,   0},
    {255, 220,   0},
    {255, 240,   0},
    {255, 255,   0},
    {255, 255,  50},
    {255, 255, 100},
    {255, 255, 150},
    {255, 255, 200},
    {255, 255, 255}
};

#define PALETTE_SIZE (sizeof(palette) / sizeof(palette[0]))

/* Alternative color palettes */

static const uint8_t color_stops[][5][3] = {
    [COLOR_BLUE] = {
        {  0,   0,   0},
        {  0,   0,  50},
        {  0,  30, 150},
        {  0, 150, 255},
        {180, 240, 255}
    },

    [COLOR_GREEN] = {
        {  0,   0,   0},
        {  0,  40,   0},
        {  0, 130,   0},
        { 80, 255,   0},
        {255, 255, 150}
    },

    [COLOR_PURPLE] = {
        {  0,   0,   0},
        { 40,   0,  70},
        {120,   0, 180},
        {220,  30, 255},
        {255, 190, 255}
    }
};

/* Handle termination signals */

static void handle_signal(int signal_number)
{
    (void)signal_number;
    running = 0;
}

/* Restore terminal settings */

static void restore_terminal(void)
{
    if (terminal_configured) {
        tcsetattr(
            STDIN_FILENO,
            TCSAFLUSH,
            &original_termios
        );

        terminal_configured = false;
    }

    printf("\033[0m");
    printf("\033[?25h");
    printf("\033[?1049l");

    fflush(stdout);
}

/* Configure terminal */

static void setup_terminal(void)
{
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr,
            "doomfire requires an interactive terminal.\n");
        exit(EXIT_FAILURE);
    }

    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    struct termios raw = original_termios;

    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_iflag &= ~(IXON | ICRNL);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }

    terminal_configured = true;

    atexit(restore_terminal);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);

    printf("\033[?1049h");  // Alternate screen
    printf("\033[?25l");    // Hide cursor
    printf("\033[2J");      // Clear screen

    fflush(stdout);
}

/* Detect terminal dimensions and resize fire buffer */

static bool update_dimensions(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return false;

    if (ws.ws_col == 0 || ws.ws_row == 0)
        return false;

    if (width == ws.ws_col && height == ws.ws_row)
        return true;

    int new_width = ws.ws_col;
    int new_height = ws.ws_row;

    uint8_t *new_fire = calloc(
        (size_t)new_width * new_height,
        sizeof(uint8_t)
    );

    if (!new_fire)
        return false;

    free(fire);

    fire = new_fire;

    width = new_width;
    height = new_height;

    return true;
}

/* Initialize fire simulation */

static void initialize_fire(void)
{
    memset(
        fire,
        0,
        (size_t)width * height
    );
}

/* Create permanent heat source at bottom */

static void create_fire_source(void)
{
    int bottom = (height - 1) * width;

    for (int x = 0; x < width; x++)
        fire[bottom + x] = MAX_HEAT;
}

/* Doom fire propagation algorithm */

static void propagate_fire(void)
{
    for (int y = 1; y < height; y++) {

        for (int x = 0; x < width; x++) {

            int source = y * width + x;

            int decay = rand() % 3;
            int spread = rand() % 3 - 1;

            int new_x = x + spread;

            if (new_x < 0)
                new_x = 0;

            if (new_x >= width)
                new_x = width - 1;

            int destination =
                (y - 1) * width + new_x;

            int heat = fire[source] - decay;

            if (heat < 0)
                heat = 0;

            fire[destination] = heat;
        }
    }
}

/* Convert heat values into RGB colors */

static void get_fire_color(
    int heat,
    int *r,
    int *g,
    int *b
)
{
    if (heat <= 0) {
        *r = *g = *b = 0;
        return;
    }

    /* Classic palette */

    if (fire_color == COLOR_CLASSIC) {

        int index =
            heat * (PALETTE_SIZE - 1) / MAX_HEAT;

        *r = palette[index][0];
        *g = palette[index][1];
        *b = palette[index][2];

        return;
    }

    /* Alternative palettes */

    float position =
        (float)heat / MAX_HEAT * 4.0f;

    int segment = (int)position;

    if (segment > 3)
        segment = 3;

    float fraction = position - segment;

    const uint8_t (*stops)[3] =
        color_stops[fire_color];

    *r = stops[segment][0] +
        (stops[segment + 1][0] -
         stops[segment][0]) * fraction;

    *g = stops[segment][1] +
        (stops[segment + 1][1] -
         stops[segment][1]) * fraction;

    *b = stops[segment][2] +
        (stops[segment + 1][2] -
         stops[segment][2]) * fraction;
}

/* Render fire in terminal */

static void render_fire(void)
{
    printf("\033[H");

    for (int y = 0; y < height; y++) {

        for (int x = 0; x < width; x++) {

            int heat = fire[y * width + x];

            int r, g, b;

            get_fire_color(heat, &r, &g, &b);

            printf(
                "\033[48;2;%d;%d;%dm ",
                r, g, b
            );
        }

        printf("\033[0m");

        if (y < height - 1)
            putchar('\n');
    }

    fflush(stdout);
}

/* Handle keyboard input */

static void handle_input(void)
{
    char key;

    while (read(STDIN_FILENO, &key, 1) == 1) {

        switch (key) {

            case 'q':
            case 'Q':
            case 27:
                running = 0;
                break;

            case '+':
            case '=':
                if (frame_delay > 5000)
                    frame_delay -= 5000;
                break;

            case '-':
                if (frame_delay < 200000)
                    frame_delay += 5000;
                break;

            case 'r':
            case 'R':
                initialize_fire();
                break;
        }
    }
}

/* Parse command-line arguments */

static void parse_arguments(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-classic") == 0) {
            fire_color = COLOR_CLASSIC;
        }

        else if (strcmp(argv[i], "-blue") == 0) {
            fire_color = COLOR_BLUE;
        }

        else if (strcmp(argv[i], "-green") == 0) {
            fire_color = COLOR_GREEN;
        }

        else if (strcmp(argv[i], "-purple") == 0) {
            fire_color = COLOR_PURPLE;
        }

        else if (
            strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0
        ) {
            printf(
                "DoomFire - Terminal Fire Simulation\n\n"
                "Usage: doomfire [OPTION]\n\n"
                "Color options:\n"
                "  -classic   Classic DOOM fire\n"
                "  -blue      Blue fire\n"
                "  -green     Green fire\n"
                "  -purple    Purple fire\n\n"
                "General options:\n"
                "  -h         Display this help\n"
                "  --help     Display this help\n\n"
                "Keyboard controls:\n"
                "  q / ESC    Quit\n"
                "  + / =      Increase speed\n"
                "  -          Decrease speed\n"
                "  r          Restart fire\n"
            );

            exit(EXIT_SUCCESS);
        }

        else {
            fprintf(
                stderr,
                "Unknown option: %s\n"
                "Use --help for available options.\n",
                argv[i]
            );

            exit(EXIT_FAILURE);
        }
    }
}

/* Main application */

int main(int argc, char *argv[])
{
    srand((unsigned int)time(NULL));

    parse_arguments(argc, argv);

    setup_terminal();

    while (running) {

        if (!update_dimensions()) {

            struct timespec pause = {
                .tv_sec = 0,
                .tv_nsec = 100000000L
            };

            nanosleep(&pause, NULL);

            continue;
        }

        create_fire_source();

        propagate_fire();

        render_fire();

        handle_input();

        struct timespec delay = {
            .tv_sec = frame_delay / 1000000,
            .tv_nsec = (frame_delay % 1000000) * 1000
        };

        nanosleep(&delay, NULL);
    }

    free(fire);
    fire = NULL;

    return EXIT_SUCCESS;
}

