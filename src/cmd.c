// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>

#include "cmd.h"
#include "utils.h"


#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
    /* TODO: Execute cd or pwd. */
	if (dir == NULL) {
		fprintf(stderr, "cd: missing argument\n");
		return false;
	}

	char *path = get_word(dir);

	if (strcmp(path, "pwd") == 0) {
		char cwd[PATH_MAX];

		if (getcwd(cwd, sizeof(cwd)) != NULL) {
			fprintf(stdout, "%s\n", cwd);
			free(path);
			return true;
		}
		perror("getcwd");
		free(path);
		return false;
	}
	if (chdir(path) != 0) {
		perror("chdir");
		free(path);
		return false;
	}
	free(path);
	return true;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */
	exit(0);

	// return 0; /* TODO: Replace with actual exit code. */
}

void perform_redirection(simple_command_t *s)
{
	if (s->in != NULL) {
		char *s_in = get_word(s->in);

		int fd_in = open(s_in, O_RDONLY);

		if (fd_in == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(fd_in, STDIN_FILENO);
		close(fd_in);
		free(s_in);
	}

	int fd_out = -1, fd_err = -1;

	if (s->out != NULL) {
		int flags = O_WRONLY | O_CREAT;

		if (s->io_flags & IO_OUT_APPEND)
			flags |= O_APPEND;
		else
			flags |= O_TRUNC;

		char *s_out = get_word(s->out);

		fd_out = open(s_out, flags, 0666);
		if (fd_out == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(fd_out, STDOUT_FILENO);
		free(s_out);
	}

	if (s->err != NULL) {
		int flags = O_WRONLY | O_CREAT;

		if (s->io_flags & IO_ERR_APPEND)
			flags |= O_APPEND;
		else
			flags |= O_TRUNC;


		char *s_out = get_word(s->out);

		char *s_err = get_word(s->err);


		if (s->out != NULL && strcmp(s_out, s_err) == 0) {
			dup2(fd_out, STDERR_FILENO);
			free(s_out);
			free(s_err);
		} else {
			fd_err = open(s_err, flags, 0666);
			if (fd_err == -1) {
				perror("open");
				exit(EXIT_FAILURE);
			}
			dup2(fd_err, STDERR_FILENO);
			free(s_err);
		}
	}

	if (fd_out != -1 && fd_out != fd_err)
		close(fd_out);


	if (fd_err != -1 && fd_err != fd_out)
		close(fd_err);
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	char *s_verb = get_word(s->verb);

	if (strchr(s_verb, '=')) {
		int state = putenv(s_verb);

		free(s_verb);
		return state;
	}

	/* TODO: Sanity checks. */
	if (s == NULL || s->verb == NULL) {
		fprintf(stderr, "Error: Invalid command\n");
		return SHELL_EXIT;
	}

	/* TODO: If builtin command, execute the command. */

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	if (strcmp(s->verb->string, "cd") == 0) {
		perform_redirection(s);

		int cd_result = shell_cd(s->params) ? 0 : 1;

		freopen("/dev/null", "a", stdout);
		freopen("/dev/null", "a", stderr);

		return cd_result;
	} else if (strcmp(s->verb->string, "exit") == 0 || strcmp(s->verb->string, "quit") == 0) {
		return shell_exit();
	}

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */
	pid_t pid = fork();

	if (pid == -1) {
		perror("fork");
		return SHELL_EXIT;
	}

	if (pid == 0) {
		perform_redirection(s);

		int params_number = 0;
		char **params = get_argv(s, &params_number);

		char *s_verb = get_word(s->verb);

		execvp(s_verb, params);

		fprintf(stderr, "Execution failed for '%s'\n", s_verb);
		free(s_verb);
		exit(EXIT_FAILURE);
	} else {
		int status;

		pid_t wait_result = waitpid(pid, &status, 0);

		if (wait_result == -1)
			perror("waitpid");

		if (WIFEXITED(status)) {
			return WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			fprintf(stderr, "Child process terminated by signal %d\n", WTERMSIG(status));
			return SHELL_EXIT;
			} else if (WIFSTOPPED(status)) {
				fprintf(stderr, "Child process stopped by signal %d\n", WSTOPSIG(status));
				return SHELL_EXIT;
			}

		return SHELL_EXIT;
	}

	return 0; /* TODO: Replace with actual exit status. */
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */

	pid_t pid1 = fork();

	if (pid1 == -1) {
		perror("fork");
		return false;
	}

	if (pid1 == 0) {
		int result1 = parse_command(cmd1, level + 1, father);

		exit(result1);
	}

	pid_t pid2 = fork();

	if (pid2 == -1) {
		perror("fork");
		return false;
	}

	if (pid2 == 0) {
		int result2 = parse_command(cmd2, level + 1, father);

		exit(result2);
	}

	int status1, status2;

	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);

	return WEXITSTATUS(status2);

	// return true; /* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */

	int pipe_fd[2];

	if (pipe(pipe_fd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	pid_t pid1 = fork();

	if (pid1 == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid1 == 0) {
		close(pipe_fd[0]);

		dup2(pipe_fd[1], STDOUT_FILENO);
		close(pipe_fd[1]);

		int status1 = parse_command(cmd1, level + 1, father);

		exit(status1);
	}

	pid_t pid2 = fork();

	if (pid2 == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid2 == 0) {
		close(pipe_fd[1]);

		dup2(pipe_fd[0], STDIN_FILENO);
		close(pipe_fd[0]);

		int status2 = parse_command(cmd2, level + 1, father);

		exit(status2);
	}

	close(pipe_fd[0]);
	close(pipe_fd[1]);

	int status1, status2;

	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);

	return WEXITSTATUS(status2);

	// return true; /* TODO: Replace with actual exit status. */
}

/**
 * Execute commands sequentially.
 */
static int run_sequential(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	parse_command(cmd1, level + 1, father);

	int status2 = parse_command(cmd2, level + 1, father);

	return status2;
}

/**
 * Execute commands conditionally based on the result of the first command.
 */
static int run_conditional(command_t *cmd1, command_t *cmd2, int level,
		command_t *father, int condition)
{
	int status1 = parse_command(cmd1, level + 1, father);


	if ((condition == OP_CONDITIONAL_ZERO && status1 == 0) ||
		(condition == OP_CONDITIONAL_NZERO && status1 != 0)) {
		int status2 = parse_command(cmd2, level + 1, father);
		return status2;
	}

	return status1;
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		return parse_simple(c->scmd, level, father);
		// return 0; /* TODO: Replace with actual exit code of command. */
	}

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		return run_sequential(c->cmd1, c->cmd2, level, father);

	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		return run_in_parallel(c->cmd1, c->cmd2, level, father);

	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		return run_conditional(c->cmd1, c->cmd2, level, father, OP_CONDITIONAL_NZERO);

	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		return run_conditional(c->cmd1, c->cmd2, level, father, OP_CONDITIONAL_ZERO);

	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		return run_on_pipe(c->cmd1, c->cmd2, level, father);

	default:
		return SHELL_EXIT;
	}

	return 0; /* TODO: Replace with actual exit code of command. */
}
