/**
 * @file smallsh.c
 * @author Kevin Sekuj (sekujk@oregonstate.edu)
 * @brief A small shell written in C. Smallsh features three built-in commands:
 *        status, cd, and exit. Besides these three commands, smallsh allows a
 *        user to enter in and execute any other commands. Smallsh allows these
 *        processes to be run in the foreground or background.
 * @version 0.1
 * @date 2022-05-09
 *
 * @copyright Copyright (c) 2022
 *
 */

/* IMPORTS */
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>
/* */

/* CONSTANTS */
#define MAX_ARGS 512
#define MAX_LENGTH 2049
char EXPAND = '$';
char *COMMENT = "#";
char *EXECUTE_BG = "&\0";
char *CHANGE_DIR = "cd";
char *STATUS = "status";
char *BLANK_LINE = "\n";
char *BLANK_SPACE = " ";
char *EXIT_SHELL = "exit";
char *REDIRECT_STDIN = "<";
char *REDIRECT_STDOUT = ">";
/* */

/* GLOBAL STATE */
bool shellActive;
bool foregroundOnly = false;

int currentProcess = 0; // global array index for inserting new processes
int lastProcessStatus = 0;
pid_t processes[MAX_LENGTH]; // keep track of background processes in a global array
/* */

/* STRUCTS */
/**
 * @brief Command struct for encapsulating single commands. Captures raw CLI
 *        input into a line array, which is parsed into an array of pointers,
 *        args. Pointers are also initialized to keep track of input and output
 *        filenames, if provided, as well as a series of bools which determine
 *        the command's properties.
 */
typedef struct
{
  pid_t pid;

  char *args[MAX_ARGS];
  char line[MAX_LENGTH];
  char *inputFile;
  char *outputFile;

  bool redirectStdin;
  bool redirectStdout;
  bool background;
} Command;
/* */

/* FUNCTION PROTOTYPES */
void status();
void tokenize();
void exitSmallsh();
void executeProgram();
void parseCommandLine();
void foregroundOnlyMode();
void checkProcessStatus();
void checkBackgroundProcess();
void killBackgroundProcesses();
void checkVariableExpansion(Command *cmd);

int cd(char *path);
int mapArguments(Command *cmd);
int parseArguments(Command *cmd);
/* */

int main(void)
{
  // Initialize an empty SIGINT_action struct and register a signal ignore
  // constant instead of a handler function, and block catchable signals
  // adapted from Exploration 5: Signal Handling API
  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = SIG_IGN;
  sigfillset(&SIGINT_action.sa_mask);
  sigaction(SIGINT, &SIGINT_action, NULL);

  // Initialize smallsh program loop
  shellActive = true;
  parseCommandLine();

  return 0;
}

/**
 * @brief Small Shell's program loop. Smallsh will first check the status of
 *        processes running in the background, in order to post updates regarding
 *        their exit status. The program then reads raw input from the command line,
 *        parses it, stores it into a command struct, and then passes the parsed
 *        user arguments to the mapArguments function which determines whether
 *        the user passed in a built-in command or not.
 */
void parseCommandLine()
{
  while (shellActive)
  {

    checkProcessStatus();
    fflush(stdout);
    printf(": ");

    // Setup signal handler from SIGTSTP to enter/exit foerground only mode
    // using a RESTART flag to restart any interrupted system/library calls
    // adapted from Exploration 5: Signal Handling API
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = foregroundOnlyMode;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Initialize a command struct and read raw CLI input into it
    Command cmd;
    memset(&cmd, 0, sizeof(Command));
    fgets(cmd.line, MAX_LENGTH, stdin);
    strtok(cmd.line, "\n");

    // Ignore comments/blank-lines
    if (strncmp((const char *)cmd.line, COMMENT, 1) == 0)
    {
      continue;
    }

    // Check input for expansion characters and expand them into the pid of Small Shell
    checkVariableExpansion(&cmd);

    // Tokenize raw CLI input into an array of pointers, delineating arguments
    // by spaces. Pass them to a handler function which determines whether to
    // execute a built-in or non-built-in function
    tokenize(&cmd);
    if (mapArguments(&cmd))
    {
      shellActive = false;
      exitSmallsh();
    }
  }
}

/**
 * @brief Checks the status of processes running in the background before aech
 *        iteration of the shell loop. Processes that have exited will have
 *        their exit status printed to the terminal, as well as processes that
 *        have been terminated by signal, along with the signal that terminated
 *        them.
 */
void checkProcessStatus()
{
  // iterate through the global processes array, reporting the status of child
  // processes that have exited or been terminated. Adapted from Module 4:
  // Process API - Monitoring Child Processes
  for (int i = 0; i < MAX_LENGTH; i++)
  {
    int status;
    pid_t pid = processes[i];
    if (waitpid(pid, &status, WNOHANG) > 0)
    {
      if (WIFEXITED(status))
      {
        printf("Background pid %d is done: exit value %d\n", pid, status);
      }
      else if (WIFSIGNALED(status))
      {
        printf("Background pid %d is done: terminated by signal %d\n", pid, WTERMSIG(status));
      }
      fflush(stdout);
    }
  }
}

/**
 * @brief Scans raw cli input for possible variable expansion. Expansion characters
 *        ($) must be found in groups of two. If so, the array indices corresponding
 *        to the expansion characters are modified to a format string, and inserted
 *        into a buffer using sprintf. Finally, the buffer is copied back into
 *        the command struct's line property.
 *
 * @param cmd - command struct with property line, containing raw cli input from user
 */
void checkVariableExpansion(Command *cmd)
{
  pid_t pid = getpid();    // get pid of small shell
  char buffer[MAX_LENGTH]; // initialize a buffer to hold expanded input

  // zero out buffer to get rid of junk data
  for (int i = 0; i < MAX_LENGTH; i++)
  {
    buffer[i] = 0;
  }

  // iterate through cli input while still reading chars in i and i+1
  for (int i = 0; cmd->line[i] && cmd->line[i + 1]; i++)
  {
    // found a pair of expansion characters ($$) - expand using a format string
    // and sprintf and copy back into command struct
    if (cmd->line[i] == EXPAND && cmd->line[i + 1] == EXPAND)
    {
      cmd->line[i] = '%';
      cmd->line[i + 1] = 'd';
      sprintf(buffer, cmd->line, pid);
      strcpy(cmd->line, buffer);
    }
  }
}

/**
 * @brief Tokenizes raw CLI input and stores individual arguments into an array
 *        of pointers within the command struct. Arguments are separated by
 *        whitespace.
 *
 * @param cmd Command struct containing raw cli input
 */
void tokenize(Command *cmd)
{
  int i = 0;
  char *token = strtok(cmd->line, " ");
  while (token != NULL)
  {
    cmd->args[i] = token;
    token = strtok(NULL, " ");
    i++;
  }
}

/**
 * @brief Maps parsed user input into commands to be executed by small shell.
 *        Null, blank space, or blank lines will lead to a reprompt. CD will
 *        call the change directory function. Exit will simply return 1, which
 *        is handled in the caller function. Finally, any non built-in functions
 *        will be passed to the executeProgram function which handles that case.
 *
 * @param cmd command struct containing parsed input
 * @return int 1 - in case of exit, else 0
 */
int mapArguments(Command *cmd)
{
  // pull the first argument in parsed input to decide what execution path to take
  char *arg = cmd->args[0];

  // if argument is NULL, or a blank space/line, print a newline and reprompt
  if (!arg || strcmp(arg, BLANK_SPACE) == 0 || (strcmp(arg, BLANK_LINE) == 0))
  {
    printf("\n");
    fflush(stdout);
    return 0;
  }

  // return 1 when an exit command is read - caller function handles this case
  if (strcmp(arg, EXIT_SHELL) == 0)
  {
    return 1;
  }

  if (strcmp(arg, CHANGE_DIR) == 0)
  {
    cd(cmd->args[1]);
    return 0;
  }

  if (strcmp(arg, STATUS) == 0)
  {
    status();
    return 0;
  }
  // if no built-in commands are detected, pass the command struct along to the
  // non built-in command handler
  executeProgram(cmd);
  return 0;
}

//=============================================================================
// Built-in commands
//=============================================================================

/**
 * @brief Exits small shell by using the SIGKILL signal on any running background
 *        processess before exiting the parent shell.
 *
 *        Reference: stackoverflow.com/questions/6501522/how-to-kill-a-child-process-by-the-parent-process
 */
void exitSmallsh()
{
  for (int i = 0; i < MAX_LENGTH; i++)
  {
    // if process is not null
    processes[i] ? kill(processes[i], SIGKILL) : 0;
  }
  exit(0);
}

/**
 * @brief Change directory built-in command. Accepts a directory path and attempts
 *        to change directory to that path, if it exists. Otherwise, the function
 *        swaps to the directory specified inside the HOME environment variable.
 * @param path directory to change to
 * @return int - result from attempting to change directory
 */
int cd(char *path)
{
  if (path == NULL)
  {
    path = getenv("HOME");
  }

  int result = chdir(path);
  if (result < 0)
  {
    perror("Error changing directory");
    return -1;
  }
  return 0;
}

/**
 * @brief Status built-in which prints the status of the last process run by
 *        smallsh.
 *
 */
void status()
{
  printf("exit value %d\n", lastProcessStatus);
  fflush(stdout);
}

//=============================================================================
// Handling execution for all other commands
//=============================================================================

/**
 * @brief Function which executes non built-in programs for Small Shell. Arguments
 *        are extracted from the command struct's argument's property and are parsed
 *        for certain flags such as input or output redirection, or running the program
 *        as a background process. The program is run with the execvp function, and
 *        upon success, forks into a child process.
 *
 * @param cmd
 */
void executeProgram(Command *cmd)
{

  // pass cmd->args to a helper function which returns the length of the
  // arguments and detects output/input redirection and background status.
  int cmdLength = parseArguments(cmd);

  // use arguments length to initialize an argv array to pass to execvp containing
  // program arguments
  char *argv[cmdLength + 1];
  argv[0] = cmd->args[0];
  argv[cmdLength] = NULL;

  // insert parsed arguments from command struct into argv array
  for (int i = 1; i < cmdLength; i++)
  {
    argv[i] = cmd->args[i];
  }

  // reference: heavily adapted from Module 4: Process API - Executing a New Program
  int processStatus;
  pid_t pid = fork();

  // exit on fork failure
  if (pid == -1)
  {
    perror("Error forking");
    lastProcessStatus = 1;
    exit(1);
  }

  // child
  else if (pid == 0)
  {
    // if not a background command, setup default action for SIGINT signal
    if (!cmd->background)
    {
      struct sigaction SIGINT_action = {0};
      SIGINT_action.sa_handler = SIG_DFL;
      sigfillset(&SIGINT_action.sa_mask);
      SIGINT_action.sa_flags = 0;
      sigaction(SIGINT, &SIGINT_action, NULL);
    }

    // initialize infile and outfile pointers
    int infp;
    int outfp;

    // if redirect stdin/out evaluates to true, or the process is to be run
    // in the background, then open the output/input files specified in
    // the command struct, which are /dev/null by default
    if (cmd->redirectStdout || cmd->background)
    {
      outfp = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (outfp == -1)
      {
        fprintf(stderr, "%s: no such file or directory\n", cmd->outputFile);
        exit(1);
      }
      // redirect stdout
      dup2(outfp, 1);
    }

    // same as above, but for redirect stdin
    if (cmd->redirectStdin || cmd->background)
    {
      infp = open(cmd->inputFile, O_RDONLY);
      if (infp == -1)
      {
        fprintf(stderr, "%s: no such file or directory\n", cmd->inputFile);
        exit(1);
      }
      // redirect stdin
      dup2(infp, 0);
    }

    // execute program and close any open file pointers
    if (execvp(argv[0], argv) == -1)
    {
      perror("Error executing command");
      exit(1);
    }
    if (cmd->redirectStdout)
    {
      close(outfp);
    }
    if (cmd->redirectStdin)
    {
      close(infp);
    }
  }

  // parent
  else
  {
    if (cmd->background)
    {
      // print pid of background process and add it to global processes array
      printf("background pid is %d\n", pid);
      fflush(stdout);

      processes[currentProcess] = pid;
      currentProcess++;
    }
    else
    {
      // if process was terminated by a signal, print out the termination status
      waitpid(pid, &processStatus, 0);
      if (WIFSIGNALED(processStatus))
      {
        printf("terminated by signal %d\n", WTERMSIG(processStatus));
        fflush(stdout);
      }
      // update the process exit status
      if (!processStatus)
      {
        lastProcessStatus = 0; // process exited normally
      }
      else
      {
        lastProcessStatus = 1; // process exited abnormally
      }
    }
  }
}

/**
 * @brief Helper function for processing CLI arguments. This function returns
 *        the length of the arguments, and also looks for for a background process
 *        character, as well as  input or output redirection characters, along with
 *         the specified filename.
 *
 * @param cmd command struct containing CLI arguments and other properties
 * @return int - length of arguments
 */
int parseArguments(Command *cmd)
{
  int i = 0;
  int length = 0;

  // scan the arguments array for a & to indicate a background process
  checkBackgroundProcess(cmd);

  // if a redirect output character or redirected input character are found,
  // then update the command struct's properties as well as the filenames
  // specified by the user. then, NULL out the arguments as they aren't needed
  // to execute the command
  while (cmd->args[i] != NULL)
  {
    if (strcmp(cmd->args[i], REDIRECT_STDOUT) == 0)
    {
      cmd->redirectStdout = true;
      cmd->args[i] = NULL;

      cmd->outputFile = cmd->args[i + 1];
      cmd->args[i + 1] = NULL;

      i += 2;
      continue;
    }

    if (strcmp(cmd->args[i], REDIRECT_STDIN) == 0)
    {
      cmd->redirectStdin = true;
      cmd->args[i] = NULL;

      cmd->inputFile = cmd->args[i + 1];
      cmd->args[i + 1] = NULL;

      i += 2;
      continue;
    }

    i++;
    length++;
  }

  return length;
}

/**
 * @brief Scans the parsed arguments array looking for an ampersand & to indicate
 *        that the process should be run in the background. If so, the element is
 *        NULLed out, and input/output point to /dev/null by default.
 *
 *        If foreground only mode is on, the & character is ignored.
 *
 * @param cmd command struct
 */
void checkBackgroundProcess(Command *cmd)
{
  int i = 0;
  while (cmd->args[i] != NULL)
  {
    // last character in args
    if (strcmp(cmd->args[i], EXECUTE_BG) == 0 && !cmd->args[i + 1])
    {
      if (!foregroundOnly)
      {
        cmd->background = true;
        cmd->inputFile = "/dev/null";
        cmd->outputFile = "/dev/null";
      }
      cmd->args[i] = NULL;
      break;
    }
    i++;
  }
}

/**
 * @brief Handles entry or exit from foreground only mode, changing the program's
 *        foreground state and printing its status.
 */
void foregroundOnlyMode()
{
  char *message;
  int count;
  if (!foregroundOnly)
  {
    message = "Entering Foreground only mode\n";
    count = 30;
    foregroundOnly = true;
  }
  else
  {
    message = "Exiting Foreground only mode\n";
    count = 29;
    foregroundOnly = false;
  }
  write(STDOUT_FILENO, message, count);
  fflush(stdout);
}
