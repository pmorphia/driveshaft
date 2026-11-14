/* Driveshaft's terminal access: raw mode, one key at a time, and the window width.
 * Imported from console.fw with `import c term`. */
#include <stdio.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>

static DWORD saved_out = 0;
static int raw_on = 0;

int ds_tty(void) { return _isatty(0) && _isatty(1); }

int ds_raw(int on) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (on && !raw_on) {
        GetConsoleMode(out, &saved_out);
        SetConsoleMode(out, saved_out | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */);
        SetConsoleOutputCP(65001);
        raw_on = 1;
    } else if (!on && raw_on) {
        raw_on = 0;
    }
    return 1;
}

int ds_cols(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return info.srWindow.Right - info.srWindow.Left + 1;
    return 80;
}

void ds_signals(void) { signal(SIGINT, SIG_IGN); }

int ds_run(const char *script) { return system(script); }

const char *ds_key(void) {
    static char buf[8];
    wint_t c = _getwch();
    if (c == 0 || c == 0xE0) {
        switch (_getwch()) {
        case 72: return "UP";
        case 80: return "DOWN";
        case 75: return "LEFT";
        case 77: return "RIGHT";
        case 71: return "HOME";
        case 79: return "END";
        case 83: return "DEL";
        case 115: return "WLEFT";
        case 116: return "WRIGHT";
        default: return "?";
        }
    }
    if (c == 13) return "ENTER";
    if (c == 9) return "TAB";
    if (c == 8) return "BACK";
    if (c == 27) return "ESC";
    if (c < 32) { buf[0] = '^'; buf[1] = (char)(c + 64); buf[2] = 0; return buf; }
    {
        wchar_t w[2] = {(wchar_t)c, 0};
        int n = WideCharToMultiByte(CP_UTF8, 0, w, 1, buf, 7, NULL, NULL);
        buf[n > 0 ? n : 0] = 0;
    }
    return buf;
}

#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <errno.h>

static struct termios saved;
static int raw_on = 0;

int ds_tty(void) { return isatty(0) && isatty(1); }

int ds_raw(int on) {
    if (on && !raw_on) {
        struct termios t;
        if (tcgetattr(0, &saved)) return 0;
        t = saved;
        t.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
        t.c_iflag &= ~(IXON | ICRNL);
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
        tcsetattr(0, TCSADRAIN, &t);
        raw_on = 1;
    } else if (!on && raw_on) {
        tcsetattr(0, TCSADRAIN, &saved);
        raw_on = 0;
    }
    return 1;
}

int ds_cols(void) {
    struct winsize w;
    if (ioctl(1, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) return w.ws_col;
    return 80;
}

static void ds_nothing(int signal_number) { (void)signal_number; }

/* Ctrl-C while a program runs should stop the program, not the console. A handler (unlike
 * SIG_IGN) is reset in the programs the console starts. */
void ds_signals(void) {
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_handler = ds_nothing;
    action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action, NULL);
}

/* Runs a shell script as the terminal's foreground job, in its own process group, the way a
 * shell runs a command: Ctrl-C reaches the script's programs and not the console (nor the
 * `flywheel` that started the console). Returns the exit status, 128 + N for signal N. */
int ds_run(const char *script) {
    int status = 0;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        setpgid(0, 0);
        if (isatty(0)) {
            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(0, getpid());
            signal(SIGTTOU, SIG_DFL);
        }
        signal(SIGINT, SIG_DFL);
        execl("/bin/sh", "sh", "-c", script, (char *)NULL);
        _exit(127);
    }
    setpgid(pid, pid);
    for (;;) {
        pid_t done = waitpid(pid, &status, WUNTRACED);
        if (done < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (WIFSTOPPED(status)) {
            kill(-pid, SIGKILL);
            continue;
        }
        break;
    }
    if (isatty(0)) {
        void (*old)(int) = signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(0, getpgrp());
        signal(SIGTTOU, old);
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
}

static int ds_byte(int wait_ms) {
    unsigned char c;
    if (wait_ms >= 0) {
        fd_set set;
        struct timeval tv;
        FD_ZERO(&set);
        FD_SET(0, &set);
        tv.tv_sec = 0;
        tv.tv_usec = wait_ms * 1000;
        if (select(1, &set, NULL, NULL, &tv) <= 0) return -1;
    }
    if (read(0, &c, 1) != 1) return -2;
    return c;
}

/* The next key: a character (UTF-8), or a name: ENTER, TAB, BACK, ESC, UP, DOWN, LEFT, RIGHT,
 * HOME, END, DEL, WLEFT, WRIGHT (word movement), ^A..^Z, EOF. */
const char *ds_key(void) {
    static char buf[16];
    int c = ds_byte(-1);
    if (c == -2) return "EOF";
    if (c == 13 || c == 10) return "ENTER";
    if (c == 9) return "TAB";
    if (c == 127 || c == 8) return "BACK";
    if (c == 27) {
        char seq[16];
        int n = 0, next = ds_byte(30);
        if (next < 0) return "ESC";
        if (next == 'b') return "WLEFT";
        if (next == 'f') return "WRIGHT";
        if (next == 127) return "WBACK";
        if (next != '[' && next != 'O') return "?";
        while (n < 15) {
            int b = ds_byte(30);
            if (b < 0) break;
            seq[n++] = (char)b;
            if ((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || b == '~') break;
        }
        seq[n] = 0;
        if (!strcmp(seq, "A")) return "UP";
        if (!strcmp(seq, "B")) return "DOWN";
        if (!strcmp(seq, "C")) return "RIGHT";
        if (!strcmp(seq, "D")) return "LEFT";
        if (!strcmp(seq, "H") || !strcmp(seq, "1~") || !strcmp(seq, "7~")) return "HOME";
        if (!strcmp(seq, "F") || !strcmp(seq, "4~") || !strcmp(seq, "8~")) return "END";
        if (!strcmp(seq, "3~")) return "DEL";
        if (!strcmp(seq, "1;5C") || !strcmp(seq, "1;3C")) return "WRIGHT";
        if (!strcmp(seq, "1;5D") || !strcmp(seq, "1;3D")) return "WLEFT";
        if (!strcmp(seq, "Z")) return "STAB";
        return "?";
    }
    if (c < 32) { buf[0] = '^'; buf[1] = (char)(c + 64); buf[2] = 0; return buf; }
    {
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0, i;
        buf[0] = (char)c;
        for (i = 1; i <= extra; i++) {
            int b = ds_byte(100);
            if (b < 0) { extra = i - 1; break; }
            buf[i] = (char)b;
        }
        buf[extra + 1] = 0;
    }
    return buf;
}
#endif
