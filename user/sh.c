// Shell.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

int DEBUG = 0;

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);
void runcmd(struct cmd*) __attribute__((noreturn));

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed.\n", ecmd->argv[0]);
    for (int i = 1; i < 10; ++i) {
      if (ecmd->argv[i]) {
        fprintf(2, "\targ %d: %s.\n", i, ecmd->argv[i]);
      } else {
        break;
      }
    }
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int
startautoc(char* s, char* e, int max) {
  // given a partial path [s, e), try to auto complete the full path of the file.
  // the remaining number of characters we can write to e is max.
  // return an int indicating how many characters we wrote to e.
  if (DEBUG) {
    printf("\n\tstartautoc. e-s=%d, max=%d\n", (int)(e - s), max);
  }

  *e = 0;
  int dirfd = open(".", O_RDONLY);
  if (dirfd < 0) {
    return 0;
  }
  struct dirent de;
  struct stat st;
  char filename[128];
  int len = 0;
  while (read(dirfd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) continue;
    int n = strlen(de.name);
    char *scpy = s;  // make a copy of s
    if (max + (int)(e - scpy) < n) {
      // the filename won't fit, proceed to next file

      if (DEBUG) {
        printf("\n\tlooks like filename won't fit, n=%d, max=%d\n", n, max);
      }

      continue;;
    }
    memset(&filename, 0, 128);
    memmove(&filename[0], de.name, n);
    int fd = open(&filename[0], O_RDONLY);
    if (fd < 0) {
      continue;
    }
    
    if (fstat(fd, &st)) {
      close(fd);
      continue;
    }
    if (st.type == T_FILE) {

      if (DEBUG) {
        printf("\tFile %s: %s - |%s|", filename, "Regular file", s);
      }

      int i = 0;
      for (; i < 128 && scpy != e; ++i, ++scpy) {
        if (filename[i] != *scpy) {
          break;
        }
      }
      if (scpy == e) {
        // got one candidate
        if (DEBUG) {
          printf(" - Match\n");
        }
        // if full match write a space to confirm
        char *p = filename[i] ? &filename[i] : " ";
        if (strlen(p) < max) {
          len = strlen(p);
          write(0, p, len);
          memcpy(e, p, len);
          close(fd);
          break;
        }
      } else {
        if (DEBUG) {
          printf(" - Mismatch\n");
        }
      }
    } else {
      if (DEBUG) {
        printf("\tFile %s: %s\n", filename, "Other");
      }
    }
    close(fd);
  }
  close(dirfd);

  return len;
}

char*
lfindspace(char* start, char* p) {
  // starting from p, find the first space on it's left, upto start
  while (p != start) {
    if (*p == ' ') {
      break;
    }
    --p;
  }
  return p;
}

// Basically the replicate of gets() with the extension for auto completion.
// Upon receiving tab, it will try to autocomplete the file name.
// TODO: improve the auto-completion to overcome the current limitation:
// Once tabbed, the previous char's cannot be modified (kernel console limitation)
// This is because backspace are only supported in kernel space, kernel modify its buffer to 
// reflect backspaces, and send to the userspace the final output.
// The solution should be extend the kernel to allow binding some user function when a tab is 
// received. The user function should then grab the current buffer content, generate 
// auto-completion suggestion and return it to the kernel.
// Bash does this using the readline library: 
// https://stackoverflow.com/questions/5570795/how-does-bash-tab-completion-work
char*
autocgets(char *buf, int max)
{
  int i, cc;
  char c;
  // int debug = 1;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if (DEBUG) {
      if (c == '\t') {
        printf("Read tab\n");
      } else if (c == ' ') {
        printf("Read space\n");
      } else if (c == '\n') {
        printf("Read newline\n");
      } else if (c == '\b') {
        printf("Read backspace\n");
      } else {
        printf("Read %c\n", c);
      }
    }
    if(cc < 1)
      break;
    if(c == '\n' || c == '\r') {
      buf[i++] = c;
      break;
    } else if (c == '\t') {

      // what to do when we receive the tab?
      if (DEBUG) {
        printf("starting autoc, buflen: %d. content: |%s|\n", i, buf);
      }
      char *e = &buf[i];
      char *s = lfindspace(&buf[0], &buf[i]);
      if (*s == ' ') ++s;
      if (s != e) {
        int len = startautoc(s, e, max - i);

        if (DEBUG) {
          printf("\nWritten %d character\n", len);
        }

        i += len;
      }

    } else {
      buf[i++] = c;
    }
  }
  buf[i] = '\0';
  return buf;
}

int
getcmd(char *buf, int nbuf)
{
  // if stdin is a file, don't print this
  struct stat s;
  if (fstat(0, &s) == 0) {
    if (DEBUG) {
      printf("fstat.type of stdin: %d\n", s.type);
    }
    if (s.type != T_FILE) {
      write(2, "$ ", 2);
    }
  } else {
    // failed to fstat, it's okay, we switch to old behavior...
    if (DEBUG) {
      fprintf(2, "Failed to do fstat on fd 0\n");
    }
    write(2, "$ ", 2);
  }

  memset(buf, 0, nbuf);
  autocgets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[256];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait(0);
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  // proceed *ps to get the next valid token (up to es). Store the location of the first char in
  // the valid token in *q, and the location of the last char in the token in *eq (e.g., if the 
  // valid token is ">>", then *eq will point to the second ">"). Return the equivalent int value
  // of the token.
  // For symbols, use q and eq arguments to get the start and end of it. 
  // For operators, use the ret to get the operator.
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;  // ret and **q is equal
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';  // ret and *q diverge.
      s++;
    }
    break;
  default:
    // Here we get ourself a symbol. Maybe 'a' means that.
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  // Proceed *ps so that it skips all the whitespaces and reaches the first token.
  // return true if *ps is now a token, false otherwise.
  // A token is neither a whitespace, nor the ones specified in the toks.
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;
  char *info = "parsing: ";
  if (DEBUG) {
    write(1, info, strlen(info));
    write(1, s, strlen(s));
  }

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  // Recursively parse pipe commands.
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
