# smallsh

<b>smallsh</b> is an interactive shell written in C. It implements a subset of features of well known shells like bash.

## Features

- Provides a prompt for running commands
- Handles blank lines and comments, which are lines beginning with the # character
- Provides expansion for the variable $$
- Execute 3 commands exit, cd, and status via code built into the shell
- Execute other commands by creating new processes using a function from the exec family of functions
- Support input and output redirection
- Support running commands in foreground and background processes
- Implement custom handlers for 2 signals, SIGINT and SIGTSTP

## Sample 

![Sample][smallshexample.png]

## Technologies

- [C]

## Development

This project was developed by [Kevin Sekuj](https://github.com/kevinsekuj) for Oregon State University's CS344 Operating Systems course.

## License

MIT
