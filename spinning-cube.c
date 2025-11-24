#include <math.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define RAND() ((double)rand() / (double)RAND_MAX)



struct vec3 {
    double x;
    double y;
    double z;
};

struct vec2 {
    double x;
    double y;
};

struct cube {
    struct vec3 rotation;
    struct vec2 position;
    struct vec2 velocity;
    struct vec3 rotational_velocity;
    double size;
};

struct bounds {
    struct vec2 min;
    struct vec2 max;
};

void clearScreen() { printf("\033[H\033[J"); }

void enterAlternateBuffer() { printf("\e[?1049h"); }
void exitAlternateBuffer() { printf("\e[?1049l"); }

void restore_terminal(int signo) {
    exitAlternateBuffer();
    fflush(stdout);
    exit(0);
}


int color = 31;
char chars[3] = {'#', '*', '~'};
struct vec2 aspectRatio;
struct bounds bounds;
struct bounds prev_bounds;

// Function to compute cross product of vector AB and AC
double crossProduct(struct vec2 A, struct vec2 B, struct vec2 C) {
    return (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
}

int pointInTriangle(struct vec2 p, struct vec2 a, struct vec2 b, struct vec2 c) {
    double c1 = crossProduct(a, b, p);
    double c2 = crossProduct(b, c, p);
    double c3 = crossProduct(c, a, p);
    /* all same sign (allow zero) */
    return (c1 >= 0 && c2 >= 0 && c3 >= 0) || (c1 <= 0 && c2 <= 0 && c3 <= 0);
}

int pointInQuad(struct vec2 p, struct vec2 q[4]) {
    return pointInTriangle(p, q[0], q[1], q[2]) ||
           pointInTriangle(p, q[0], q[2], q[3]);
}


void getBounds(struct vec2 *points, size_t size) {
    bounds.min.x = points[0].x;
    bounds.min.y = points[0].y;
    bounds.max.x = points[0].x;
    bounds.max.y = points[0].y;

    for (size_t i = 1; i < size; i++) {
        if (points[i].x < bounds.min.x) bounds.min.x = points[i].x;
        if (points[i].y < bounds.min.y) bounds.min.y = points[i].y;
        if (points[i].x > bounds.max.x) bounds.max.x = points[i].x;
        if (points[i].y > bounds.max.y) bounds.max.y = points[i].y;
    }
}

void getColoredChars(char faceString[3][8], char chars[3]) {
    for (int i = 0; i < 3; i++) {
        int code = color + i;
        faceString[i][0] = '\033';
        faceString[i][1] = '[';
        faceString[i][2] = '3';
        faceString[i][3] = '0' + (code % 10);
        faceString[i][4] = 'm';
        faceString[i][5] = chars[i];
        faceString[i][6] = '\0';
    }
}

void draw(struct vec2 faces[3][4], char chars[], struct winsize *w) {
    char coloredChars[3][8];
    getColoredChars(coloredChars, chars);

    getBounds((struct vec2 *)faces, 12);

    struct bounds dirtyBounds;
    dirtyBounds.min.x = fmin(bounds.min.x, prev_bounds.min.x);
    dirtyBounds.min.y = fmin(bounds.min.y, prev_bounds.min.y);
    dirtyBounds.max.x = fmax(bounds.max.x, prev_bounds.max.x);
    dirtyBounds.max.y = fmax(bounds.max.y, prev_bounds.max.y);
    prev_bounds = bounds;

    int x, y, size_x, size_y;
    x = floor((dirtyBounds.min.x / aspectRatio.x) * w->ws_col);
    y = floor((dirtyBounds.min.y / aspectRatio.y) * w->ws_row);
    size_x = ceil((dirtyBounds.max.x / aspectRatio.x) * w->ws_col);
    size_y = ceil((dirtyBounds.max.y / aspectRatio.y) * w->ws_row);

    if (size_x > w->ws_col) { size_x = w->ws_col; }
    if (size_y > w->ws_row) { size_y = w->ws_row; }
    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }

    char output[size_y][size_x][8];
    for (int i = y; i < size_y; i++) {
        for (int j = x; j < size_x; j++) {
            struct vec2 P = {
                ((double)j + 0.5) / w->ws_col * aspectRatio.x,
                ((double)i + 0.5) / w->ws_row * aspectRatio.y
            };
            int c;
            for (c = 0; c < 3; c++) {
                if (pointInQuad(P, faces[c])) {
                    strcpy(output[i - y][j - x], coloredChars[c]);
                    break;
                } else {
                    output[i - y][j - x][0] = ' ';
                    output[i - y][j - x][1] = '\0';
                }
            }
        }
    }

    printf("\033[H");      

    for (int i = 0; i < (size_y - y); i++) {
        printf("\033[%d;%dH", i + y + 1, x + 1);  // goto row/col (1-indexed)
        for (int j = 0; j < (size_x - x); j++) {
            fputs(output[i][j], stdout);
        }
    }
    printf("\e[?25l");
    fflush(stdout);

}

void updateCube(struct cube* cube) {
    // get velocity
    struct vec2 *velocity = &cube->velocity;
    struct vec3 *rotational_velocity = &cube->rotational_velocity;

    cube->position.x += velocity->x;
    cube->position.y += velocity->y;

    // update rotation
    cube->rotation.x += rotational_velocity->x;
    cube->rotation.y += rotational_velocity->y;
    cube->rotation.z += rotational_velocity->z;
}

void rotate(struct vec3 *points, size_t size, struct vec3 rotation) {
    for (int i = 0; i < size; i++) {
        double x = points[i].x;
        double y = points[i].y;
        double z = points[i].z;

        double cx = cos(rotation.x), sx = sin(rotation.x);
        double cy = cos(rotation.y), sy = sin(rotation.y);
        double cz = cos(rotation.z), sz = sin(rotation.z);


        double x1 = x;
        double y1 = y * cx - z * sx;
        double z1 = y * sx + z * cx;

        double x2 = x1 * cy + z1 * sy;
        double y2 = y1;
        double z2 = -x1 * sy + z1 * cy;

        double x3 = x2 * cz - y2 * sz;
        double y3 = x2 * sz + y2 * cz;
        double z3 = z2;

        points[i].x = x3;
        points[i].y = y3;
        points[i].z = z3;
    }
}

void rotateFace(struct vec3 *face, struct vec3 rotation) {
    rotate(face, 4, rotation);
}

void rotateNormals(struct vec3 *normals, struct vec3 rotation) {
    rotate(normals, 6, rotation);
}

void projectQuadToTerminal(struct vec3 *quad, struct vec2 *projected_quad, struct vec2 position, double cube_size) {
    for (int i = 0; i < 4; i++) {
        projected_quad[i].x = (quad[i].x * cube_size / 2) + position.x;
        projected_quad[i].y = (quad[i].y * cube_size / 2) + position.y;
    }
}

// Returns a bitmask of visible cube faces
// Bit 0 = right, 1 = left, 2 = top, 3 = bottom, 4 = front, 5 = back
int getVisibleFaces(struct vec3 rotation) {
    struct vec3 normals[6] = {
        {1, 0, 0},   // right
        {-1, 0, 0},  // left
        {0, 1, 0},   // top
        {0, -1, 0},  // bottom
        {0, 0, 1},   // front
        {0, 0, -1}   // back
    };

    struct vec3 view = {0, 0, 1};
    // Rotate view by inverse of cube rotation
    double cx = cos(-rotation.x), sx = sin(-rotation.x);
    double cy = cos(-rotation.y), sy = sin(-rotation.y);
    double cz = cos(-rotation.z), sz = sin(-rotation.z);

    // Rotate around Z
    double x1 = view.x * cz - view.y * sz;
    double y1 = view.x * sz + view.y * cz;
    double z1 = view.z;

    // Rotate around Y
    double x2 = x1 * cy + z1 * sy;
    double y2 = y1;
    double z2 = -x1 * sy + z1 * cy;

    // Rotate around X
    double x3 = x2;
    double y3 = y2 * cx - z2 * sx;
    double z3 = y2 * sx + z2 * cx;

    view.x = x3;
    view.y = y3;
    view.z = z3;

    int mask = 0;
    for (int i = 0; i < 6; i++) {
        double dp = normals[i].x * view.x + normals[i].y * view.y + normals[i].z * view.z;
        if (dp > 0) mask |= (1 << i);
    }
    return mask;
}



void updateFaces(struct cube *cube, struct vec2 projections[3][4]) {
    struct vec3 faces[6][4] = {
        {{1, 1, 1}, {1, -1, 1}, {1, -1, -1}, {1, 1, -1}},     // right
        {{-1, 1, 1}, {-1, -1, 1}, {-1, -1, -1}, {-1, 1, -1}}, // left
        {{1, 1, 1}, {1, 1, -1}, {-1, 1, -1}, {-1, 1, 1}},     // top
        {{1, -1, 1}, {1, -1, -1}, {-1, -1, -1}, {-1, -1, 1}}, // bottom
        {{1, 1, 1}, {1, -1, 1}, {-1, -1, 1}, {-1, 1, 1}},     // front
        {{1, 1, -1}, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}}  // back
    };

    int mask = getVisibleFaces(cube->rotation);

    int k = 0;
    for (int i = 0; i < 6; i++) {
        if (!(mask & (1 << i))) continue; // skip invisible faces

        rotate(faces[i], 4, cube->rotation);
        projectQuadToTerminal(faces[i], projections[k], cube->position, cube->size);
        k++;
        if (k == 3) break; // only draw top 3 visible faces
    }
}

// change color variable range from 31 to 35
void updateColor() { color = (color + 1) % 5 + 31; }

void handleEdgeCollision(struct vec2 faces[3][4], struct cube *cube) {
    struct vec2 *velocity = &cube->velocity;
    struct vec3 *rotational_velocity = &cube->rotational_velocity;

    if (bounds.min.x >= 0 && bounds.max.x <= aspectRatio.x &&
        bounds.min.y >= 0 && bounds.max.y <= aspectRatio.y) {
        return; // no collision
    }
    if (bounds.min.x < 0 && velocity->x < 0) {
        velocity->x = fabs(velocity->x);
        rotational_velocity->y *= -1;
        updateColor();
    }
    if (bounds.max.x > aspectRatio.x && velocity->x > 0) {
        velocity->x = -fabs(velocity->x);
        rotational_velocity->y *= -1;
        updateColor();
    }
    if (bounds.min.y < 0 && velocity->y < 0) {
        velocity->y = fabs(velocity->y);
        rotational_velocity->x *= -1;
        updateColor();
    }
    if (bounds.max.y > aspectRatio.y && velocity->y > 0) {
        velocity->y = -fabs(velocity->y);
        rotational_velocity->x *= -1;
        updateColor();
    }
}

void updateAspectRatio(struct winsize *w) {
    //detect change
    int size_x = w->ws_col;
    int size_y = w->ws_row * 2;
    aspectRatio.x = (double)size_x / MIN(size_x, size_y);
    aspectRatio.y = (double)size_y / MIN(size_x, size_y);
}

// Set terminal to raw mode (no buffering, no echo)
struct termios orig_termios;

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); // no echo, no canonical mode
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// Restore terminal to normal mode
void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

// Non-blocking keyboard read
int kbhit() {
    struct timeval tv = {0, 0};
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    return select(STDIN_FILENO+1, &readfds, NULL, NULL, &tv);
}

int getch() {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}



// Returns 1 if the program should quit, 0 otherwise
int handleKeyboardInput(struct cube *cube) {
    if (!kbhit()) return 0; // no key pressed

    int c = getch();
    if (c == 'q') return 1; // quit

    if (c == '-') {
        cube->size -= 0.05;
        if (cube->size < 0.05) cube->size = 0.05; // minimum size
    } else if (c == '+') {
        cube->size += 0.05;
        if (cube->size > 1.0) cube->size = 1.0; // maximum size
    } 
    return 0; // continue running
}


int main(int argc, char **argv) {
    struct cube cube;

    cube.position = (struct vec2){0.3, 0.3};
    cube.rotation = (struct vec3){0.1, 0.2, 0.3};
    cube.size = 0.3;
    cube.velocity = (struct vec2){0.01, 0.01};
    cube.rotational_velocity = (struct vec3){0.05, 0.05, 0};


    struct vec2 faces[3][4];

    enableRawMode();
    atexit(disableRawMode); // ensure normal terminal on exit
    enterAlternateBuffer();
    signal(SIGINT, restore_terminal); // ensure normal terminal on Ctrl-C


    struct winsize w, prev_w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &prev_w);

    while (1) {
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        if (w.ws_col != prev_w.ws_col || w.ws_row != prev_w.ws_row) {
            clearScreen();
            prev_w = w;
        }
        updateAspectRatio(&w);
        updateFaces(&cube, faces);
        draw(faces, chars, &w);
        handleEdgeCollision(faces, &cube);
        updateCube(&cube);
        struct timespec ts = {0, 40 * 1000 * 1000};

        if (handleKeyboardInput(&cube)) break;

        nanosleep(&ts, NULL);
    }

    exitAlternateBuffer();
    return 0; // make sure your main returns in
}
