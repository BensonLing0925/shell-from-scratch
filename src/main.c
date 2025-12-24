#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define DEFAULT_STR_ALLOC 64
#define MAX_STR_ALLOC 1024

static void chomp_newline(char *s) {
  if (!s) return;
  size_t n = strlen(s);
  if (n && s[n-1] == '\n') s[n-1] = '\0';
}

// ssize_t getline(char **lineptr, size_t *n, FILE *stream); POSIX getline signature
// lineptr: actual memory to store string
// n: buffer max capacity
// stream: valid stream

// return read character
ssize_t my_getline(char **lineptr, size_t *n, FILE *stream) {

  if (!lineptr || !n || !stream) {
    errno = EINVAL;
    return -1;
  }

  if (*lineptr == NULL || *n == 0) {
    *lineptr = (char*)malloc(DEFAULT_STR_ALLOC);
    *n = DEFAULT_STR_ALLOC;
  }

  int c = 0;
  size_t num_char = 0;
  while ((c = fgetc(stream)) != EOF) {
    if (num_char + 1 >= *n) {
      size_t new_cap = (*n <= (MAX_STR_ALLOC / 2)) ? (*n * 2) : MAX_STR_ALLOC;
      if (new_cap <= *n) {
        errno = ENOMEM;
        return -1;
      }
      char* temp = realloc(*lineptr, new_cap);
      if (temp == NULL) {
        errno = ENOMEM;
        return -1;
      }
      (*lineptr) = temp;
      *n = new_cap;
    }

    (*lineptr)[num_char++] = (char) c;

    if (c == '\n') {
      break;
    }
  }

  if (num_char == 0 && c == EOF) {
    return -1;
  }

  (*lineptr)[num_char] = '\0';
  return num_char;

}

char* readCommand(FILE* stream) {
  char* cmd = NULL;
  size_t cap = 0;

  ssize_t r = my_getline(&cmd, &cap, stream);
  if (r == -1) {   // EOF or error
    free(cmd);
    return NULL;
  }
  return cmd; // caller frees
}

int isValidCommand(char* cmd) {
  int rt = 0;
  return rt;
}

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  // TODO: Uncomment the code below to pass the first stage
  printf("$ ");

  char* cmd = readCommand(stdin);
  if (!cmd) return 0;
  chomp_newline(cmd);
  if (!isValidCommand(cmd)) {
    printf("%s: command not found", cmd);
    free(cmd);
  }

  return 0;
}
