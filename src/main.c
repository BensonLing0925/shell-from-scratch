#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define DEFAULT_STR_ALLOC 64
#define MAX_STR_ALLOC 1024

#define NUM_COMMAND 2

#define DEFAULT_NUM_ARG 8

struct Cmd {
  size_t argc;
  char** argv;
  size_t cap;
};

struct Cmd* createCmd();
void freeCmd(struct Cmd* cmd);
struct Cmd initCmd(struct Cmd* cmd);
void initCmdArgv(struct Cmd* cmd);

struct Cmd initCmd(struct Cmd* cmd) {
  cmd->argc = 0;
  cmd->argv = NULL;
  cmd->cap = DEFAULT_STR_ALLOC;
}

void initCmdArgv(struct Cmd* cmd) {
  cmd->argv = (char**) malloc(sizeof(char*) * DEFAULT_NUM_ARG);
  for ( int i = 0 ; i < DEFAULT_NUM_ARG ; i++ ) {
    cmd->argv[i] = (char*) malloc(sizeof(char) * DEFAULT_STR_ALLOC);
  }
}

struct Cmd* createCmd() {
  struct Cmd* cmd = (struct Cmd*)malloc(sizeof(struct Cmd));
  initCmd(cmd);
  return cmd;
}

// return 0 if success
ssize_t cmdArgvRealloc(struct Cmd* cmd) {
  size_t old_cap = cmd->cap;
  size_t new_cap = old_cap * 2;
  char** temp = realloc(cmd->argv, new_cap * sizeof(char*));
  if (temp == NULL) {
    freeCmd(cmd);
    errno = ENOMEM;
    return -1;
  }
  else {
    cmd->argv = temp;
    for ( int i = old_cap ; i < new_cap ; i++ ) {
      cmd->argv[i] = (char*) malloc(sizeof(char) * DEFAULT_STR_ALLOC);
    }
  }
  return 0;
}

void freeCmd(struct Cmd* cmd) {
  for ( int i = 0 ; i < cmd->argc ; i++ ) {
    free(cmd->argv[i]);
  }
  free(cmd->argv);
}

const char commands[NUM_COMMAND][DEFAULT_STR_ALLOC] = {
  "exit",
  "echo"
};

/* string manipulation utilities */

ssize_t tokenize(char* str, const char* delim, struct Cmd* cmd) {
  char* token = NULL;
  token = strtok(str, delim);
  while (token != NULL) {
    if (cmd->argc + 1 >= cmd->cap) {    // + 1 for null terminator
      ssize_t ret = cmdArgvRealloc(cmd);
      if (ret != 0) {
        freeCmd(cmd);
        errno = ENOMEM;
        return -1;
      }
    }
    strcpy(cmd->argv[cmd->argc++], token);
    token = strtok(NULL, delim);
  }
  return 0;
}

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

/* command varifications */

int isValidCommand(char* cmd) {
  int rt = 0;
  for ( int i = 0 ; i < NUM_COMMAND ; i++ ) {
    if (strcmp(cmd, commands[i]) == 0) {
      rt = 1;
    }
  }
  return rt;
}

int isExit(char* cmd) {
  if (strcmp(cmd, "exit") == 0) {
    return 1;
  }
  return 0;
}

int isEcho(char* cmd) {
  if (strcmp(cmd, "echo") == 0) {
    return 1;
  }
  return 0;
}

/* main */

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  // TODO: Uncomment the code below to pass the first stage
  while (1) {
    printf("$ ");

    char* cmd_str = readCommand(stdin);
    if (!cmd_str) return 0;
    chomp_newline(cmd_str);
    struct Cmd* cmd = createCmd();
    initCmdArgv(cmd);
    tokenize(cmd_str, " ", cmd);
    char* exe_name = cmd->argv[0];
    if (!isValidCommand(exe_name)) {
      printf("%s: command not found\n", cmd_str);
      free(cmd_str);
    }
    else {
      if (isExit(exe_name)) {
        freeCmd(cmd);
        break;
      }
      else if (isEcho(exe_name)) {
        for ( int num_arg = 1 ; num_arg < cmd->argc-1 ; num_arg++ ) {
          printf("%s ", cmd->argv[num_arg]);
        }
        printf("%s\n", cmd->argv[cmd->argc-1]);
      }
    }
    free(cmd_str);
    freeCmd(cmd);
  }

  return 0;
}
