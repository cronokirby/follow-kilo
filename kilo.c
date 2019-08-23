#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k)&0x1F)

struct editorConfig {
  struct termios orig_termios;
};

// This holds our global config settings.
struct editorConfig E;

// This logic is abstracted out, because it's used in a few places.
void editorResetScreen() {
  // Clear entire screen.
  write(STDOUT_FILENO, "\x1B[2J", 4);
  // Place cursor in top left corner.
  write(STDOUT_FILENO, "\x1B[H", 3);
}

// Terminate the program with an error message
void die(const char *error) {
  editorResetScreen();

  perror(error);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetarr");
  }
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  // Prevent carriage returns from being converted into newlines.
  // Prevent Ctrl-Q and Ctrl-S from doing weird things.
  raw.c_iflag &= ~(ICRNL | IXON);
  // Turn off post processing.
  raw.c_oflag &= ~(OPOST);
  // Prevent characters from being output directly to the terminal.
  // Read input byte by byte instead of line by line.
  // Prevent Ctrl-V from doing weird things.
  // Prevent signals from Ctrl-C or Ctrl-Z being sent to us.
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  // Miscellaneous flags.
  raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP);
  raw.c_cflag |= CS8;
  // Set a timeout for reading.
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

// Block until the next character of input arrives.
char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    // This is necessary because cygwin will report timeouts with -1.
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }
  return c;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
  }
}

// Draw a row of tildes on the left side of the screen.
void editorDrawRows() {
  for (int y = 0; y < 24; ++y) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

// Clear the terminal screen.
//
// This should be called in order to have a blank slate before doing other
// things.
void editorRefreshScreen() {

  editorResetScreen();
  editorDrawRows();
  // Move the cursor back to the top left.
  write(STDOUT_FILENO, "\x1B[H", 3);
}

// Dispatch actions based on a given key pressed.
void editorProcessKeypress(char key) {
  switch (key) {
  case CTRL_KEY('q'):
    editorResetScreen();
    exit(0);
    break;
  }
}

int main() {
  enableRawMode();
  for (;;) {
    char key = editorReadKey();
    editorRefreshScreen();
    editorProcessKeypress(key);
  }
  return 0;
}
