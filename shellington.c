#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
const char * sysname = "shellington";

#define PATH_LEN  2048
char short_cmm_list[PATH_LEN];

enum return_codes {
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};
struct command_t {
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3]; // in/out redirection
	struct command_t *next; // for piping
};

// Bookmark Node Structure (LinkedList)
struct bmnode {
	int key;
	//struct command_t *command;
	struct bmnode *next;
	char book[256];
};

struct bmnode *bmhead = NULL;
struct bmnode *bmcurrent = NULL;

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t * command)
{
	int i=0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background?"yes":"no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete?"yes":"no");
	printf("\tRedirects:\n");
	for (i=0;i<3;i++)
		printf("\t\t%d: %s\n", i, command->redirects[i]?command->redirects[i]:"N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i=0;i<command->arg_count;++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}


}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i=0; i<command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i=0;i<3;++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next=NULL;
	}
	free(command->name);
	free(command);
	return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
    gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{

	char str[256] = "";
	strcat(str, buf);
	char *token = strtok(str, " \"");


	if(strcmp(token, "bookmark") == 0) {
		command->name=(char *)malloc(strlen(token)+1);
		strcpy(command->name, token);

		char ar[1] = "";

		token = strtok(NULL, "");
		strncat(ar, token, 1);

		if (strcmp(ar, "\"") == 0) {
			printf("Enter bookmark if\n");
			int arg_i = 0;
                	command->args=(char **)malloc(sizeof(char *)*2);
                	command->args[arg_i]=(char *)malloc(strlen(token)+1);
               		printf("command arg0 is %s\n", token);
                	strcpy(command->args[arg_i], token);
                	command->arg_count = 1;
                	return SUCCESS;

		}

	}

	const char *splitters=" \t"; // split at whitespace
	int index, len;
	len=strlen(buf);
	while (len>0 && strchr(splitters, buf[0])!=NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len>0 && strchr(splitters, buf[len-1])!=NULL)
		buf[--len]=0; // trim right whitespace

	if (len>0 && buf[len-1]=='?') // auto-complete
		command->auto_complete=true;
	if (len>0 && buf[len-1]=='&') // background
		command->background=true;

	char *pch = strtok(buf, splitters);
	command->name=(char *)malloc(strlen(pch)+1);
	if (pch==NULL)
		command->name[0]=0;
	else
		strcpy(command->name, pch);

	command->args=(char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index=0;
	char temp_buf[1024], *arg;
	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch) break;
		arg=temp_buf;
		strcpy(arg, pch);
		len=strlen(arg);

		if (len==0) continue; // empty arg, go for next
		while (len>0 && strchr(splitters, arg[0])!=NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len>0 && strchr(splitters, arg[len-1])!=NULL) arg[--len]=0; // trim right whitespace
		if (len==0) continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|")==0)
		{
			struct command_t *c=malloc(sizeof(struct command_t));
			int l=strlen(pch);
			pch[l]=splitters[0]; // restore strtok termination
			index=1;
			while (pch[index]==' ' || pch[index]=='\t') index++; // skip whitespaces

			parse_command(pch+index, c);
			pch[l]=0; // put back strtok termination
			command->next=c;
			continue;
		}

		// background process
		if (strcmp(arg, "&")==0)
			continue; // handled before

		// handle input redirection
		redirect_index=-1;
		if (arg[0]=='<')
			redirect_index=0;
		if (arg[0]=='>')
		{
			if (len>1 && arg[1]=='>')
			{
				redirect_index=2;
				arg++;
				len--;
			}
			else redirect_index=1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index]=malloc(len);
			strcpy(command->redirects[redirect_index], arg+1);
			continue;
		}

		// normal arguments
		if (len>2 && ((arg[0]=='"' && arg[len-1]=='"')
			|| (arg[0]=='\'' && arg[len-1]=='\''))) // quote wrapped arg
		{
			arg[--len]=0;
			arg++;
		}
		command->args=(char **)realloc(command->args, sizeof(char *)*(arg_index+1));
		command->args[arg_index]=(char *)malloc(len+1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count=arg_index;
	return 0;
}
void prompt_backspace()
{
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index=0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

    // tcgetattr gets the parameters of the current terminal
    // STDIN_FILENO will tell tcgetattr that it should write the settings
    // of stdin to oldt
    static struct termios backup_termios, new_termios;
    tcgetattr(STDIN_FILENO, &backup_termios);
    new_termios = backup_termios;
    // ICANON normally takes care that one line at a time will be processed
    // that means it will return if it sees a "\n" or an EOF or an EOL
    new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
    // Those new settings will be set to STDIN
    // TCSANOW tells tcsetattr to change attributes immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);


    //FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state=0;
	buf[0]=0;
  	while (1)
  	{
		c=getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c==9) // handle tab
		{
			buf[index++]='?'; // autocomplete
			break;
		}

		if (c==127) // handle backspace
		{
			if (index>0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c==27 && multicode_state==0) // handle multi-code keys
		{
			multicode_state=1;
			continue;
		}
		if (c==91 && multicode_state==1)
		{
			multicode_state=2;
			continue;
		}
		if (c==65 && multicode_state==2) // up arrow
		{
			int i;
			while (index>0)
			{
				prompt_backspace();
				index--;
			}
			for (i=0;oldbuf[i];++i)
			{
				putchar(oldbuf[i]);
				buf[i]=oldbuf[i];
			}
			index=i;
			continue;
		}
		else
			multicode_state=0;

		putchar(c); // echo the character
		buf[index++]=c;
		if (index>=sizeof(buf)-1) break;
		if (c=='\n') // enter key
			break;
		if (c==4) // Ctrl+D
			return EXIT;
  	}
  	if (index>0 && buf[index-1]=='\n') // trim newline from the end
  		index--;
  	buf[index++]=0; // null terminate string

  	strcpy(oldbuf, buf);

  	parse_command(buf, command);

  	// print_command(command); // DEBUG: uncomment for debugging

    // restore the old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  	return SUCCESS;
}
int process_command(struct command_t *command);
int main()
{

	if (getcwd(short_cmm_list, sizeof(short_cmm_list)) != NULL) {
		printf("Current working dir: %s\n", short_cmm_list);
		strcat(short_cmm_list, "/s_list.txt");
	}

	while (1)
	{
		struct command_t *command=malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code==EXIT) break;

		code = process_command(command);
		if (code==EXIT) break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

// Function Declarations
int crobtab(struct command_t *command, char *file_name);
void remindme_command(struct command_t *command);
//int short_command(struct command_t *command);
//void is_key_exist(char *key);
int bookmark_command(struct command_t *command);
void printAllBookmarks();
//bool bookmarkIsEmpty();
int bookmarkLength();
//struct bmnode* findBookmark(int key);
void addBookmark(struct command_t *command);
//int deleteBookmark(int key);
int kerem_awesome_command(struct command_t *command);
int beyza_awesome_command(struct command_t *command);
void waitSec(int sec);
int pstraverse(struct command_t *command);


int process_command(struct command_t *command)
{
	int r;
	if (strcmp(command->name, "")==0) return SUCCESS;

	if (strcmp(command->name, "exit")==0)
		return EXIT;

	if (strcmp(command->name, "cd")==0)
	{
		if (command->arg_count > 0)
		{
			r=chdir(command->args[0]);
			if (r==-1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}

	if (strcmp(command->name, "remindme") == 0) {
		remindme_command(command);
		return SUCCESS;
	}

	/*if (strcmp(command->name, "short") == 0) {
		short_command(command);
		return SUCCESS;
	}*/

	if(strcmp(command->name, "bookmark") == 0) {
		bookmark_command(command);
		return SUCCESS;
	}

	if (strcmp(command->name, "beyza") == 0) {
		beyza_awesome_command(command);
		return SUCCESS;
	}
	
	if (strcmp(command->name, "kerem") == 0) {
		kerem_awesome_command(command);
		return SUCCESS;
	}

	if (strcmp(command->name, "pstraverse") == 0) {
		pstraverse(command);
		return SUCCESS;
	}

	pid_t pid=fork();
	if (pid==0) // child
	{
		/// This shows how to do exec with environ (but is not available on MacOs)
	    // extern char** environ; // environment variables
		// execvpe(command->name, command->args, environ); // exec+args+path+environ

		/// This shows how to do exec with auto-path resolve
		// add a NULL argument to the end of args, and the name to the beginning
		// as required by exec

		// increase args size by 2
		command->args=(char **)realloc(
			command->args, sizeof(char *)*(command->arg_count+=2));

		// shift everything forward by 1
		for (int i=command->arg_count-2;i>0;--i)
			command->args[i]=command->args[i-1];

		// set args[0] as a copy of name
		command->args[0]=strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count-1]=NULL;

		//execvp(command->name, command->args); // exec+args+path
		
		/// TODO: do your own exec with path resolving using execv()
		
		// the first argument of execvp() is the filename, it can resolve the path automatically, 
		// it looks for the filename in all of the directories in the PATH environment variable
		//
		// however, the first argument of execv() is a path to the executable, it does not resolve the path by itself
		// we should provide the path to it
		

		char *currentPath = getenv("PATH");
		char *pathToken = strtok(currentPath, ":");
		char *lastCofToken = pathToken; // to test whether we are at the last char of the currentPath or not

		while(pathToken != NULL) {

			char pathToCommand[strlen(command->name) + strlen(pathToken) + 1];
			strcpy(pathToCommand, pathToken); // add all the tokens to the path

			if(*(lastCofToken+1) != 0) {
				lastCofToken++;
			} else {
				if(strcmp(lastCofToken, "/") != 0) {
					// if we are at the last char, we add / to append the command name at the end of the path
					strcat(pathToCommand, "/");
				}

				strcat(pathToCommand, command->name);
				execv(pathToCommand, command->args); // if we are at the last char, execute the command after adding command name

				pathToken = strtok(NULL, ":");
			}
		}

		exit(0);
		
	}
	else
	{
		if (!command->background)
			wait(0); // wait for child process to finish
		return SUCCESS;
	}

	// TODO: your implementation here

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}

int crontab(struct command_t *command, char *file_name) {
	if(command->args[0] == NULL) {
		perror("There is no enough argument to run remindme command!");
		return -1;
	}

	char *message = (char *)malloc(strlen(command->args[1])+1);
	strcpy(message, command->args[1]);	

	char *time = (char *)malloc(strlen(command->args[0])+1);
	strcpy(time, command->args[0]);

	char *timeToken = strtok(time, ".");
	char *hour = (char *)malloc(strlen(timeToken)+1);
	strcpy(hour, timeToken);

	timeToken = strtok(NULL, ".");
	char *minute = (char *)malloc(strlen(timeToken)+1);
	strcpy(minute, timeToken);

	//printf("%s\n", time);
	//printf("%s\n", hour);
	//printf("%s\n", minute);
	//printf("%s\n", message);
	
	char *notify_cmm = "XDG_RUNTIME_DIR=/run/user/$(id -u) /usr/bin/notify-send";

	int cmm_line_len = strlen(minute) + strlen(hour) + 3 + strlen(notify_cmm) + strlen(message) + 10;

	char *cmm_line = (char *)malloc(sizeof(char) * cmm_line_len);
	snprintf(cmm_line, cmm_line_len, "%s %s %s %s %s %s \"%s\"", minute, hour, "*", "*", "*", notify_cmm, message);

	FILE *file;
	file = fopen(file_name, "w");

	if(file != NULL) {
		strcat(cmm_line, "\n");
		fputs(cmm_line, file);
		printf("%s", cmm_line);

		free(message);
		free(time);
		free(hour);
		free(minute);
		free(cmm_line);
		fclose(file);
	} else {
		perror("Cannot open the file!");
		return -1;
	}
}

void remindme_command(struct command_t *command) {
	char *cron_file = "cron.txt";
	crontab(command, cron_file);
	execlp("crontab", "crontab", cron_file, NULL);
}

/*int short_command(struct command_t *command) {
	if(command->args[0] == NULL || command->args[1] == NULL) {
		perror("Not enough argument to run short command\n");
		return -1;	
	}

	char *indicator = (char *)malloc(strlen(command->args[0])+1);
	strcpy(indicator, command->args[0]);

	char *name = (char *)malloc(strlen(command->args[1])+1);
	strcpy(name, command->args[1]);

	if (strcmp(indicator, "set") == 0) {
		is_key_exist(name);

	} else if (strcmp(indicator, "jump") == 0) {
		
	} else {
		perror("You did not provide a valid option for short command\n");
		return -1;
	}
}

void is_key_exist(char *key) {
	char line[512];
	FILE *file = fopen(short_cmm_list, "r");
	FILE *tmpfile = fopen("tmpfile.txt", "w");
	int count = 1;

	if(file ==  NULL || tmpfile == NULL) {
		perror("Cannot open the file!\n");
	}

	rewind(file);


	while(fgets(line, 512, file)) {
		char *token = strtok(line, " ");

		if(strcmp(key, token) != 0) {
			fputs(line, tmpfile);	
		}

	}

	fclose(file);
	fclose(tmpfile);

	remove(short_cmm_list);
	rename(tmpfile, short_cmm_list);

} */

int bookmark_command(struct command_t *command) {
	/*if(command->arg_count == 2) {
		
		if(strcmp(command->args[0], "-i") == 0) {
			// Execute command at index
			int bm_index = atoi(command->args[1]);

			if(bm_index < bookmarkLength()) {
				process_command(findBookmark(bm_index)->command);
			} else {
				printf("\nIndex out of bounds\n");
			}
		}
	
		if(strcmp(command->args[0], "-d") == 0) {
			// Delete bookmark at index
			int bm_index = atoi(command->args[1]);
			
			deleteBookmark(bm_index);
		}*/


	if (command->arg_count == 1) {
		if (strcmp(command->args[0], "-l") == 0) {
			printAllBookmarks();
		} else {
			addBookmark(command);
		}

	} else if (command->arg_count == 2) {
		printf("Two arguments\n");
		if (strcmp(command->args[0], "-d") == 0) {
			int bm_index = atoi(command->args[1]);
			int len = bookmarkLength();
			printf("Bookmark list len: %d\n", len);

			if(bm_index > len) {
				perror("Index out of bound!\n");
				return -1;
			} else {
				printf("delete index = %d\n",bm_index);
			}
		} else if (strcmp(command->args[0], "-i") == 0) {
			int bm_index = atoi(command->args[1]);
			int len = bookmarkLength();
			printf("Bookmark list len: %d\n", len);

			if(bm_index > len) { 
				perror("Index out of bound!\n");
				return -1;
			} else {
				printf("run index = %d\n", bm_index);
			}
		} else {
			perror("You provided wrong arguments to use Bookmark!\n");
			return -1;
		}

	} else {
		perror("Non-permitted use of Bookmark.\n");
		return -1;
	}
}

// Method to traverse and print all saved bookmarks
void printAllBookmarks() {
	struct bmnode *ptr = bmhead;
	printf("Current Bookmarks\n\n");
	
	while(ptr != NULL) {
		printf("\t %d ", ptr->key);
		printf("%s\n", ptr->book);
		ptr = ptr->next;
	}
}

/*bool bookmarkIsEmpty() {
	return bmhead == NULL;	
}*/

// Method that gives the number of saved bookmark commands
int bookmarkLength() {
	int length = 0;
	struct bmnode **head_ref = &bmhead;
	struct bmnode *curr = *head_ref;

	while(curr->next != NULL) {
		length++;
		curr = curr->next;
	}

	length++;
	
	return length;
}

/*struct bmnode* findBookmark(int key) {
	struct bmnode* bmcurrent = bmhead;
	
	if(bookmarkIsEmpty()) return NULL;
	
	while(bmcurrent->key != key) {
		if(bmcurrent->next == NULL) {
			return NULL;
		} else {
			bmcurrent = bmcurrent->next;
		}
	}
	
	return bmcurrent;
}*/

// Method to add a new bookmark command at the end of the linked list structure
void addBookmark(struct command_t *command) {
	struct bmnode *new_node = (struct bmnode*) malloc(sizeof(struct bmnode));
	struct bmnode **head_ref = &bmhead;

	struct bmnode *ptr = *head_ref;

	char bookm[256] = "";
	strcat(bookm, command->args[0]);

	if(ptr == NULL) {
		*head_ref = (struct bmnode*)malloc(sizeof(struct bmnode));
		new_node->key = 0;
		strcpy(new_node->book, bookm);
		new_node->next = NULL;
		*head_ref = new_node;
	} else {
		while(ptr->next != NULL){
			ptr = ptr->next;
		}
		new_node->key = ptr->key + 1;
		strcpy(new_node->book, bookm);
		new_node->next = NULL;
		ptr->next = new_node;
	}
	
	/*int i=0;
	if(ptr != NULL) {
		i++;
		//printf("Second iteration, arg is %s\n", ptr->book);
		while(ptr->next != NULL) {
			i++;
			ptr = ptr->next;
		}
	}
	
	new_node->key = i;
	printf("new node key is %d\n", new_node->key);
	new_node->next = NULL;
	new_node->book = bookm;
	printf("new node arg is %s\n", new_node->book);

	if(ptr == NULL) {
		bmhead = (struct bmnode*) malloc(sizeof(struct bmnode));
		bmhead = new_node;
		printf("head node arg is %s\n", bmhead->book);
	} else {
		ptr->next = new_node;
	}*/

	
	/*if(ptr != NULL) {
		ptr->next = new;
	}*/
}

/*int deleteBookmark(int key) {
	struct bmnode* bmcurrent = bmhead;
	struct bmnode* bmprevious = NULL;
	
	if(bmhead == NULL) return -1;
	
	while(bmcurrent->key != key) {
		if(bmcurrent->next == NULL) {
			return -1;
		} else {
			bmprevious = bmcurrent;
			bmcurrent = bmcurrent->next;
		}
	}
	
	if(bmcurrent == bmhead) {
		bmhead = bmhead->next;
	} else {
		bmprevious->next = bmcurrent->next;
	}
}*/


/*
 * Call this function by "kerem".
 * Supply two arguments.
 * 1. plane, siren, or protester
 * 2. string of max. 20 characters (continuous, no spaces)
 * Enjoy!
 */
int kerem_awesome_command(struct command_t *command) {
	if(command->arg_count == 2) {
		
		char sign_message[21];
		strcpy(sign_message, command->args[1]);
		
		if(strcmp(command->args[0], "plane") == 0) {
			// ASCII animation of a banner on a plane
			int left_space = ((20 - strlen(sign_message)) / 2);
			int right_space = 20 - strlen(sign_message) - left_space;
			
			printf("\n");
			int i, j;
			for(i=0; i<60; i++) {
				system("clear");
				if(i%4==0 || i%4==2) {
					for(j=0; j<i; j++) printf(" "); printf("                                                   -:++++:.              \n");
                                        for(j=0; j<i; j++) printf(" "); printf("                                    /y/         `oo:.   `./oo`           \n");
                                        for(j=0; j<i; j++) printf(" "); printf("                                    o+-oo-     .h.          -h`          \n");
                                        for(j=0; j<i; j++) printf(" "); printf("OXXXXXXXXXXXXXXXXXXXXXXXXO          o+   /h    h:            +s-`        \n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          o+-oo+:----h:            +h/+ss.     \n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          ods        .h`          .ms   `h/  sM\n");
					for(j=0; j<i; j++) printf(" "); printf("X  ");
					for(j=0; j<left_space; j++) printf(" "); printf("%s", sign_message); for(j=0; j<right_space; j++) printf(" ");
					printf("  X==========oh          `oo:`    .:oo/s     m- yM\n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          o+             -/++++/-  :s     ydhmM\n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          :s                       :s     d- sM\n");
					for(j=0; j<i; j++) printf(" "); printf("OXXXXXXXXXXXXXXXXXXXXXXXXO           y/                      :s    oy  sM\n");
					for(j=0; j<i; j++) printf(" "); printf("                                      +s/.                   :s`-+y/     \n");
					for(j=0; j<i; j++) printf(" "); printf("                                        l:++++++++++++++++++++o+:/       \n");
				}
				if(i%4==1) {
					for(j=0; j<i; j++) printf(" "); printf("                                                   -:++++:.              \n");
					for(j=0; j<i; j++) printf(" "); printf("                                    /y/         `oo:.   `./oo`           \n");
					for(j=0; j<i; j++) printf(" "); printf("                                    o+-oo-     .h.          -h`          \n");
					for(j=0; j<i; j++) printf(" "); printf("OXXXXXXXXXXXXXXXXXXXXXXXXO          o+   /h    h:            +s-`        \n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          o+-oo+:----h:            +h/+ss.   sM\n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          ods        .h`          .ms   `h/  sM\n");
					for(j=0; j<i; j++) printf(" "); printf("X  ");
					for(j=0; j<left_space; j++) printf(" "); printf("%s", sign_message); for(j=0; j<right_space; j++) printf(" ");
					printf("  X==========oh          `oo:`    .:oo/s     m- yM\n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          o+             -/++++/-  :s     ydhmM\n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          :s                       :s     d- sM\n");
					for(j=0; j<i; j++) printf(" "); printf("OXXXXXXXXXXXXXXXXXXXXXXXXO           y/                      :s    oy  sM\n");
					for(j=0; j<i; j++) printf(" "); printf("                                      +s/.                   :s`-+y/   sM\n");
					for(j=0; j<i; j++) printf(" "); printf("                                        l:++++++++++++++++++++o+:/       \n");
				
				}
				if(i%4==3) {
					for(j=0; j<i; j++) printf(" "); printf("                                                   -:++++:.              \n");
					for(j=0; j<i; j++) printf(" "); printf("                                    /y/         `oo:.   `./oo`           \n");
					for(j=0; j<i; j++) printf(" "); printf("                                    o+-oo-     .h.          -h`          \n");
					for(j=0; j<i; j++) printf(" "); printf("OXXXXXXXXXXXXXXXXXXXXXXXXO          o+   /h    h:            +s-`        \n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          o+-oo+:----h:            +h/+ss.     \n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          ods        .h`          .ms   `h/    \n");
					for(j=0; j<i; j++) printf(" "); printf("X  ");
					for(j=0; j<left_space; j++) printf(" "); printf("%s", sign_message); for(j=0; j<right_space; j++) printf(" ");
					printf("  X==========oh          `oo:`    .:oo/s     m- yM\n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          o+             -/++++/-  :s     ydhmM\n");
					for(j=0; j<i; j++) printf(" "); printf("X                        X          :s                       :s     d- sM\n");
					for(j=0; j<i; j++) printf(" "); printf("OXXXXXXXXXXXXXXXXXXXXXXXXO           y/                      :s    oy    \n");
					for(j=0; j<i; j++) printf(" "); printf("                                      +s/.                   :s`-+y/     \n");
					for(j=0; j<i; j++) printf(" "); printf("                                        l:++++++++++++++++++++o+:/       \n");
				
				}
				system("sleep .125");
			}
			return 1;
		}
	
		if(strcmp(command->args[0], "siren") == 0) {
			// ASCII animation of a one-line police siren
			printf("\n");
			int i;
			for(i=0; i<60; i++) {
				system("clear");
				if(i%4==0) printf(".uU    %s    Uu.\n\n", sign_message);
				if(i%4==1 || i%4==3) printf("uUu    %s    uUu\n\n", sign_message);
				if(i%4==2) printf("Uu.    %s    .uU\n\n", sign_message);
				
				system("sleep .125");
			}
			printf("\n");
			return 1;
		}
		
		if(strcmp(command->args[0], "protester") == 0) {
			// ASCII animation of a protester holding a sign
			int left_space = ((20 - strlen(sign_message)) / 2);
			int right_space = 20 - strlen(sign_message) - left_space;
			
			printf("\n");
			int i, j;
			for(i=0; i<16; i++) {
				system("clear");
				if(i%4==0 || i%4==2) {
					printf("    -+osssssssssssssssssssssssssso+-    \n  -do-`                          `-od-  \n");
					printf(" `N:      "); for(j=0; j<left_space; j++) printf(" ");
					printf("%s", sign_message);
					for(j=0; j<right_space; j++) printf(" "); printf("      :N` \n");
					printf("  mo                                od  \n  `sho+//////////////////////////+ohs`  \n    sN                            Ns     \n     +m-       `oo+::+oo`       -m+      \n      :N:     -h        h-     :N:       \n       -N+    y   0  0   y    +N-        \n        .ms   +s        s+   sm.         \n         `hy   +s      s+   yh`          \n           o:    -++++-    :o            \n");
				}
				if(i%4==1) {
					printf("     -+osssssssssssssssssssssssssso+-    \n   -do-`                          `-od-  \n");
					printf("  `N:      "); for(j=0; j<left_space; j++) printf(" ");
					printf("%s", sign_message);
					for(j=0; j<right_space; j++) printf(" "); printf("      :N` \n");
					printf("   mo                                od  \n   `sho+//////////////////////////+ohs`  \n     sN                            Ns     \n      +m-      `oo+::+oo`        -m+      \n       :N:    -h        h-      :N:       \n        -N+   y    0  0  y     +N-        \n         .ms  +s        s+    sm.         \n          `hy  +s      s+    yh`          \n            o:   -++++-     :o            \n");
				}
				if(i%4==3) {
					printf("   -+osssssssssssssssssssssssssso+-    \n -do-`                          `-od-  \n");
					printf("`N:      "); for(j=0; j<left_space; j++) printf(" ");
					printf("%s", sign_message);
					for(j=0; j<right_space; j++) printf(" "); printf("      :N` \n");
					printf(" mo                                od  \n `sho+//////////////////////////+ohs`  \n   sN                            Ns     \n    +m-        `oo+::+oo`      -m+      \n     :N:      -h        h-    :N:       \n      -N+     y  0  0    y   +N-        \n       .ms    +s        s+  sm.         \n        `hy    +s      s+  yh`          \n          o:     -++++-   :o            \n");
				}
				
				system("sleep .5");
			}
			printf("\n");
			return 1;
		}
	}
	
	else {
		perror("Non-permitted use of Kerem's Awesome Command.");
		return -1;
	}
}

/* Call this command by "beyza"
 * Specify 1 argument: seconds
 * That's it! You set a timer to warn you.
 * When it's time to eat a muffin, the system will warn you
 * with a cute song
 */
int beyza_awesome_command(struct command_t *command) {
	if(command->arg_count != 1) {
		perror("The user did not specify enough argument to set the timer!\n");
		return -1;
	}

	int seconds = 0;
	seconds = atoi(command->args[0]);
	
	char music[256];
	strcpy(music, "rhythmbox-client --play-uri=");
	char cwd[256];
	getcwd(cwd, sizeof(cwd));

	strcat(music, cwd);
	strcat(music, "/The_Muffin_Song.mp3");
	printf("%s\n", music); 
	
	time_t t;
	struct tm *tm;

	tm = localtime(&t);
	int s = seconds;
	
	while(seconds > 0) {
		tm->tm_sec = s;
		tm->tm_min = s/60;
		tm->tm_hour = s/3600;

		mktime(tm);

		printf("%02d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
		s--;
		seconds--;
		waitSec(1);
	}
	
	system(music);
	
	return SUCCESS;

}	

void waitSec(int sec) {
	clock_t ending = clock() + (sec * CLOCKS_PER_SEC);
	while (clock() < ending) {}
}

int pstraverse(struct command_t *command) {
	if(command->arg_count !=2) {
		perror("The user did not enter the arguments correctly. Please provide a PID and search method -d or -b.\n");
		return -1;	
	}

	char* mod_dfs[] = {"dfs_module.ko"};
	char* mod_bfs[] = {"bfs_module.ko"};

	if(strcmp(command->args[1], "-d") == 0) {
		execvp("sudo insmod",mod_dfs);
		execvp("sudo rmmod", mod_dfs);
	} else if (strcmp(command->args[1], "-b") == 0) {
		execvp("sudo insmod",mod_bfs);
		execvp("sudo insmod",mod_bfs);
	}

	return SUCCESS;
}
