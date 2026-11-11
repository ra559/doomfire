
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>

#define MAX_HEAT 36
#define FRAME_DELAY 40000L
#define DEFAULT_FONT_SIZE 14
#define MIN_FONT_SIZE 7
#define MAX_FONT_SIZE 140

/* -------------------------------------------------
   CONFIGURATION
   ------------------------------------------------- */

typedef enum {
    COLOR_CLASSIC,
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_PURPLE
} FireColor;

static struct termios original_termios;
static bool terminal_configured = false;
static volatile sig_atomic_t running = 1;

static uint8_t *fire = NULL;

static int width = 0;
static int height = 0;

static long frame_delay = FRAME_DELAY;

static FireColor fire_color = COLOR_CLASSIC;

static int font_size = DEFAULT_FONT_SIZE;
static int font_scale = 1;

/* Text storage */

static char **text_lines = NULL;
static int text_count = 0;
static int text_width = 0;

/* -------------------------------------------------
   FIRE PALETTES
   ------------------------------------------------- */

/* Classic DOOM palette */

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

/* -------------------------------------------------
   BITMAP FONT
   ------------------------------------------------- */

/*
   5x7 bitmap font.

   Supported:
   A-Z
   0-9
   Space

   Each character occupies 5 columns and 7 rows.
*/

static const uint8_t font5x7[37][7] = {

    /* A-Z */

    {14,17,17,31,17,17,17}, // A
    {30,17,17,30,17,17,30}, // B
    {14,17,16,16,16,17,14}, // C
    {30,17,17,17,17,17,30}, // D
    {31,16,16,30,16,16,31}, // E
    {31,16,16,30,16,16,16}, // F
    {14,17,16,23,17,17,15}, // G
    {17,17,17,31,17,17,17}, // H
    {14,4,4,4,4,4,14},       // I
    {7,2,2,2,18,18,12},      // J
    {17,18,20,24,20,18,17},  // K
    {16,16,16,16,16,16,31},  // L
    {17,27,21,21,17,17,17},  // M
    {17,25,21,19,17,17,17},  // N
    {14,17,17,17,17,17,14},  // O
    {30,17,17,30,16,16,16},  // P
    {14,17,17,17,21,18,13},  // Q
    {30,17,17,30,20,18,17},  // R
    {15,16,16,14,1,1,30},    // S
    {31,4,4,4,4,4,4},        // T
    {17,17,17,17,17,17,14},  // U
    {17,17,17,17,17,10,4},   // V
    {17,17,17,21,21,21,10},  // W
    {17,17,10,4,10,17,17},   // X
    {17,17,10,4,4,4,4},       // Y
    {31,1,2,4,8,16,31},      // Z

    /* 0-9 */

    {14,17,19,21,25,17,14},  // 0
    {4,12,4,4,4,4,14},       // 1
    {14,17,1,2,4,8,31},      // 2
    {30,1,1,14,1,1,30},      // 3
    {2,6,10,18,31,2,2},      // 4
    {31,16,16,30,1,1,30},    // 5
    {14,16,16,30,17,17,14},  // 6
    {31,1,2,4,8,8,8},        // 7
    {14,17,17,14,17,17,14},  // 8
    {14,17,17,15,1,1,14},    // 9

    /* Space */

    {0,0,0,0,0,0,0}
};

/* Return bitmap for a character */

static const uint8_t *get_glyph(char ch)
{
    static const uint8_t blank[7] = {0};

    ch = (char)toupper((unsigned char)ch);

    if (ch >= 'A' && ch <= 'Z')
        return font5x7[ch - 'A'];

    if (ch >= '0' && ch <= '9')
        return font5x7[26 + ch - '0'];

    return blank;
}

/* Calculate scaled text width */

static int get_scaled_text_width(const char *line)
{
    int characters = (int)strlen(line);

    if (characters == 0)
        return 0;

    return (characters * 6 - 1) * font_scale;
}

/* Determine whether a terminal cell belongs to a glyph */

static bool is_glyph_pixel(int x, int y)
{
    int text_height = 7 * font_scale;

    int total_height = text_count * text_height;

    int top = (height - total_height) / 2;

    int text_y = y - top;

    if (text_y < 0)
        return false;

    int line_index = text_y / text_height;

    if (line_index >= text_count)
        return false;

    int line_y =
        (text_y % text_height) / font_scale;

    const char *line = text_lines[line_index];

    int scaled_width = get_scaled_text_width(line);

    int left = (width - scaled_width) / 2;

    int text_x = x - left;

    if (text_x < 0 || text_x >= scaled_width)
        return false;

    int character_index =
        text_x / (6 * font_scale);

    int character_x =
        (text_x % (6 * font_scale)) / font_scale;

    if (character_index >= (int)strlen(line))
        return false;

    if (character_x >= 5)
        return false;

    const uint8_t *glyph =
        get_glyph(line[character_index]);

    return (glyph[line_y] & (1 << (4 - character_x))) != 0;
}

/* -------------------------------------------------
   TEXT MANAGEMENT
   ------------------------------------------------- */

/* Add a line to the text buffer */

static bool add_text_line(const char *source, size_t length)
{
    char **new_lines = realloc(
        text_lines,
        (size_t)(text_count + 1) * sizeof(char *)
    );

    if (!new_lines)
        return false;

    text_lines = new_lines;

    char *line = malloc(length + 1);

    if (!line)
        return false;

    for (size_t i = 0; i < length; i++) {

        unsigned char ch = (unsigned char)source[i];

        line[i] = (ch >= 32 && ch <= 126)
            ? (char)ch
            : '?';
    }

    line[length] = '\0';

    text_lines[text_count++] = line;

    if ((int)length > text_width)
        text_width = (int)length;

    return true;
}

/* Load a string, interpreting \n as a newline */

static bool load_text_string(const char *input)
{
    size_t length = strlen(input);

    char *expanded = malloc(length + 1);

    if (!expanded)
        return false;

    size_t output = 0;

    for (size_t i = 0; i < length; i++) {

        if (input[i] == '\\' && input[i + 1] == 'n') {

            expanded[output++] = '\n';
            i++;
        }
        else {
            expanded[output++] = input[i];
        }
    }

    expanded[output] = '\0';

    char *start = expanded;
    char *current = expanded;

    bool success = true;

    while (true) {

        if (*current == '\n' || *current == '\0') {

            if (!add_text_line(
                start,
                (size_t)(current - start)
            )) {
                success = false;
                break;
            }

            if (*current == '\0')
                break;

            start = current + 1;
        }

        current++;
    }

    free(expanded);

    return success;
}

/* Load text from a file */

static bool load_text_file(const char *filename)
{
    FILE *file = fopen(filename, "r");

    if (!file) {
        perror(filename);
        return false;
    }

    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    bool success = true;

    while ((length = getline(&line, &capacity, file)) != -1) {

        while (length > 0 &&
              (line[length - 1] == '\n' ||
               line[length - 1] == '\r')) {
            length--;
        }

        if (!add_text_line(line, (size_t)length)) {
            success = false;
            break;
        }
    }

    if (ferror(file))
        success = false;

    free(line);
    fclose(file);

    if (!success)
        return false;

    if (text_count == 0)
        return add_text_line("", 0);

    return true;
}

/* Free text memory */

static void free_text(void)
{
    for (int i = 0; i < text_count; i++)
        free(text_lines[i]);

    free(text_lines);

    text_lines = NULL;
    text_count = 0;
    text_width = 0;
}

/* -------------------------------------------------
   TERMINAL MANAGEMENT
   ------------------------------------------------- */

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

static void handle_signal(int signal_number)
{
    (void)signal_number;
    running = 0;
}

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

    printf("\033[?1049h");
    printf("\033[?25l");
    printf("\033[2J");

    fflush(stdout);
}

/* Detect terminal dimensions */

static bool update_dimensions(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return false;

    if (!ws.ws_col || !ws.ws_row)
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

/* -------------------------------------------------
   FIRE ENGINE
   ------------------------------------------------- */

static void initialize_fire(void)
{
    memset(
        fire,
        0,
        (size_t)width * height
    );
}

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

/* Convert heat into RGB colors */

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

    if (fire_color == COLOR_CLASSIC) {

        int index =
            heat * (PALETTE_SIZE - 1) / MAX_HEAT;

        *r = palette[index][0];
        *g = palette[index][1];
        *b = palette[index][2];

        return;
    }

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

/* -------------------------------------------------
   SCREEN RENDERER
   ------------------------------------------------- */

static void render_screen(void)
{
    printf("\033[H");

    for (int y = 0; y < height; y++) {

        for (int x = 0; x < width; x++) {

            int heat = fire[y * width + x];

            int r, g, b;

            get_fire_color(heat, &r, &g, &b);

            /* Fire background */

            printf(
                "\033[48;2;%d;%d;%dm",
                r, g, b
            );

            /* Text foreground */

            if (is_glyph_pixel(x, y)) {

                printf(
                    "\033[1;38;2;255;255;255m█\033[39;22m"
                );
            }
            else {
                putchar(' ');
            }
        }

        printf("\033[0m");

        if (y < height - 1)
            putchar('\n');
    }

    fflush(stdout);
}

/* -------------------------------------------------
   KEYBOARD CONTROLS
   ------------------------------------------------- */

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

/* -------------------------------------------------
   COMMAND-LINE ARGUMENTS
   ------------------------------------------------- */

static void print_help(void)
{
    printf(
        "DoomFire - Terminal Fire Typography\n\n"

        "Usage:\n"
        "  doomfire [OPTIONS]\n\n"

        "Input options:\n"
        "  -t, --text STRING    Display text\n"
        "  -f, --file FILE      Display text from file\n\n"

        "Color options:\n"
        "  -classic             Classic DOOM fire\n"
        "  -blue                Blue fire\n"
        "  -green               Green fire\n"
        "  -purple              Purple fire\n\n"

        "Font options:\n"
        "  -s, --size SIZE      Font size (default: 14)\n\n"

        "General options:\n"
        "  -h, --help           Display help\n\n"

        "Keyboard controls:\n"
        "  q / ESC              Quit\n"
        "  + / =                Increase speed\n"
        "  -                    Decrease speed\n"
        "  r                    Restart fire\n"
    );
}

static void parse_arguments(int argc, char *argv[])
{
    bool input_provided = false;

    for (int i = 1; i < argc; i++) {

        /* Help */

        if (
            strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0
        ) {

            print_help();
            exit(EXIT_SUCCESS);
        }

        /* Color themes */

        else if (strcmp(argv[i], "-classic") == 0) {
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

        /* Font size */

        else if (
            strcmp(argv[i], "-s") == 0 ||
            strcmp(argv[i], "--size") == 0
        ) {

            if (++i >= argc) {
                fprintf(stderr, "Missing font size.\n");
                exit(EXIT_FAILURE);
            }

            char *end;

            long size = strtol(argv[i], &end, 10);

            if (
                *end != '\0' ||
                size < MIN_FONT_SIZE ||
                size > MAX_FONT_SIZE
            ) {

                fprintf(
                    stderr,
                    "Invalid font size. Use a value between %d and %d.\n",
                    MIN_FONT_SIZE,
                    MAX_FONT_SIZE
                );

                exit(EXIT_FAILURE);
            }

            font_size = (int)size;

            /*
               14 = 1x
               24 = 2x
               36 = 3x
               48 = 4x
            */

            font_scale = (font_size + 13) / 14;
        }

        /* String input */

        else if (
            strcmp(argv[i], "-t") == 0 ||
            strcmp(argv[i], "--text") == 0
        ) {

            if (++i >= argc) {
                fprintf(stderr, "Missing text argument.\n");
                exit(EXIT_FAILURE);
            }

            if (input_provided) {
                fprintf(stderr, "Specify only one input source.\n");
                exit(EXIT_FAILURE);
            }

            if (!load_text_string(argv[i])) {
                fprintf(stderr, "Unable to load text.\n");
                exit(EXIT_FAILURE);
            }

            input_provided = true;
        }

        /* File input */

        else if (
            strcmp(argv[i], "-f") == 0 ||
            strcmp(argv[i], "--file") == 0
        ) {

            if (++i >= argc) {
                fprintf(stderr, "Missing filename.\n");
                exit(EXIT_FAILURE);
            }

            if (input_provided) {
                fprintf(stderr, "Specify only one input source.\n");
                exit(EXIT_FAILURE);
            }

            if (!load_text_file(argv[i])) {
                fprintf(stderr, "Unable to load file.\n");
                exit(EXIT_FAILURE);
            }

            input_provided = true;
        }

        /* Invalid option */

        else {

            fprintf(
                stderr,
                "Unknown option: %s\n",
                argv[i]
            );

            fprintf(stderr, "Use --help for usage.\n");

            exit(EXIT_FAILURE);
        }
    }

    if (!input_provided) {

        fprintf(stderr, "No text input provided.\n");
        fprintf(stderr, "Use -t STRING or -f FILE.\n");

        exit(EXIT_FAILURE);
    }
}

/* -------------------------------------------------
   MAIN
   ------------------------------------------------- */

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

        render_screen();

        handle_input();

        struct timespec delay = {
            .tv_sec = frame_delay / 1000000,
            .tv_nsec = (frame_delay % 1000000) * 1000
        };

        nanosleep(&delay, NULL);
    }

    free(fire);
    fire = NULL;

    free_text();

    return EXIT_SUCCESS;
}

