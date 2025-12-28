#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>

#include "arena.h"

#define DEFAULT_STR_ALLOC 64
#define MAX_STR_ALLOC 1024

#define NUM_COMMAND 5

#define DEFAULT_NUM_ARG 8

struct Cmd {
  size_t argc;
  char** argv;
  size_t cap;
};

struct Cmd* createCmd();
void freeCmd(struct Cmd* cmd);
void initCmd(struct Cmd* cmd);
void initCmdArgv(struct Cmd* cmd, struct arena* a);

void initCmd(struct Cmd* cmd) {
  cmd->argc = 0;
  cmd->argv = NULL;
  cmd->cap = DEFAULT_NUM_ARG;
}

void initCmdArgv(struct Cmd* cmd, struct arena* a) {
    cmd->argv = arena_alloc(a, DEFAULT_NUM_ARG * sizeof(char*));
    if (cmd->argv == NULL) {
        perror("Not enough memory at initCmdArgv");
        return;
    }

    for ( size_t i = 0 ; i < cmd->cap ; ++i ) {
        cmd->argv[i] = NULL;
    }
}

int cmdArgvGrow(struct Cmd* cmd, struct arena* a) {
    size_t old = cmd->cap;
    size_t new = old * 2;

    char** v = arena_alloc(a, sizeof(char*) * new);
    if (!v) return -1;

    memcpy(v, cmd->argv, sizeof(char*) * old);
    memset(v + old, 0, sizeof(char*) * (new - old));

    cmd->argv = v;
    cmd->cap  = new;
    return 0;
}

struct Cmd* createCmd() {
  struct Cmd* cmd = (struct Cmd*)malloc(sizeof(struct Cmd));
  initCmd(cmd);
  return cmd;
}

const char built_in_commands[NUM_COMMAND][DEFAULT_STR_ALLOC] = {
  "exit",
  "echo",
  "type",
  "pwd",
  "cd"
};

/* string manipulation utilities */

int isDelimiter(char ch) {
    if (ch == ' ')
        return ch;
    return 0;
}

static int push_token(struct Cmd* cmd, struct arena* a,
                      const char* token, int token_len) {
    if (token_len <= 0) return 0;

    // TODO: grow argv (選 A 或 B)
    if (cmd->argc + 1 >= cmd->cap) {
        // 如果 cmd->argv 也改 arena：cmdArgvGrow_arena(cmd,a)
        if (cmdArgvGrow(cmd, a) < 0) return -1;
    }

    char* mem = arena_alloc(a, (size_t)token_len + 1);
    if (!mem) return -1;

    memcpy(mem, token, (size_t)token_len);
    mem[token_len] = '\0';

    cmd->argv[cmd->argc++] = mem;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

static int emit_token(struct Cmd* cmd, struct arena* a, 
                  char* token, int* token_index) {

    if (*token_index == 0) return 0;
    if (push_token(cmd, a, token, *token_index) < 0) // push_token should add '\0'
        return -1;
    *token_index = 0;
    token[0] = '\0';
    return 0;

}

ssize_t tokenize(char *str, struct Cmd *cmd, struct arena *a) {
    int i = 0;
    char token[DEFAULT_STR_ALLOC];
    int n = 0;

    token[0] = '\0';
    while (str[i] != '\0') {
        char c = str[i];
        // 1) whitespace: end token
        if (c == ' ') {
            if (emit_token(cmd, a, token, &n) < 0) return -1;
            // skip all spaces
            do { i++; } while (str[i] == ' ');
            continue;
        }
        // 2) single quote: read until closing quote (allow concatenated quotes)
        if (c == '\'') {
            i++; // consume opening quote
            for (;;) {
                // read quoted content
                while (str[i] != '\0' && str[i] != '\'') {
                    if (n + 1 >= DEFAULT_STR_ALLOC) { errno = EOVERFLOW; return -1; }
                    token[n++] = str[i++];
                }
                if (str[i] == '\0') { errno = EINVAL; return -1; } // unmatched quote
                i++; // consume closing quote
                // if next char is another quote, concatenate
                if (str[i] == '\'') { i++; continue; }
                // quoted segment ended
                break;
            }
            // 注意：不要在這裡 emit，因為你允許 quote 後面緊接著普通字元黏在同一個 token
            continue;
        }
        // 3) double quote: read until closing quote (allow concatenated quotes)
        if (c == '\"') {
            i++; // consume opening quote
            for (;;) {
                // read quoted content
                while (str[i] != '\0' && str[i] != '\"') {
                    if (n + 1 >= DEFAULT_STR_ALLOC) { errno = EOVERFLOW; return -1; }
                    token[n++] = str[i++];
                }
                if (str[i] == '\0') { errno = EINVAL; return -1; } // unmatched quote
                i++; // consume closing quote
                // if next char is another quote, concatenate
                if (str[i] == '\"') { i++; continue; }
                // quoted segment ended
                break;
            }
            // 注意：不要在這裡 emit，因為你允許 quote 後面緊接著普通字元黏在同一個 token
            continue;
        }
        // 4) normal char
        if (n + 1 >= DEFAULT_STR_ALLOC) { errno = EOVERFLOW; return -1; }
        token[n++] = str[i++];
    }
    // end of input: emit last token
    if (emit_token(cmd, a, token, &n) < 0) return -1;
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
      char* temp = realloc(*lineptr, new_cap * sizeof(char*));
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

  // ssize_t r = my_getline(&cmd, &cap, stream);
  ssize_t r = getline(&cmd, &cap, stream);
  if (r == -1) {   // EOF or error
    free(cmd);
    return NULL;
  }
  return cmd; // caller frees
}

/* command verifications */

int isValidCommand(char* cmd) {
  int rt = 0;
  for ( int i = 0 ; i < NUM_COMMAND ; i++ ) {
    if (strcmp(cmd, built_in_commands[i]) == 0) {
      rt = 1;
    }
  }
  return rt;
}

int isBuiltinCommand(char* cmd) {
  int rt = 0;
  for ( int i = 0 ; i < NUM_COMMAND ; i++ ) {
    if (strcmp(cmd, built_in_commands[i]) == 0) {
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

int isType(char* cmd) {
  if (strcmp(cmd, "type") == 0) {
    return 1;
  }
  return 0;
}

int isPwd(char* cmd) {
  if (strcmp(cmd, "pwd") == 0) {
    return 1;
  }
  return 0;
}

int isCd(char* cmd) {
  if (strcmp(cmd, "cd") == 0) {
    return 1;
  }
  return 0;
}

/* critical functions */
char* find_path_executable(char* path, char* type_arg) {
    char* save = NULL;
    char* rt = NULL;
    for ( char* dir = strtok_r(path, ":", &save) ;
          dir ;
          dir = strtok_r(NULL, ":", &save)) {

        char full_path[PATH_MAX] = {0};
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, type_arg);
        
        if (access(full_path, X_OK) == 0) {
            rt = strdup(full_path); 
            free(path);
            return rt;
        }
    }
    free(path);
    return NULL;
}

int run_process(struct Cmd* cmd) {
    pid_t pid = fork();
    // child
    if (pid == 0) {
        execvp(cmd->argv[0], cmd->argv);
        perror("execvp");
        _exit(127);
    }
    // parent
    else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
    else {
        perror("fork");
        return -1;
    }
    return 0;
}

int changeDir(char* destDir) {

    if (strcmp(destDir, "~") == 0) {
        destDir = getenv("HOME");
        destDir = strdup(destDir);
    }

    if (chdir(destDir) == -1) {
        // perror("cd");
        return -1;
    }
    return 0;
}

/* main */

int main() {
  // Flush after every printf
  setbuf(stdout, NULL);
  struct arena a;
  arena_init(&a);

  // TODO: Uncomment the code below to pass the first stage
  while (1) {
    printf("$ ");

    char* cmd_str = readCommand(stdin);
    if (!cmd_str) return 0;
    chomp_newline(cmd_str);
    struct Cmd cmd;
    initCmd(&cmd);
    initCmdArgv(&cmd, &a);
    // tokenize(cmd_str, " ", cmd);
    tokenize(cmd_str, &cmd, &a);
    char* exe_name = cmd.argv[0];

    /* get PATH */
    char* path = getenv("PATH");
    if (!path) {
        errno = EINVAL;
        return -1;
    }

    if (!isBuiltinCommand(exe_name)) {
        // check if PATH can find that executable
        char* path_copy = strdup(path);
        if (!path_copy) {
            errno = ENOMEM;
            return -1;
        }
        char* full_path = find_path_executable(path_copy, exe_name);
        if (full_path) {
            run_process(&cmd); 
            free(full_path);
        }
        else {
            printf("%s: command not found\n", cmd_str);
        }
    }
    else {
      if (isExit(exe_name)) {
        free(cmd_str);
        break;
      }
      else if (isEcho(exe_name)) {
        for ( size_t num_arg = 1 ; num_arg < cmd.argc-1 ; num_arg++ ) {
          printf("%s ", cmd.argv[num_arg]);
        }
        printf("%s\n", cmd.argv[cmd.argc-1]);
      }
      else if (isType(exe_name)) {
        char* type_arg = cmd.argv[1];
        if (cmd.argc >= 2) {
          if (isBuiltinCommand(type_arg)) {
            printf("%s is a shell builtin\n", type_arg);
          }
          else {
            // try to parse PATH and find executable
            char* path = getenv("PATH");
            if (!path) {
                errno = EINVAL;
                return -1;
            }
            char* path_copy = strdup(path);
            if (!path_copy) {
                errno = ENOMEM;
                return -1;
            }
            char* full_path = find_path_executable(path_copy, type_arg);
            if (full_path) {
                printf("%s is %s\n", type_arg, full_path);
            }
            else if (!isValidCommand(type_arg)) {
                printf("%s: not found\n", type_arg);
            }
          }
        }
      }
      else if (isPwd(exe_name)) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd");
            return -1;
        }
        printf("%s\n", cwd);
      }
      else if (isCd(exe_name)) {
        // currently suppose argc == 2
        if (changeDir(cmd.argv[1]) == -1) {
            printf("cd: %s: No such file or directory\n", cmd.argv[1]);
        }
      }
    }
    arena_reset(&a);
    free(cmd_str);
  }
  arena_destroy(&a);

  return 0;
}

