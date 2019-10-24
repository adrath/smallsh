/******************************************************************************
* Program: Program 3 - smallsh
* Author: Alexander Drath
* Course: CS344
* Date: 5/18/19
* Last Modified: 5/26/19
* Filename: smallsh.c
* Description: In this assignment you will write your own shell in C, similar
*	to bash. No other languages, including C++, are allowed, though you may
*	use any version of C you like, such as C99. The shell will run command
*	line instructions and return the results similar to other shells you have
*	used, but without many of their fancier features. In this assignment you
*	will write your own shell, called smallsh.  This will work like the bash
*	shell you are used to using, prompting for a command line and running
*	commands, but it will not have many of the special features of the bash
*	shell. Your shell will allow for the redirection of standard input and
*	standard output and it will support both foreground and background
*	processes (controllable by the command line and by receiving signals).
*	Your shell will support three built in commands: exit, cd, and status. It
*	will also support comments, which are lines beginning with the # character.
******************************************************************************/
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>

#define MAX_LENGTH 2048
#define MAX_ARG 513

//Global variable for background signal
int activeBackground = 1;

//Global variable for parent pid
pid_t parentPid;

/******************************************************************************
* Function: void catchSIGTSTP(int signal)
* Description: If the SIGTSTP signal is activated a message will print indicating
*	whether or not the user can run something in the background or not. If the
*	background is not available the code will not ignore the & sign at the end of
*	the user input. There will be a global variable that indicates whether or not
*	the user can run in the background. The reason a global variable is used is
*	referenced in one of the source links below. Basically signals handlers already
*	function as a global state which is equivalent to global variables. Therefore
*	the user of global variables in this situation is much more justified. Also
*	the suggestion from Alexander Bernal (TA) was that global variables would
*	be the best approach to use. Code based off of code from lecture.
* Input: int signal
* Output: N/A
******************************************************************************/
void catchSIGTSTP(int signal) {
	if (activeBackground == 1) {
		char* message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 50);
		fflush(stdout);
		message = ": ";
		write(STDOUT_FILENO, message, 3);
		fflush(stdout);
		activeBackground = 0;
	}
	else {
		char* message = "Exiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 30);
		fflush(stdout);
		message = ": ";
		write(STDOUT_FILENO, message, 3);
		fflush(stdout);
		activeBackground = 1;
	}
}

/******************************************************************************
* Function: void catchSIGQUIT(int signal)
* Description: This function will catch the kill signal when the user types in
*	exit. Instead of keeping an array of pids and trying to keep track of which
*	ones have finished which could slow down the shell. I decided to use a kill
*	signal only for the children and then let the parent processes finish out
*	really quickly. Code modified from these sources:
*	https://ideone.com/4zs4u3
*	https://stackoverflow.com/questions/18433585/kill-all-child-processes-of-a-parent-but-leave-the-parent-alive
* Input: int signal
* Output: dead children
******************************************************************************/
void catchSIGQUIT(int signal) {
	assert(signal == SIGQUIT);
	pid_t self = getpid();
	if (parentPid != self) _exit(0);
}



/******************************************************************************
* Function: void showExitStatus(int childExitMethod)
* Description: This function is taken from the lecture notes. This function will
*	check the exist status and print it to the screen.
*	WIFEXITED: if the the process terminated normally, WIFEXITED macro returns a
*		non-zero value.
*	WEXITSTATUS: get the terminating signal with this macro (can't use WEXITSTATUS
*		if terminated by signal since there was no exit status)
*	WTERMSIG: get the terminating signal with this macro (can't use WTERMSIG
*		if terminated normally since there was no signal number that killed it)
* Input: int childExitMethod
* Output: N/A
******************************************************************************/
void showExitStatus(int childExitMethod) {
	if (WIFEXITED(childExitMethod)) {
		printf("exit value %d\n", WEXITSTATUS(childExitMethod));
	}
	else {
		printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
	}
}

/******************************************************************************
* Function: replaceToPid(char* token)
* Description: This function will replace all of the instances of $$ from the
*	userInput into the pid. This code is based off of a function found at this
*	site: https://www.intechgrity.com/c-program-replacing-a-substring-from-a-string/
* Input: char* token
* Output: N/A
******************************************************************************/
void replaceToPid(char* token) {
	char* ch = NULL;

	//If there is no occurence of $$ in the string, return back to main
	if (!(ch = strstr(token, "$$"))) {
		return;
	}

	//a buffer variable
	char buffer[MAX_LENGTH] = { 0 };

	//Get the current pid
	int pid = getpid();

	//Convert the pid integer value into a string
	char pidBuffer[snprintf(NULL, 0, "%d", pid) + 1];
	sprintf(pidBuffer, "%d", pid);

	//Copy the token string into the buffer before the first occurence of the substring
	strncpy(buffer, token, ch - token);

	//add null to the end of the buffer
	buffer[ch - token] = 0;

	//combine the strings together to make one long string
	sprintf(buffer + (ch - token), "%s%s", pidBuffer, ch + strlen("$$"));

	//Clear out the inital string
	token[0] = 0;

	//copy the buffer into the initial string
	strcpy(token, buffer);

	//pass recursively to replaceToPid to check for other occurences of $$ in the string
	replaceToPid(token);
}


/******************************************************************************
* Function: int main()
* Description: Refer to description at top of file and comments below.
******************************************************************************/
int main() {

	//Initialize Variables
	int	ending = 0;
	int backgroundCalled = 0;
	int currStatus = 0;
	int input = -1;
	int output = -1;
	int currChar = -5;
	int numCharsEntered = -5;
	int currPids[MAX_ARG];
	char* token;
	char* userInput = NULL;
	char* arguments[MAX_ARG];
	char* inputFile = NULL;
	char* outputFile = NULL;
	size_t bufferSize = 0;
	struct sigaction SIGINT_action = { 0 };
	struct sigaction SIGTSTP_action = { 0 };

	//Ctr-C Signal
	SIGINT_action.sa_handler = SIG_IGN;				//Ignore the signal
	SIGINT_action.sa_flags = 0;						//No flags
	sigfillset(&SIGINT_action.sa_mask);				//initializes set to full, blocking all signals arriving while this mask is in place
	sigaction(SIGINT, &SIGINT_action, NULL);		//registers a signal handling function for ^C

	//Ctr-Z Signal
	SIGTSTP_action.sa_handler = catchSIGTSTP;		//call the catchSIGTSP function if signal triggered.
	SIGTSTP_action.sa_flags = SA_RESTART;			//SA_RESTART to prevent issues with getline()
	sigfillset(&SIGTSTP_action.sa_mask);			//initializes set to full, blocking all signals arriving while this mask is in place
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);		//registers a signal handling function for ^Z

	while (!ending) {
		//Print prompt
		printf(": "); fflush(stdout);

		//Get the user input
		numCharsEntered = getline(&userInput, &bufferSize, stdin);

		//If the user doesn't enter anything (not even newline) exit the program
		if (numCharsEntered == 0) {
			return 0;
		}
		//If there is an error with getline, usually due to a signal call causing
		//	an error, clear std in
		if (numCharsEntered == -1) {
			clearerr(stdin);
		}

		//call the function that replaces $$ with the current pid
		replaceToPid(userInput);

		//Define the token variable and a tracker for the number of arguments placed
		int numOfArg = 0;
		token = strtok(userInput, " \n");

		//Break apart the user input string using strtok and strdup
		while ((token != NULL) && (numOfArg < (MAX_ARG - 1))) {
			backgroundCalled = 0;

			//If the token is telling the file to output to stdout, get the output file name 
			if (strcmp(token, ">") == 0) {
				token = strtok(NULL, " \n");
				outputFile = strdup(token);
				token = strtok(NULL, " \n");
			}
			//If the token is telling the file to feed to stdin, get the output file name
			else if (strcmp(token, "<") == 0) {
				token = strtok(NULL, " \n");
				inputFile = strdup(token);
				token = strtok(NULL, " \n");
			}
			//If the token is ampersand, indicate that the output should run the background
			else if (strcmp(token, "&") == 0) {
				backgroundCalled = 1;
				token = strtok(NULL, " \n");
			}
			//If it is none of the above, add the arguments to a list of characters
			else {
				arguments[numOfArg] = strdup(token);
				numOfArg++;
				token = strtok(NULL, " \n");
			}
		}

		//Initialize the end of the array with a NULL so that we can test to see if we
		//	reached the end of the array
		arguments[numOfArg] = NULL;

		/*****************************************************************************
		* This next section is actually the majority of the rest of the code. There are
		* 5 if/else if/else statements.
		*	1) arguments[0] array to be if the user input a comment or blank line.
		*	2) arguments[0] array == "exit"
		*	3) arguments[0] array == "status"
		*	4) arguments[0] array == "cd"
		*	5) else we begin to create childen by using fork(). Create a switch statement
		*		to account for 4 different scenarios.
		*			a) case -1: there is an error and we need to exit(1) and break out of the
		*					statement.
		*			b) case 0: check if there is an input and/or output file to process. If
		*					there is, then execute it using execvp(). Indicate where there
		*					were any errors opening, writing or closing files.
		*			c) default: check to see if it should be run in the background and
		*					whether or not it is allowed to run in the background. If
		*					if it can run in background and is supposed to, run in the
		*					background, otherwise it ignore it. If not requested to be
		*					run in the background, run as normal.
		*					In both instances print the childs pid to the screen.
		*		Always check for terminated background processes and notify the user that
		*		these background processes are completed.
		*		This switch statement format is taken from the lecture examples.
		******************************************************************************/
		if (arguments[0] == NULL || strcmp(arguments[0], "#") == 0) {
			//Do absolutely nothing.
		}
		else if (strcmp(arguments[0], "exit") == 0) {

			//get the parent pid so that it doesn't close this pid
			parentPid = getpid();

			//Use the SIGQUIT signal to kill all of the children processes while keeping
			//	the parent process intact
			signal(SIGQUIT, SIG_IGN);
			kill(-parentPid, SIGQUIT);

			//Tell the shell you are done running it and close the program
			ending = 1;
		}
		else if (strcmp(arguments[0], "status") == 0) {
			//Call showExitStatus function to determine what the current status is
			showExitStatus(currStatus);
		}
		else if (strcmp(arguments[0], "cd") == 0) {
			//if there is an argument as to which directory to go into, try to access it 
			if (arguments[1] != NULL) {
				//If there is an error trying to open up the directory, display an error
				if (chdir(arguments[1]) == -1) {
					printf("Directory not found.\n"); fflush(stdout);
				}
			}
			//if there is no argument given, cd to the "HOME" directory
			else {
				chdir(getenv("HOME"));
			}
		}
		else {
			//create a child using fork()
			pid_t spawnPid = -5;
			spawnPid = fork();

			//create the switch statement described above
			switch (spawnPid) {
			case -1:
				perror("Error Occurred While You Were Forking Around!\n");
				exit(1);
				break;

			case 0:
				//If it is supposed to run in the foreground, change the signal handler for ^C
				if (backgroundCalled == 0 || (backgroundCalled == 1 && activeBackground == 0)) {
					SIGINT_action.sa_handler = SIG_DFL;
					sigaction(SIGINT, &SIGINT_action, NULL);
				}
				//Check to see if there is an input file. If there is, then open it and read
				//	the contents into stdin.
				if (inputFile != NULL) {
					input = open(inputFile, O_RDONLY);
					if (input == -1) {
						printf("cannot open %s for input\n", inputFile); fflush(stdout);
						_exit(1);
					}
					if (dup2(input, 0) == -1) {
						perror("error with input");
						_exit(1);
					}
				}
				//Check to see if there is an input file. If there is, then open it and write
				//	the contents to the file.
				if (outputFile != NULL) {
					output = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
					if (output == -1) {
						printf("cannot open %s for output\n", outputFile); fflush(stdout);
						_exit(1);
					}
					if (dup2(output, 1) == -1) {
						perror("error with input");
						_exit(1);
					}
					fcntl(output, F_SETFD, FD_CLOEXEC);
				}
				//Run the arguments in bash, if there is an error then display a message.
				if (execvp(arguments[0], arguments)) {
					printf("%s: no such file or directory\n", arguments[0]); fflush(stdout);
					_exit(1);
				}
				break;

			default:
				//if the input is supposed to run in the background and the background is enabled,
				//	run this statement. The input will run in the background and will not hang up
				//	since WNOHANG is called.
				if (activeBackground == 1 && backgroundCalled == 1) {
					waitpid(spawnPid, &currStatus, WNOHANG);
					printf("background pid is %d\n", spawnPid); fflush(stdout);
				}
				//if not running in the background, run the tests in the foreground and wait until
				//	the process is completed before returning the prompt for the user. Signals can
				//	still be entered in as long as they are not disabled.
				else {
					waitpid(spawnPid, &currStatus, 0);
					if (WIFSIGNALED(currStatus) != 0) {
						showExitStatus(currStatus);
					}
				}

				//Check to see if any pending processes have completed and print out that they are complete
				//	for the user to see.
				while ((spawnPid = waitpid(-1, &currStatus, WNOHANG)) > 0) {
					printf("background pid %d is done: ", spawnPid);
					showExitStatus(currStatus);
					fflush(stdout);
				}
			}
		}
		pid_t spawnPid = -5;
		while ((spawnPid = waitpid(-1, &currStatus, WNOHANG)) > 0) {
			printf("background pid %d is done: ", spawnPid);
			showExitStatus(currStatus);
			fflush(stdout);
		}

		//Free the input and output files to prevent leaks and initialize them for the
		//	next run through if necessary. This will be done regardless if input and output
		//	files are used just to be on the safe side.
		free(inputFile);
		inputFile = NULL;
		free(outputFile);
		outputFile = NULL;

		//Free all of the arguments left in the argument array to prevent leaks
		int i;
		for (i = 0; arguments[i] != NULL; i++) {
			free(arguments[i]);
		}
	}
	//Free the userInput to prevent leaks
	free(userInput);

	return 0;
}

/******************************************************************************
* Sources:
*
* //bash cheat sheet
* https://github.com/LeCoupa/awesome-cheatsheets/blob/master/languages/bash.sh
*
* //Signals
* http://man7.org/linux/man-pages/man2/signal.2.html
* https://www.linux.org/threads/kill-signals-and-commands-revised.11625/
* https://www.geeksforgeeks.org/signals-c-language/
* http://www.man7.org/linux/man-pages/man7/signal-safety.7.html
* https://stackoverflow.com/questions/6970224/providing-passing-argument-to-signal-handler#comment56969091_6970238
*
* //Compile/Makefile
* https://en.wikipedia.org/wiki/C99
* https://stackoverflow.com/questions/25566597/how-enable-c99-mode-in-gcc-with-terminal
*
* //Strtok and strdup
* http://www.qnx.com/developers/docs/6.5.0SP1.update/com.qnx.doc.neutrino_lib_ref/s/strtok.html
* https://www.codingame.com/playgrounds/14213/how-to-play-with-strings-in-c/string-split
* https://linux.die.net/man/3/strtok
* https://www.geeksforgeeks.org/strtok-strtok_r-functions-c-examples/
* https://stackoverflow.com/questions/14020380/strcpy-vs-strdup
* https://stackoverflow.com/questions/40222582/segfault-resulting-from-strdup-and-strtok
*
* //Find and Replace $$ in user string
* https://www.intechgrity.com/c-program-replacing-a-substring-from-a-string/#
* https://stackoverflow.com/questions/15098936/simple-way-to-check-if-a-string-contains-another-string-in-c
*
* //Convert pid to a string
* https://ubuntuforums.org/showthread.php?t=1430052
* http://www.cplusplus.com/reference/cstdio/sprintf/
* https://stackoverflow.com/questions/36274902/convert-int-to-string-in-standard-c
*
* //Redirecting stdout & stdin with execvp
* http://man7.org/linux/man-pages/man2/fcntl.2.html
* https://linux.die.net/man/3/execlp
* https://linux.die.net/man/3/execvp
* http://www.csl.mtu.edu/cs4411.ck/www/NOTES/process/fork/exec.html
*
* //changing directories using cd
* http://man7.org/linux/man-pages/man3/getenv.3.html
* https://pubs.opengroup.org/onlinepubs/009695399/functions/getenv.html
*
* //exit vs. _exit
* https://www.unix.com/programming/116721-difference-between-exit-_exit.html
* http://www.unixguide.net/unix/programming/1.1.3.shtml
* http://man7.org/linux/man-pages/man2/_exit.2.html
*
* //Kill children processes
* https://ideone.com/4zs4u3
* https://stackoverflow.com/questions/18433585/kill-all-child-processes-of-a-parent-but-leave-the-parent-alive
* https://www.linux.org/threads/kill-signals-and-commands-revised.11625/
*
* //perror
* https://www.tutorialspoint.com/c_standard_library/c_function_perror.htm
*
*******************************************************************************/