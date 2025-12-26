#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>

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
struct Cmd initCmd(struct Cmd* cmd);
void initCmdArgv(struct Cmd* cmd);

struct Cmd initCmd(struct Cmd* cmd) {
  cmd->argc = 0;
  cmd->argv = NULL;
  cmd->cap = DEFAULT_NUM_ARG;
}

void initCmdArgv(struct Cmd* cmd) {
  cmd->argv = (char**) malloc(sizeof(char*) * DEFAULT_NUM_ARG);
  for ( int i = 0 ; i < cmd->cap ; ++i ) {
    cmd->argv[i] = NULL;
  }
}

void CmdArgvStrMalloc(char** current_argv) {
    *current_argv = (char*)malloc(DEFAULT_STR_ALLOC);
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
    for ( int i = old_cap ; i < new_cap ; ++i ) {
        cmd->argv[i] = NULL;
    }
    cmd->cap = new_cap;
  }
  return 0;
}

void freeCmd(struct Cmd* cmd) {
  for ( int i = 0 ; i < cmd->argc ; ++i ) {
    free(cmd->argv[i]);
  }
  free(cmd->argv);
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

ssize_t tokenize(char* str, struct Cmd* cmd) {
    int str_index = 0;
    int token_index = 0;
    char ch = str[str_index++];
    char token[DEFAULT_STR_ALLOC] = {0};
    while (ch != '\0') {
        if (ch == ' ') {
            if (cmd->argc + 1 >= cmd->cap) {    // + 1 for null terminator
                ssize_t ret = cmdArgvRealloc(cmd);
                if (ret != 0) {
                    freeCmd(cmd);
                    errno = ENOMEM;
                    return -1;
                }
            }
            token[token_index] = '\0';
            token_index = 0;
            CmdArgvStrMalloc(&cmd->argv[cmd->argc]);
            strcpy(cmd->argv[cmd->argc++], token);
            memset(token, 0, sizeof(token));
            // skip white space
            ch = str[str_index++];
            while (ch == ' ') {
                ch = str[str_index++];
            }
            continue;
        }
        else if (ch == '\'') {
            // str_index++;
            ch = str[str_index];
            if (ch == '\'') {
                str_index++;
                ch = str[str_index++];
                continue;
            }
            again:
                while (ch != '\'') {
                    token[token_index++] = ch;
                    str_index++;
                    ch = str[str_index];
                }

            if (str[str_index+1] == ' ') {
                ch = str[str_index+2];
                while (ch == ' ') {
                    str_index++;
                    ch = str[str_index];
                }
                if (ch == '\'')
                    str_index--;
            }
            if (str[str_index+1] == '\'') {
                str_index += 2;
                ch = str[str_index];
                goto again;
            }
            str_index++;
            token[token_index] = '\0';
            token_index = 0;
        }
        else {
            token[token_index++] = ch;
        }
        ch = str[str_index++];
    }
    CmdArgvStrMalloc(&cmd->argv[cmd->argc]);
    strcpy(cmd->argv[cmd->argc++], token);
}

/*
ssize_t tokenize(char* str, const char* delim, struct Cmd* cmd) {
  char* token = NULL;
  char* save_ptr = NULL;
  token = strtok_r(str, delim, &save_ptr);
  while (token != NULL) {
    if (cmd->argc + 1 >= cmd->cap) {    // + 1 for null terminator
      ssize_t ret = cmdArgvRealloc(cmd);
      if (ret != 0) {
        freeCmd(cmd);
        errno = ENOMEM;
        return -1;
      }
    }
    CmdArgvStrMalloc(&cmd->argv[cmd->argc]);
    strcpy(cmd->argv[cmd->argc++], token);
    token = strtok_r(NULL, delim, &save_ptr);
  }
  return 0;
}
*/

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

  ssize_t r = my_getline(&cmd, &cap, stream);
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
    char** exe_argv = cmd->argv;
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
    // tokenize(cmd_str, " ", cmd);
    tokenize(cmd_str, cmd);
    char* exe_name = cmd->argv[0];

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
            run_process(cmd); 
            free(full_path);
        }
        else {
            printf("%s: command not found\n", cmd_str);
        }
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
      else if (isType(exe_name)) {
        char* type_arg = cmd->argv[1];
        if (cmd->argc >= 2) {
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
        if (changeDir(cmd->argv[1]) == -1) {
            printf("cd: %s: No such file or directory\n", cmd->argv[1]);
        }
      }
    }
    free(cmd_str);
    freeCmd(cmd);
  }

  return 0;
}

