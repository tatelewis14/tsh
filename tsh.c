#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

#define TK_BUFF_SIZE 64
#define TSH_BUFF_SIZE 1024

#define RED   		"\033[0;31m"
#define YELLOW 		"\033[0;33m"
#define CYAN 		"\033[0;36m"
#define GREEN 		"\033[0;32m"
#define BLUE 		"\033[0;34m"
#define INVERT		"\033[0;7m"
#define RESET  		"\e[0m" 
#define BOLD		"\e[1m"
#define ITALICS		"\e[3m"
#define TOK_DELIM " \t\r\n\a"
#define PIPE_DELIM '|'

char* get_file_history_path();
int tsh_launch(char** args);
int tsh_exit(char** args);
char** read_history();
void write_to_history(char* line);
int tsh_cd(char** args);
char** split_line(char* line);
char* read_line();
int parse_for_piped(char* line);
int tsh_launch_pipes(char** args);

const char* builtin_func_strings[2] =
{
	"cd",
	"exit"
};

int num_of_builtins()
{
	return sizeof(builtin_func_strings) / sizeof(char*);
}

int (*builtin_func[]) (char**) = 
{
	&tsh_cd,
	&tsh_exit
};

char** read_history()
{
	char** buffer = (char**)malloc(TSH_BUFF_SIZE * sizeof(char));
	if(!buffer)
	{
		fprintf(stderr, "%stsh: Allocation error%s\n", RED, RESET);	
	}
	FILE* path;
	path = fopen(get_file_history_path(), "r");
	if(!path)
	{
		perror("tsh");
		return NULL;
	} else {
		int position = 0;
		do
		{
			fgets(buffer[position],TK_BUFF_SIZE, path);
			position++;
		} while (fgetc(path) != EOF);
		fclose(path);
	}
	return buffer;
}

char* get_file_history_path()
{
	char* path = (char*)malloc(1024 * sizeof(char));
	if(path == NULL) 
	{
		fprintf(stderr, "%stsh: Allocation error%s\n", RED, RESET);
		return NULL;
	}
	char* home = getenv("HOME");
	if(home == NULL)
	{
		fprintf(stderr,"%stsh: HOME environment variable not set%s\n", RED, RESET);
		free(path);
		free(home);
		return NULL;
	}
	strcat(strcpy(path, home), "/.tsh_history.txt");

	return path;
}


void write_to_history(char* line)
{
	if(line == NULL) {
		fprintf(stderr, RED "ARGUEMENT TO WRITE TO HISTORY IS NULL" RESET);
		return;
	}
	FILE* file;
	file = fopen(get_file_history_path(), "a+");
	fprintf(file, "%s\n", line);
	fclose(file);
}

int tsh_exit(char** args)
{
	write_to_history(args[0]);
	return 0;
}

int tsh_cd(char** args)
{
	if(args[1] == NULL)
	{
		fprintf(stderr, "%stsh expected a directory to \'%s%scd%s%s\' into%s\n", RED,ITALICS, BOLD, RESET, RED, RESET);
	} else if(chdir(args[1]) != 0)
	{
		perror("tsh");
	} else {
		//write_to_history(strcat(args[0], args[1]));
	}
	return 1;
}

int tsh_execute(char** args)
{
	if(args[0] == NULL) return 1;
	for(int i = 0; i < num_of_builtins(); i++)
	{
		if(strcmp(args[0], builtin_func_strings[i]) == 0)
		{
			return (*builtin_func[i])(args);
		}
	}
	return tsh_launch(args);
}

int tsh_launch(char** args)
{
	pid_t cpid;
	int status;
	
	cpid = fork();

	if(cpid == 0)
	{
		//this is so that bin executables can be run without typing /bin/(name of exe)
		if(execvp(args[0], args) < 0)
		{
			printf("tsh: command not found: %s\n", args[0]);
    	exit(EXIT_FAILURE);
		}
	} else if (cpid < 0)
	{
		printf(RED "Error forking" RESET "\n");	
	} else 
	{
		 waitpid(cpid, &status, WUNTRACED);
	}

	return 1;
}

int tsh_launch_piped(char** args)
{
	int status = 0;
	int pipefds[2]; 
	pipe(pipefds);
	char* buffer = (char*)malloc(1024 * sizeof(char*));

	char** before_pipe;
	char** after_pipe = args;
	before_pipe = (char**)strsep(after_pipe, "|");

	pid_t cpid1 = fork();

	if(cpid1 == 0)
	{
		
		close(pipefds[0]);
		dup2(pipefds[1], STDOUT_FILENO);
		close(pipefds[1]);


		if(execvp(before_pipe[0], before_pipe) < 0)
		{
			printf("tsh: command not found: %s\n", args[0]);
		}
	} else if(cpid1 < 0){
			printf(RED "Error forking" RESET "\n");
	} else {
		waitpid(cpid1, &status, WUNTRACED);
	}
	
	pid_t cpid2 = fork();

	if(cpid2 == 0)
	{
		read(pipefds[0], buffer, sizeof(buffer)); 
		printf("Got from first process: %s\n", buffer);
	} else if (cpid2 < 0) {
		printf(RED "Error forking" RESET "\n");
	} else {
		waitpid(cpid2, &status, WUNTRACED);
	}

	close(pipefds[0]);
	close(pipefds[1]);


	free(buffer);
	return 1;	
}


char** split_line(char* line)
{
	int buffsize = 1024;
	int position = 0;
	char** tokens = (char**)malloc(buffsize * sizeof(char*));
	char* token;

	if(!tokens) 
	{
    fprintf(stderr, "%stsh: Allocation error%s\n", RED, RESET);
    exit(EXIT_FAILURE);
  }
	token = strtok(line, TOK_DELIM);
	while(token != NULL)
	{
		tokens[position] = token;
		position++;

		if(position >= buffsize)
		{
			buffsize += TK_BUFF_SIZE;
			tokens = (char**)realloc(tokens, buffsize * sizeof(char * ));
		}
		if(!tokens)
		{
			fprintf(stderr, "%stsh: Allocation error%s\n", RED, RESET);
      exit(EXIT_FAILURE);
		}
		token = strtok(NULL, TOK_DELIM);
	}
	tokens[position] = NULL;
	return tokens;
}


char* read_line()
{
	int buffsize = 1024;
	int position = 0;
	char* buffer = (char*)malloc(1024 * sizeof(char));
	int c;
	if(!buffer) 
	{
		fprintf(stderr, "%stsh: Allocation error%s\n", RED, RESET);
		exit(EXIT_FAILURE);
	}
	while(1)
	{
		c = getchar();
		if(c == EOF || c == '\n')
		{
			buffer[position] = '\0';
			return buffer;
		} 
		else 
		{
			buffer[position] = c;
		}
		position++;
		if(position >= buffsize) buffsize += 1024;
		buffer = (char*)realloc(buffer, buffsize);

		if(!buffer) 
		{
			fprintf(stderr, "%stsh: Allocation error%s\n", RED, RESET);
			exit(EXIT_FAILURE);
		}	
	} 
}

int parse_for_piped(char* line)
{
	int position = 0;
	char* before_pipe = (char*)malloc(1024 * sizeof(char));
	if(!before_pipe)
	{
		perror("tsh before pipe:");
		return 1;
	}
	char* after_pipe = line;
	if(!after_pipe) {
		perror("tsh after pipe");
		return 1;
	}	

	before_pipe = strsep(&after_pipe, "|");	
	if(after_pipe == NULL) return 1;

	printf("before_pipe: %s\n", before_pipe);			
	printf("after_pipe: %s\n", after_pipe);

	return 2;
}

void loop()
{
	char* line;
	char** args;
	int status = 1;
	int piped = 1;

	do
	{
		char cwd[PATH_MAX];
		char* name = getlogin();
		getcwd(cwd, sizeof(cwd));
		if(strcmp(cwd, getenv("HOME")) == 0)
		{
			printf("%s - %s %% ", name, "~");
		} else {
			printf("%s - %s %% ", name, basename(cwd));
		}
		line = read_line();
		piped = parse_for_piped(line);
		args = split_line(line);
		if(piped == 2)
		{
			status = tsh_launch_piped(args);
		} else if (piped == 1){
			status = tsh_execute(args);
		} else {
			perror("tsh in loop:");
		}
		free(line);
		free(args);
	} while (status);
}


int main(int argc, char** argv)
{
	loop();	
	printf(CYAN "exited tsh\n" RESET);
	return EXIT_SUCCESS; 
}
