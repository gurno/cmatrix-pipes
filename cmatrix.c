/*
    cmatrix.c

    Copyright (C) 1999-2017 Chris Allegretta
    Copyright (C) 2017-Present Abishek V Ashok

    This file is part of cmatrix.

    cmatrix is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cmatrix is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cmatrix. If not, see <http://www.gnu.org/licenses/>.

*/

#define NCURSES_WIDECHAR 1

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <locale.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef EXCLUDE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#ifdef _WIN32
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#elif defined(HAVE_TERMIO_H)
#include <termio.h>
#endif

/* For named pipes */
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>

#define PIPE_BUF_SIZE 256

#ifdef __CYGWIN__
#define TIOCSTI 0x5412
#endif

/* Matrix typedef */
typedef struct cmatrix {
    int val;
    bool is_head;
} cmatrix;

/* Global variables */
int console = 0;
int xwindow = 0;
int lock = 0;
cmatrix **matrix = (cmatrix **) NULL;
int *length = NULL;  /* Length of cols in each line */
int *spaces = NULL;  /* Spaces left to fill */
int *updates = NULL; /* What does this do again? */
#ifndef _WIN32
volatile sig_atomic_t signal_status = 0; /* Indicates a caught signal */
#endif

/* Global variables for pipe control */
char *pipe_path = NULL;      /* Path to the named pipe */
int pipe_fd = -1;            /* File descriptor for the pipe */
pthread_t pipe_thread;       /* Thread for reading from pipe */
int use_pipe = 0;            /* Flag for pipe usage */
pthread_mutex_t pipe_mutex;  /* Mutex for shared resources (initialized in setup_pipe) */
volatile int pipe_active = 0;/* Flag indicating if pipe is currently active */
int pause = 0;               /* Global pause state, can be controlled via pipe */

int va_system(char *str, ...) {

    va_list ap;
    char buf[133];

    va_start(ap, str);
    vsnprintf(buf, sizeof(buf), str, ap);
    va_end(ap);
    return system(buf);
}

/* What we do when we're all set to exit */
/* Clean up pipe resources */
void cleanup_pipe(void) {
    /* Set flag to indicate pipe is no longer active */
    pipe_active = 0;
    
    /* Safely close and clean up pipe resources */
    if (pipe_fd >= 0) {
        close(pipe_fd);
        pipe_fd = -1;
    }
    
    if (pipe_path) {
        /* Remove the pipe file from the filesystem */
        unlink(pipe_path);
        free(pipe_path);
        pipe_path = NULL;
    }
    
    /* Clean up mutex */
    pthread_mutex_destroy(&pipe_mutex);
}

void finish(void) {
    curs_set(1);
    clear();
    refresh();
    resetty();
    endwin();
    if (console) {
#ifdef HAVE_CONSOLECHARS
        va_system("consolechars -d");
#elif defined(HAVE_SETFONT)
        va_system("setfont");
#endif
    }
    
    /* Clean up pipe if used */
    if (use_pipe) {
        pthread_cancel(pipe_thread);
        pthread_join(pipe_thread, NULL);
        cleanup_pipe();
    }
    
    exit(0);
}

/* What we do when we're all set to exit */
void c_die(char *msg, ...) {

    va_list ap;

    curs_set(1);
    clear();
    refresh();
    resetty();
    endwin();

    if (console) {
#ifdef HAVE_CONSOLECHARS
        va_system("consolechars -d");
#elif defined(HAVE_SETFONT)
        va_system("setfont");
#endif
    }
    
    /* Clean up pipe if used */
    if (use_pipe) {
        pthread_cancel(pipe_thread);
        pthread_join(pipe_thread, NULL);
        cleanup_pipe();
    }

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    exit(0);
}

/* Forward declarations of global variables */
extern int mcolor;
extern int rainbow;
extern int update;

/* Process pipe commands */
void process_pipe_command(char *cmd) {
    if (!cmd || strlen(cmd) == 0) 
        return;
    
    /* Trim newline character if present */
    size_t len = strlen(cmd);
    if (cmd[len-1] == '\n')
        cmd[len-1] = '\0';
        
    /* Parse command with format: command=value */
    char *value = strchr(cmd, '=');
    if (!value) {
        fprintf(stderr, "Invalid command format: %s\n", cmd);
        return;
    }
    
    *value = '\0'; /* Split string at = */
    value++;       /* Move to value part */
    
    pthread_mutex_lock(&pipe_mutex);
    
    /* Handle color command */
    if (strcmp(cmd, "color") == 0) {
        if (strcmp(value, "green") == 0) {
            mcolor = COLOR_GREEN;
            rainbow = 0;
        } else if (strcmp(value, "red") == 0) {
            mcolor = COLOR_RED;
            rainbow = 0;
        } else if (strcmp(value, "blue") == 0) {
            mcolor = COLOR_BLUE;
            rainbow = 0;
        } else if (strcmp(value, "white") == 0) {
            mcolor = COLOR_WHITE;
            rainbow = 0;
        } else if (strcmp(value, "yellow") == 0) {
            mcolor = COLOR_YELLOW;
            rainbow = 0;
        } else if (strcmp(value, "cyan") == 0) {
            mcolor = COLOR_CYAN;
            rainbow = 0;
        } else if (strcmp(value, "magenta") == 0) {
            mcolor = COLOR_MAGENTA;
            rainbow = 0;
        } else if (strcmp(value, "black") == 0) {
            mcolor = COLOR_BLACK;
            rainbow = 0;
        } else if (strcmp(value, "rainbow") == 0) {
            rainbow = 1;
        }
    }
    /* Handle speed command */
    else if (strcmp(cmd, "speed") == 0) {
        int speed = atoi(value);
        if (speed >= 0 && speed <= 10) {
            update = speed;
        }
    }
    /* Handle pause command */
    else if (strcmp(cmd, "pause") == 0) {
        if (strcmp(value, "on") == 0 || strcmp(value, "1") == 0 || strcmp(value, "true") == 0) {
            pause = 1;
        } else if (strcmp(value, "off") == 0 || strcmp(value, "0") == 0 || strcmp(value, "false") == 0) {
            pause = 0;
        } else {
            pause = !pause; /* Toggle pause if value isn't recognized */
        }
    }
    /* Handle bold command */
    else if (strcmp(cmd, "bold") == 0) {
        if (strcmp(value, "off") == 0 || strcmp(value, "0") == 0) {
            bold = 0;
        } else if (strcmp(value, "on") == 0 || strcmp(value, "1") == 0) {
            bold = 1;
        } else if (strcmp(value, "all") == 0 || strcmp(value, "2") == 0) {
            bold = 2;
        }
    }
    /* Handle exit command */
    else if (strcmp(cmd, "exit") == 0) {
        if (lock != 1 && (strcmp(value, "force") == 0 || strcmp(value, "1") == 0 || strcmp(value, "true") == 0)) {
            /* Set signal to exit, which will be handled in the main loop */
            pthread_mutex_unlock(&pipe_mutex);
            finish();
            return;
        }
    }
    
    pthread_mutex_unlock(&pipe_mutex);
}

/* Thread function to monitor pipe */
void *pipe_monitor_thread(void *arg) {
    char buffer[PIPE_BUF_SIZE];
    struct pollfd pfd;
    int ret;
    int reconnect_attempts = 0;
    const int max_reconnect_attempts = 5;
    const int reconnect_delay_ms = 500;
    
    /* Set up polling structure */
    pfd.fd = pipe_fd;
    pfd.events = POLLIN;
    
    while (1) {
        /* Check for thread cancellation */
        pthread_testcancel();
        
        /* Wait for data with a timeout */
        ret = poll(&pfd, 1, 1000); /* 1 second timeout */
        
        if (ret < 0) {
            /* Error occurred */
            if (errno != EINTR) {
                perror("poll");
                
                /* Try to recover from poll error by reopening pipe */
                if (++reconnect_attempts <= max_reconnect_attempts) {
                    fprintf(stderr, "Attempting to recover pipe connection (%d/%d)...\n", 
                            reconnect_attempts, max_reconnect_attempts);
                    
                    /* Close and reopen pipe */
                    if (pipe_fd >= 0) {
                        close(pipe_fd);
                    }
                    
                    napms(reconnect_delay_ms); /* Short delay before reconnect attempt */
                    
                    pipe_fd = open(pipe_path, O_RDONLY | O_NONBLOCK);
                    if (pipe_fd < 0) {
                        perror("Failed to reopen pipe during recovery");
                        continue;
                    }
                    
                    /* Update poll structure with new file descriptor */
                    pfd.fd = pipe_fd;
                    fprintf(stderr, "Pipe connection recovered\n");
                    reconnect_attempts = 0; /* Reset counter on successful reconnect */
                } else {
                    fprintf(stderr, "Failed to recover pipe connection after %d attempts\n", 
                            max_reconnect_attempts);
                    break;
                }
            }
        } else if (ret > 0) {
            /* Data is available */
            if (pfd.revents & POLLIN) {
                ssize_t bytes_read = read(pipe_fd, buffer, PIPE_BUF_SIZE - 1);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    process_pipe_command(buffer);
                    reconnect_attempts = 0; /* Reset counter on successful read */
                } else if (bytes_read == 0) {
                    /* Pipe closed, reopen it */
                    fprintf(stderr, "Pipe closed, reopening...\n");
                    close(pipe_fd);
                    
                    napms(reconnect_delay_ms); /* Short delay before reconnect attempt */
                    
                    pipe_fd = open(pipe_path, O_RDONLY | O_NONBLOCK);
                    if (pipe_fd < 0) {
                        perror("Failed to reopen pipe");
                        if (++reconnect_attempts > max_reconnect_attempts) {
                            fprintf(stderr, "Maximum reconnect attempts reached\n");
                            break;
                        }
                        continue;
                    }
                    pfd.fd = pipe_fd;
                    fprintf(stderr, "Pipe reopened successfully\n");
                    reconnect_attempts = 0; /* Reset counter on successful reconnect */
                } else {
                    /* Read error */
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("read");
                        if (++reconnect_attempts > max_reconnect_attempts) {
                            break;
                        }
                    }
                }
            } else if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
                /* Handle pipe errors */
                fprintf(stderr, "Pipe error detected, attempting to reopen...\n");
                close(pipe_fd);
                
                napms(reconnect_delay_ms); /* Short delay before reconnect attempt */
                
                pipe_fd = open(pipe_path, O_RDONLY | O_NONBLOCK);
                if (pipe_fd < 0) {
                    perror("Failed to reopen pipe after error");
                    if (++reconnect_attempts > max_reconnect_attempts) {
                        fprintf(stderr, "Maximum reconnect attempts reached\n");
                        break;
                    }
                    continue;
                }
                pfd.fd = pipe_fd;
                fprintf(stderr, "Pipe reopened successfully after error\n");
                reconnect_attempts = 0; /* Reset counter on successful reconnect */
            }
        }
    }
    
    fprintf(stderr, "Pipe monitoring thread exiting\n");
    return NULL;
}

/* Create and initialize named pipe */
int setup_pipe(const char *path) {
    /* Create the pipe if it doesn't exist */
    if (mkfifo(path, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo");
        return 0;
    }
    
    /* Open the pipe for reading, non-blocking */
    pipe_fd = open(path, O_RDONLY | O_NONBLOCK);
    if (pipe_fd < 0) {
        perror("open pipe");
        unlink(path);
        return 0;
    }
    
    /* Initialize mutex with recursive attribute to allow nested locking */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&pipe_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    
    /* Set thread attributes for better cleanup */
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
    
    /* Start the monitoring thread */
    if (pthread_create(&pipe_thread, &thread_attr, pipe_monitor_thread, NULL) != 0) {
        perror("pthread_create");
        pthread_attr_destroy(&thread_attr);
        pthread_mutex_destroy(&pipe_mutex);
        close(pipe_fd);
        unlink(path);
        return 0;
    }
    
    pthread_attr_destroy(&thread_attr);
    
    pipe_path = strdup(path);
    fprintf(stderr, "Named pipe created at: %s\n", path);
    fprintf(stderr, "Send commands with: echo \"command=value\" > %s\n", path);
    fprintf(stderr, "Available commands:\n");
    fprintf(stderr, "  color=green|red|blue|white|yellow|cyan|magenta|black|rainbow\n");
    fprintf(stderr, "    - Change the matrix color or enable rainbow mode\n");
    fprintf(stderr, "  speed=0-10\n");
    fprintf(stderr, "    - Set the animation speed (0=fastest, 10=slowest)\n");
    fprintf(stderr, "  pause=on|off|1|0|true|false\n");
    fprintf(stderr, "    - Pause or resume the animation\n");
    fprintf(stderr, "  bold=on|off|all|0|1|2\n");
    fprintf(stderr, "    - Control character boldness (0=off, 1=some, 2=all)\n");
    fprintf(stderr, "  exit=force|1|true\n");
    fprintf(stderr, "    - Exit cmatrix (only works if not in lock mode)\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  echo \"color=red\" > %s\n", path);
    fprintf(stderr, "  echo \"speed=3\" > %s\n", path);
    fprintf(stderr, "  echo \"pause=on\" > %s\n", path);
    
    return 1;
}

void usage(void) {
    printf(" Usage: cmatrix -[abBcfhlsmVxk] [-u delay] [-C color] [-t tty] [-M message] [-P pipe_path]\n");
    printf(" -a: Asynchronous scroll\n");
    printf(" -b: Bold characters on\n");
    printf(" -B: All bold characters (overrides -b)\n");
    printf(" -c: Use Japanese characters as seen in the original matrix. Requires appropriate fonts\n");
    printf(" -f: Force the linux $TERM type to be on\n");
    printf(" -l: Linux mode (uses matrix console font)\n");
    printf(" -L: Lock mode (can be closed from another terminal)\n");
    printf(" -o: Use old-style scrolling\n");
    printf(" -h: Print usage and exit\n");
    printf(" -n: No bold characters (overrides -b and -B, default)\n");
    printf(" -s: \"Screensaver\" mode, exits on first keystroke\n");
    printf(" -x: X window mode, use if your xterm is using mtx.pcf\n");
    printf(" -V: Print version information and exit\n");
    printf(" -M [message]: Prints your message in the center of the screen. Overrides -L's default message.\n");
    printf(" -u delay (0 - 10, default 4): Screen update delay\n");
    printf(" -C [color]: Use this color for matrix (default green)\n");
    printf(" -r: rainbow mode\n");
    printf(" -m: lambda mode\n");
    printf(" -k: Characters change while scrolling. (Works without -o opt.)\n");
    printf(" -t [tty]: Set tty to use\n");
    printf(" -P [pipe_path]: Create a named pipe for runtime control of color and speed\n");
}

void version(void) {
    printf(" CMatrix version %s (compiled %s, %s)\n",
        VERSION, __TIME__, __DATE__);
    printf("Email: abishekvashok@gmail.com\n");
    printf("Web: https://github.com/abishekvashok/cmatrix\n");
}


/* nmalloc from nano by Big Gaute */
void *nmalloc(size_t howmuch) {
    void *r;

    if (!(r = malloc(howmuch))) {
        c_die("CMatrix: malloc: out of memory!");
    }

    return r;
}

/* Initialize the global variables */
void var_init() {
    int i, j;

    if (matrix != NULL) {
        free(matrix[0]);
        free(matrix);
    }

    matrix = nmalloc(sizeof(cmatrix *) * (LINES + 1));
    matrix[0] = nmalloc(sizeof(cmatrix) * (LINES + 1) * COLS);
    for (i = 1; i <= LINES; i++) {
        matrix[i] = matrix[i - 1] + COLS;
    }

    if (length != NULL) {
        free(length);
    }
    length = nmalloc(COLS * sizeof(int));

    if (spaces != NULL) {
        free(spaces);
    }
    spaces = nmalloc(COLS* sizeof(int));

    if (updates != NULL) {
        free(updates);
    }
    updates = nmalloc(COLS * sizeof(int));

    /* Make the matrix */
    for (i = 0; i <= LINES; i++) {
        for (j = 0; j <= COLS - 1; j += 2) {
            matrix[i][j].val = -1;
        }
    }

    for (j = 0; j <= COLS - 1; j += 2) {
        /* Set up spaces[] array of how many spaces to skip */
        spaces[j] = (int) rand() % LINES + 1;

        /* And length of the stream */
        length[j] = (int) rand() % (LINES - 3) + 3;

        /* Sentinel value for creation of new objects */
        matrix[1][j].val = ' ';

        /* And set updates[] array for update speed. */
        updates[j] = (int) rand() % 3 + 1;
    }

}

#ifndef _WIN32
void sighandler(int s) {
    signal_status = s;
}
#endif

void resize_screen(void) {
#ifdef _WIN32
    BOOL result;
    HANDLE hStdHandle;
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

    hStdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdHandle == INVALID_HANDLE_VALUE)
        return;
#else
    char *tty;
    int fd = 0;
    int result = 0;
    struct winsize win;

    tty = ttyname(0);
    if (!tty) {
        return;
#endif
#ifdef _WIN32
    result = GetConsoleScreenBufferInfo(hStdHandle, &csbiInfo);
    if (!result)
        return;
    LINES = csbiInfo.dwSize.Y;
    COLS = csbiInfo.dwSize.X;
#else
    }
    fd = open(tty, O_RDWR);
    if (fd == -1) {
        return;
    }
    result = ioctl(fd, TIOCGWINSZ, &win);
    if (result == -1) {
        return;
    }

    COLS = win.ws_col;
    LINES = win.ws_row;
#endif

    if (LINES < 10) {
        LINES = 10;
    }
    if (COLS < 10) {
        COLS = 10;
    }

#ifdef HAVE_RESIZETERM
    resizeterm(LINES, COLS);
#ifdef HAVE_WRESIZE
    if (wresize(stdscr, LINES, COLS) == ERR) {
        c_die("Cannot resize window!");
    }
#endif /* HAVE_WRESIZE */
#endif /* HAVE_RESIZETERM */

    var_init();
    /* Do these because width may have changed... */
    clear();
    refresh();
}

/* Shared variables used by pipe control */
int update = 4;
int mcolor = COLOR_GREEN;
int rainbow = 0;

int main(int argc, char *argv[]) {
    int i, y, z, optchr, keypress;
    int j = 0;
    int count = 0;
    int screensaver = 0;
    int asynch = 0;
    int bold = 0;
    int force = 0;
    int firstcoldone = 0;
    int oldstyle = 0;
    int random = 0;
    int highnum = 0;
    int lambda = 0;
    int randnum = 0;
    int randmin = 0;
    int pause = 0;
    int classic = 0;
    int changes = 0;
    char *msg = "";
    char *tty = NULL;

    srand((unsigned) time(NULL));
    setlocale(LC_ALL, "");

    /* Many thanks to morph- (morph@jmss.com) for this getopt patch */
    opterr = 0;
    while ((optchr = getopt(argc, argv, "abBcfhlLnrosmxkVM:u:C:t:P:")) != EOF) {
        switch (optchr) {
        case 's':
            screensaver = 1;
            break;
        case 'a':
            asynch = 1;
            break;
        case 'b':
            if (bold != 2) {
                bold = 1;
            }
            break;
        case 'B':
            bold = 2;
            break;
        case 'C':
            if (!strcasecmp(optarg, "green")) {
                mcolor = COLOR_GREEN;
            } else if (!strcasecmp(optarg, "red")) {
                mcolor = COLOR_RED;
            } else if (!strcasecmp(optarg, "blue")) {
                mcolor = COLOR_BLUE;
            } else if (!strcasecmp(optarg, "white")) {
                mcolor = COLOR_WHITE;
            } else if (!strcasecmp(optarg, "yellow")) {
                mcolor = COLOR_YELLOW;
            } else if (!strcasecmp(optarg, "cyan")) {
                mcolor = COLOR_CYAN;
            } else if (!strcasecmp(optarg, "magenta")) {
                mcolor = COLOR_MAGENTA;
            } else if (!strcasecmp(optarg, "black")) {
                mcolor = COLOR_BLACK;
            } else {
                c_die(" Invalid color selection\n Valid "
                       "colors are green, red, blue, "
                       "white, yellow, cyan, magenta " "and black.\n");
            }
            break;
        case 'c':
            classic = 1;
            break;
        case 'f':
            force = 1;
            break;
        case 'l':
            console = 1;
            break;
        case 'L':
            lock = 1;
            //if -M was used earlier, don't override it
            if (0 == strncmp(msg, "", 1)) {
                msg = "Computer locked.";
            }
            break;
        case 'M':
            msg = strdup(optarg);
            break;
        case 'n':
            bold = -1;
            break;
        case 'h':
        case '?':
            usage();
            exit(0);
        case 'o':
            oldstyle = 1;
            break;
        case 'u':
            update = atoi(optarg);
            break;
        case 'x':
            xwindow = 1;
            break;
        case 'V':
            version();
            exit(0);
        case 'r':
            rainbow = 1;
            break;
        case 'm':
            lambda = 1;
            break;
        case 'k':
            changes = 1;
            break;
        case 't':
            tty = optarg;
            break;
        case 'P':
            use_pipe = 1;
            if (!setup_pipe(optarg)) {
                c_die("Failed to set up pipe at %s\n", optarg);
            }
            break;
        }
    }

    /* Clear TERM variable on Windows */
#ifdef _WIN32
    _putenv_s("TERM", "");
#endif

    if (force && strcmp("linux", getenv("TERM"))) {
#ifdef _WIN32
        SetEnvironmentVariableW(L"TERM", L"linux");
#else
        /* setenv is much more safe to use than putenv */
        setenv("TERM", "linux", 1);
#endif
    }
    if (tty) {
        FILE *ftty = fopen(tty, "r+");
        if (!ftty) {
            fprintf(stderr, "cmatrix: error: '%s' couldn't be opened: %s.\n",
                    tty, strerror(errno));
            exit(EXIT_FAILURE);
        }
        SCREEN *ttyscr;
        ttyscr = newterm(NULL, ftty, ftty);
        if (ttyscr == NULL)
            exit(EXIT_FAILURE);
        set_term(ttyscr);
    } else
        initscr();
    savetty();
    nonl();
#ifdef _WIN32
    raw();
#else
    cbreak();
#endif
    noecho();
    timeout(0);
    leaveok(stdscr, TRUE);
    curs_set(0);
#ifndef _WIN32
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGWINCH, sighandler);
    signal(SIGTSTP, sighandler);
#endif

if (console) {
#ifdef HAVE_CONSOLECHARS
        if (va_system("consolechars -f matrix") != 0) {
            c_die
                (" There was an error running consolechars. Please make sure the\n"
                 " consolechars program is in your $PATH.  Try running \"consolechars -f matrix\" by hand.\n");
        }
#elif defined(HAVE_SETFONT)
        if (va_system("setfont matrix") != 0) {
            c_die
                (" There was an error running setfont. Please make sure the\n"
                 " setfont program is in your $PATH.  Try running \"setfont matrix\" by hand.\n");
        }
#else
        c_die(" Unable to use both \"setfont\" and \"consolechars\".\n");
#endif
}
    if (has_colors()) {
        start_color();
        /* Add in colors, if available */
#ifdef HAVE_USE_DEFAULT_COLORS
        if (use_default_colors() != ERR) {
            init_pair(COLOR_BLACK, -1, -1);
            init_pair(COLOR_GREEN, COLOR_GREEN, -1);
            init_pair(COLOR_WHITE, COLOR_WHITE, -1);
            init_pair(COLOR_RED, COLOR_RED, -1);
            init_pair(COLOR_CYAN, COLOR_CYAN, -1);
            init_pair(COLOR_MAGENTA, COLOR_MAGENTA, -1);
            init_pair(COLOR_BLUE, COLOR_BLUE, -1);
            init_pair(COLOR_YELLOW, COLOR_YELLOW, -1);
        } else {
#else
        { /* Hack to deal the after effects of else in HAVE_USE_DEFAULT_COLOURS */
#endif
            init_pair(COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
            init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
            init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
            init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
            init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
            init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
            init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
            init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
        }
    }

    /* Set up values for random number generation */
    if (classic) {
        /* Half-width kana characters. In the movie they are y-axis flipped, and
         * they appear alongside latin characters and numerals, but this is the
         * closest we can do with a standard unicode set and a single number
         * range */
        randmin = 0xff66;
        highnum = 0xff9d;
    } else if (console || xwindow) {
        randmin = 166;
        highnum = 217;
    } else {
        randmin = 33;
        highnum = 123;
    }
    randnum = highnum - randmin;

    var_init();

    while (1) {
#ifndef _WIN32
        /* Check for signals */
        if (signal_status == SIGINT || signal_status == SIGQUIT) {
            if (lock != 1)
                finish();
            /* exits */
        }
        if (signal_status == SIGWINCH) {
            resize_screen();
            signal_status = 0;
        }

        if (signal_status == SIGTSTP) {
            if (lock != 1)
                    finish();
        }
#endif

        count++;
        if (count > 4) {
            count = 1;
        }

        if ((keypress = wgetch(stdscr)) != ERR) {
            if (screensaver == 1) {
#ifdef USE_TIOCSTI
                char *str = malloc(0);
                size_t str_len = 0;
                do {
                    str = realloc(str, str_len + 1);
                    str[str_len++] = keypress;
                } while ((keypress = wgetch(stdscr)) != ERR);
                size_t i;
                for (i = 0; i < str_len; i++)
                    ioctl(STDIN_FILENO, TIOCSTI, (char*)(str + i));
                free(str);
#endif
                finish();
            } else {
                switch (keypress) {
#ifdef _WIN32
                case 3: /* Ctrl-C. Fall through */
#endif
                case 'q':
                    if (lock != 1)
                        finish();
                    break;
                case 'a':
                    asynch = 1 - asynch;
                    break;
                case 'b':
                    bold = 1;
                    break;
                case 'B':
                    bold = 2;
                    break;
                case 'L':
                    lock = 1;
                    break;
                case 'n':
                    bold = 0;
                    break;
                case '0': /* Fall through */
                case '1': /* Fall through */
                case '2': /* Fall through */
                case '3': /* Fall through */
                case '4': /* Fall through */
                case '5': /* Fall through */
                case '6': /* Fall through */
                case '7': /* Fall through */
                case '8': /* Fall through */
                case '9':
                    update = keypress - 48;
                    break;
                case '!':
                    mcolor = COLOR_RED;
                    rainbow = 0;
                    break;
                case '@':
                    mcolor = COLOR_GREEN;
                    rainbow = 0;
                    break;
                case '#':
                    mcolor = COLOR_YELLOW;
                    rainbow = 0;
                    break;
                case '$':
                    mcolor = COLOR_BLUE;
                    rainbow = 0;
                    break;
                case '%':
                    mcolor = COLOR_MAGENTA;
                    rainbow = 0;
                    break;
                case 'r':
                     rainbow = 1;
                     break;
                case 'm':
                     lambda = !lambda;
                     break;
                case '^':
                    mcolor = COLOR_CYAN;
                    rainbow = 0;
                    break;
                case '&':
                    mcolor = COLOR_WHITE;
                    rainbow = 0;
                    break;
                case 'p':
                case 'P':
                    pause = (pause == 0)?1:0;
                    break;

                }
            }
        }
        for (j = 0; j <= COLS - 1; j += 2) {
            if ((count > updates[j] || asynch == 0) && pause == 0) {

                /* I don't like old-style scrolling, yuck */
                if (oldstyle) {
                    for (i = LINES - 1; i >= 1; i--) {
                        matrix[i][j].val = matrix[i - 1][j].val;
                    }
                    random = (int) rand() % (randnum + 8) + randmin;

                    if (matrix[1][j].val == 0) {
                        matrix[0][j].val = 1;
                    } else if (matrix[1][j].val == ' '
                             || matrix[1][j].val == -1) {
                        if (spaces[j] > 0) {
                            matrix[0][j].val = ' ';
                            spaces[j]--;
                        } else {

                            /* Random number to determine whether head of next column
                               of chars has a white 'head' on it. */

                            if (((int) rand() % 3) == 1) {
                                matrix[0][j].val = 0;
                            } else {
                                matrix[0][j].val = (int) rand() % randnum + randmin;
                            }
                            spaces[j] = (int) rand() % LINES + 1;
                        }
                    } else if (random > highnum && matrix[1][j].val != 1) {
                        matrix[0][j].val = ' ';
                    } else {
                        matrix[0][j].val = (int) rand() % randnum + randmin;
                    }

                } else { /* New style scrolling (default) */
                    if (matrix[0][j].val == -1 && matrix[1][j].val == ' '
                        && spaces[j] > 0) {
                        spaces[j]--;
                    } else if (matrix[0][j].val == -1
                        && matrix[1][j].val == ' ') {
                        length[j] = (int) rand() % (LINES - 3) + 3;
                        matrix[0][j].val = (int) rand() % randnum + randmin;

                        spaces[j] = (int) rand() % LINES + 1;
                    }
                    i = 0;
                    y = 0;
                    firstcoldone = 0;
                    while (i <= LINES) {

                        /* Skip over spaces */
                        while (i <= LINES && (matrix[i][j].val == ' ' ||
                               matrix[i][j].val == -1)) {
                            i++;
                        }

                        if (i > LINES) {
                            break;
                        }

                        /* Go to the head of this column */
                        z = i;
                        y = 0;
                        while (i <= LINES && (matrix[i][j].val != ' ' &&
                               matrix[i][j].val != -1)) {
                            matrix[i][j].is_head = false;
                            if (changes) {
                                if (rand() % 8 == 0)
                                    matrix[i][j].val = (int) rand() % randnum + randmin;
                            }
                            i++;
                            y++;
                        }

                        if (i > LINES) {
                            matrix[z][j].val = ' ';
                            continue;
                        }

                        matrix[i][j].val = (int) rand() % randnum + randmin;
                        matrix[i][j].is_head = true;

                        /* If we're at the top of the column and it's reached its
                           full length (about to start moving down), we do this
                           to get it moving.  This is also how we keep segments not
                           already growing from growing accidentally =>
                         */
                        if (y > length[j] || firstcoldone) {
                            matrix[z][j].val = ' ';
                            matrix[0][j].val = -1;
                        }
                        firstcoldone = 1;
                        i++;
                    }
                }
            }
            /* A simple hack */
            if (!oldstyle) {
                y = 1;
                z = LINES;
            } else {
                y = 0;
                z = LINES - 1;
            }
            for (i = y; i <= z; i++) {
                move(i - y, j);

                if (matrix[i][j].val == 0 || (matrix[i][j].is_head && !rainbow)) {
                    if (console || xwindow) {
                        attron(A_ALTCHARSET);
                    }
                    attron(COLOR_PAIR(COLOR_WHITE));
                    if (bold) {
                        attron(A_BOLD);
                    }
                    if (matrix[i][j].val == 0) {
                        if (console || xwindow) {
                            addch(183);
                        } else {
                            addch('&');
                        }
                    } else if (matrix[i][j].val == -1) {
                        addch(' ');
                    } else {
                        addch(matrix[i][j].val);
                    }

                    attroff(COLOR_PAIR(COLOR_WHITE));
                    if (bold) {
                        attroff(A_BOLD);
                    }
                    if (console || xwindow) {
                        attroff(A_ALTCHARSET);
                    }
                } else {
                    if (rainbow) {
                        int randomColor = rand() % 6;

                        switch (randomColor) {
                            case 0:
                                mcolor = COLOR_GREEN;
                                break;
                            case 1:
                                mcolor = COLOR_BLUE;
                                break;
                            case 2:
                                mcolor = COLOR_BLACK;
                                break;
                            case 3:
                                mcolor = COLOR_YELLOW;
                                break;
                            case 4:
                                mcolor = COLOR_CYAN;
                                break;
                            case 5:
                                mcolor = COLOR_MAGENTA;
                                break;
                       }
                    }
                    attron(COLOR_PAIR(mcolor));
                    if (matrix[i][j].val == 1) {
                        if (bold) {
                            attron(A_BOLD);
                        }
                        addch('|');
                        if (bold) {
                            attroff(A_BOLD);
                        }
                    } else {
                        if (console || xwindow) {
                            attron(A_ALTCHARSET);
                        }
                        if (bold == 2 ||
                            (bold == 1 && matrix[i][j].val % 2 == 0)) {
                            attron(A_BOLD);
                        }
                        if (matrix[i][j].val == -1) {
                            addch(' ');
                        } else if (lambda && matrix[i][j].val != ' ') {
                            addstr("λ");
                        } else {
                            /* addch doesn't seem to work with unicode
                             * characters and there was no direct equivalent.
                             * So, construct a c-style string with the character
                             * and print that.
                             */
                            wchar_t char_array[2];
                            char_array[0] = matrix[i][j].val;
                            char_array[1] = 0;
                            /* Use add_wch when addwstr is unavailable */
                            #ifdef HAVE_NCURSESW_H
                            addwstr(char_array);
                            #else
                            {
                                char buffer[MB_CUR_MAX + 1];
                                wcstombs(buffer, char_array, MB_CUR_MAX);
                                addstr(buffer);
                            }
                            #endif
                        }
                        if (bold == 2 ||
                            (bold == 1 && matrix[i][j].val % 2 == 0)) {
                            attroff(A_BOLD);
                        }
                        if (console || xwindow) {
                            attroff(A_ALTCHARSET);
                        }
                    }
                    attroff(COLOR_PAIR(mcolor));
                }
            }
        }

        //check if -M and/or -L was used
        if (msg[0] != '\0') {
            //Add our message to the screen
            int msg_x = LINES/2;
            int msg_y = COLS/2 - strlen(msg)/2;
            int i = 0;

            //Add space before message
            move(msg_x-1, msg_y-2);
            for (i = 0; i < strlen(msg)+4; i++)
                addch(' ');

            //Write message
            move(msg_x, msg_y-2);
            addch(' ');
            addch(' ');
            addstr(msg);
            addch(' ');
            addch(' ');

            //Add space after message
            move(msg_x+1, msg_y-2);
            for (i = 0; i < strlen(msg)+4; i++)
                addch(' ');
        }

        /* Sleep according to update speed */

        napms(update * 10);
    }
    finish();
}
