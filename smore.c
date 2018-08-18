#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define _LARGEFILE_SOURCE
#include <fcntl.h>
#include <sys/fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#define NEXT 0
#define ERROR -2
#define UP 'u'
#define DOWN 'd'
#define SCREEN 's'
#define ROW 'r'
struct winsize ttysz;
struct termios otty;
struct termios ntty;
struct stat fileStat;
int wsrow, wscol;
void tty_size();
void init_buffer();
char* buffer;
int open_out(char*, int);
int fildes;
int rows;
int cols;
off_t offset;
int parse_command();
int output(char, char, int);
void set_tty();
void reset_tty();
char get_char();
int pow_t(int, int);
int cat_t(char*);
int current_row;
int first_row;
int last_row;
char block_output;
char disable_scroll_up;
int look_screen(int, int*, int, char);
int look_str(int, int*);
void print_error(int, char*, char*);
char* filename;
void clear_line();
void write_t(char*, int);
char prompt_buffer[1024];
int main(int argc, char* argv[]) {
    int i=1;
write(2,"Hello\n",6);
    disable_scroll_up=0;
    if (!isatty(STDIN_FILENO)) {
        argv[0] = "-";
        i=0;
    }
    if (!isatty(STDOUT_FILENO)) {
        for (; i < argc; i++)
            if (cat_t(argv[i]) == ERROR)
                _exit(1);
        _exit(0);
    }
    if ((argc == 1) && (argv[0] != "-")) {
        fprintf(stderr, "Incorrect number of arguments\n");
        _exit(1);
    }

    tty_size();
    set_tty();
    init_buffer();
    for (;i<argc; i++) {

        filename=argv[i];
        if((open_out(argv[i], i) == EOF) && (i<(argc-1))) {
            clear_line();
        }
    }
    clear_line();
    reset_tty();
    free(buffer);
    _exit(0);
}
void tty_size() {
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ttysz) != -1) {
        wsrow=ttysz.ws_row;
        wscol=ttysz.ws_col;
    }
    else {
        print_error(errno, "ioctl", "");
        _exit(1);
    }
}
void init_buffer() {
    buffer=(char*)malloc((wsrow-1)*wscol);
}
int open_out(char* path, int arg_number ) {
    if (!strcmp(path, "-")) {
        if (arg_number == 0) {
            fildes = STDIN_FILENO;
            disable_scroll_up = 1;
        }
        else {
            fprintf(stderr, "Can't read because sdtin is a tty");
            return EOF;
        }
    }
    else {
        if ((fildes = open(path, O_RDONLY | O_LARGEFILE)) != -1) {
            if (stat(path, &fileStat) != 0) {
                print_error(errno, "stat", path);
                return EOF;
            }
            if (!(fileStat.st_mode & S_IFREG) && !(fileStat.st_mode & S_IFIFO)) {
                fprintf(stderr, "%s is not a regular file or fifo\n", path);
                return EOF;
            }
            if (fileStat.st_mode & S_IFIFO)
                disable_scroll_up=1;
        }
        else {
            print_error(errno, "open", path);
            return EOF;
        }
    }
    current_row=1;
    if (output(SCREEN, DOWN, 0) == EOF) return EOF;
    while(1)
        if (parse_command() == EOF) break;
    if (fildes != STDIN_FILENO)
        close(fildes);
    disable_scroll_up=0;
    return EOF;
}

int output(char action, char up_down, int number) {
    int count=0;
    int index=0;
    rows = wsrow - 1;
    cols = 0;
    if (!disable_scroll_up) {
        if (up_down == DOWN) {
            if ((offset = lseek(fildes, 0, SEEK_CUR)) == (off_t) -1) {
                clear_line();
                print_error(errno, "lseek", filename);
                return EOF;
            }
        }
        if (up_down == UP) {
            last_row = current_row - number * rows;
            first_row = last_row - rows;
            if ((first_row < 2) && (last_row <= wsrow)) {
                current_row = 1;
                if ((offset = lseek(fildes, 0, SEEK_SET)) == (off_t) -1) {
                    clear_line();
                    print_error(errno, "lseek", filename);
                    return EOF;
                }
                return output(SCREEN, DOWN, 0);
            }
            if ((offset = lseek(fildes, 0, SEEK_SET)) == (off_t) -1) {
                clear_line();
                print_error(errno, "lseek", filename);
                return EOF;
            }
            current_row = 1;
            while (1) {
                index = 0;
                if (look_screen(rows, &index, first_row, '1') == -2)
                    break;
            }
            return output(SCREEN, DOWN, 0);
        }
    }
    switch (action) {
        case SCREEN :
            count=look_screen(rows, &index, 0, '0');
            break;
        case ROW :
            count=look_str(cols, &index);
            break;
    }
    if (buffer[index-1] == '\n') {
        clear_line();
        write_t(buffer, index);
        write_t("More", 4);

    }
    else {
        clear_line();
        write_t(buffer, index);
        write_t("\n", 1);
        write_t("More", 4);
    }
    if (count) return NEXT;
    return EOF;
}
int parse_command() {
    int i=0, j=0;
    int count=0;
    char ch[1024];
    int number=0;
    for (;;i++) {
        ch[i]=get_char();
        if (ch[i] < '0' || ch[i] > '9') break;
        count++;
    }
    if (ch[i] != ' ' && ch[i] != '\n' && ch[i] != 'b' && ch[i] != 'q') return ERROR;
    if (count == 0) number=1;
    for (j=0; j<i; j++)
        number=number+(((int)(ch[j]-'0'))*pow_t(10, (count-1-j)));
    switch (ch[i]) {
        case ' ' :
            tty_size();
            if (number == 1) {
                if (output(SCREEN, DOWN, 0) == EOF) return EOF;
            }
            else
                while (number--)
                    if (output(ROW , DOWN, 0) == EOF) return EOF;
            break;
        case '\n' :
            tty_size();
            while (number--)
                if (output(ROW, DOWN,  0) == EOF) return EOF;
            break;
        case 'b' :
            tty_size();
            if (disable_scroll_up) break;
            if (output(SCREEN, UP, number) == EOF) return EOF;
            break;
        case 'q' :
            reset_tty();
            clear_line();
            _exit(0);
    }
    return EOF;
}
void set_tty() {
    if (ioctl(STDERR_FILENO, TCGETS, &otty) != -1) {
        ntty = otty;
        ntty.c_lflag &= ~(ICANON | ECHO);
        ntty.c_cc[VMIN] = 1;
        ntty.c_cc[VTIME] = 0;
    }
    else {
        print_error(errno, "ioctl", "");
        _exit(1);
    }
    if (ioctl(STDERR_FILENO, TCSETS, &ntty) == -1) {
        print_error(errno, "ioctl", "");
        _exit(1);
    }
}
void reset_tty() {
    if (ioctl(STDERR_FILENO, TCSETS, &otty) == -1) {
        print_error(errno, "ioctl", "");
        _exit(1);
    }
}
char get_char() {
    char ch;
    int newttyfd=dup(STDERR_FILENO);
    if (read(newttyfd, &ch, 1) == -1) {
        print_error(errno, "read", "");
    }
    close(newttyfd);
    return ch;
}
int pow_t(int a, int b) {
    int i=0, j=a;
    if (b == 0) return 1;
    for (;i<b-1; i++)
        a*=j;
    return a;
}
int cat_t(char* path) {
    int n;
    char* buf = (char*)malloc(1024);
    if (!strcmp(path, "-"))
        fildes=STDIN_FILENO;
    else if ((fildes = open(path, O_RDONLY)) == -1) {
            print_error(errno, "open", path);
            free(buf);
            return ERROR;
        }
    while ((n=read(fildes, buf, 1024)) > 0) {
        write(STDOUT_FILENO, buf, n);
    }
    if (n == -1) {
        print_error(errno, "read", path);
        free(buf);
        return ERROR;
    }
    free(buf);
    return 0;
}
int look_screen(int rows, int* index, int first_row, char skip_mode) {
    int count;
    while (rows) {
        if ((count = read(fildes, &buffer[*index], 1)) > 0) {
            switch (buffer[*index]) {
                case '\n' :
                    cols = 0;
                    rows--;
                    current_row++;
                    break;
                case '\t' :
                    cols += 4 - (cols % 4);
                    break;
                default :
                    cols++;
                    break;
            }

            if (cols >= wscol) {
                cols = 0;
                rows--;
                current_row++;
            }
            if ((current_row == first_row) && (skip_mode == '1'))
                return -2;
            (*index)++;
        }
        else
            break;
    }
    if (count == -1) {
        print_error(errno, "read", filename);
        return EOF;
    }
    return count;
}
int look_str(int cols, int* index) {
    int count=0;
    while (cols < wscol) {
        if ((count = read(fildes, &buffer[*index], 1)) > 0) {
            switch (buffer[*index]) {
                case '\n' :
                    cols = wscol;
                    break;
                case '\t' :
                    cols += 4 - (cols % 4);
                    break;
                default :
                    cols++;
                    break;
            }
            (*index)++;
        }
        else
            break;
    }
    if (count == -1) {
        print_error(errno, "read", filename);
        return EOF;
    }
    current_row++;
    return count;
}
void print_error(int errno_t, char* str, char* filename) {
    if (strcmp(filename, ""))
        fprintf(stderr, "%s: %d ", filename, errno_t);
    else
        fprintf(stderr, "%d ", errno_t);
    perror(str);
}
void clear_line() {
    int i=0;
    for (;i<ttysz.ws_col; i++)
        prompt_buffer[i]=' ';
    write_t("\r", 1);
    write_t(prompt_buffer, ttysz.ws_col);
    write_t("\r", 1);
}
void write_t(char* str, int length) {
    write(STDOUT_FILENO, str, length);
}
