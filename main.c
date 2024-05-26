#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <time.h>
#include <fcntl.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARG_NUM 64
#define MAX_ALIASES 64
#define COMMAND_FILE "old_commands.txt"
#define MAX_COMMAND_LENGTH 1024

int background_processes[MAX_ARG_NUM];  // Array to store background process IDs
int background_count = 0;               // Counter to keep track of background processes

void reverse_string(char *str) { // Function to reverse a string in place
    int n = strlen(str);
    for (int i = 0; i < n / 2; i++) {
        char temp = str[i];
        str[i] = str[n - i - 1];
        str[n - i - 1] = temp;
    }
}
void check_background_processes() { // Function to check and handle terminated background processes
    int i = 0;
    while (i < background_count) {
        int status;
        pid_t result = waitpid(background_processes[i], &status, WNOHANG);
        if (result > 0) { // If the background process has terminated, remove it from the list
            for (int j = i; j < background_count - 1; j++) {
                background_processes[j] = background_processes[j + 1];
            }
            background_count--;
        } else { // If the process is still running, move to the next process
            i++;
        }
    }
}
void add_command(const char *command) { // Function to add a command to a log file
    FILE *file = fopen(COMMAND_FILE, "a");
    if (file != NULL) {
        fprintf(file, "%s\n", command);
        fclose(file);
    } else {
        printf("Error opening log file.\n");
    }
}
char* get_last_command() { // Function to retrieve the last command from a log file
    static char prev_command[MAX_COMMAND_LENGTH];
    FILE *file = fopen(COMMAND_FILE, "r");
    if (file != NULL) {
        char current_command[MAX_COMMAND_LENGTH];
        char last_command[MAX_COMMAND_LENGTH];
        last_command[0] = '\0'; // Empty string
        while (fgets(current_command, sizeof(current_command), file) != NULL) {
            strcpy(prev_command, last_command); // Store the second-to-last command since the last one is bello if we call bello to get last command
            strcpy(last_command, current_command);
        }
        fclose(file);
        size_t len = strlen(prev_command);
        if (len > 0 && prev_command[len - 1] == '\n') {
            prev_command[len - 1] = '\0';
        }
        return strdup(prev_command);
    } else {
        printf("Error opening log file.\n");
    }
    return NULL;
}
void delete_last_command() { // Function to delete the last command (not executed) from a log file
    FILE *file = fopen(COMMAND_FILE, "r");
    if (file != NULL) {
        char lines[1000][MAX_COMMAND_LENGTH];
        char current_line[MAX_COMMAND_LENGTH];
        int line_count = 0;

        while (fgets(current_line, sizeof(current_line), file) != NULL && line_count < 1000) {
            strcpy(lines[line_count++], current_line);
        }
        fclose(file);

        if (line_count > 0) {
            file = fopen(COMMAND_FILE, "w");
            for (int i = 0; i < line_count - 1; i++) { // Rewrite all lines except the last one
                fputs(lines[i], file);
            }
            fclose(file);
        } else {
            printf("No commands.\n");
        }
    } else {
        printf("Error opening log file.\n");
    }
}
void display_user_info() {
    char *username = getenv("USER");
    printf("Username: %s\n", username);

    char hostname[1024];
    gethostname(hostname, sizeof(hostname));
    printf("Hostname: %s\n", hostname);

    char *last_command = get_last_command(); // Retrieve the last executed command from the log file
    if (last_command != NULL) {
        printf("Last Executed Command: %s\n", last_command);
    } else {
        printf("Last Executed Command: No previous command logged\n");
    }

    char *tty = ttyname(STDIN_FILENO);
    printf("TTY: %s\n", tty);

    char *shell = getenv("SHELL");
    printf("Current Shell Name: %s\n", shell);

    char *home = getenv("HOME");
    printf("Home Location: %s\n", home);

    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    printf("Current Time and Date: %s", asctime(timeinfo));

    check_background_processes(); // Check and display the number of background processes being executed
    printf("Current number of processes being executed: %d\n", background_count);
}


struct Alias {
    char name[MAX_INPUT_SIZE];
    char command[MAX_INPUT_SIZE];
};

struct Alias aliases[MAX_ALIASES];
int alias_count = 0;

void load_aliases() {
    FILE *file = fopen("aliases.txt", "r");
    if (file != NULL) {
        // Read aliases from the file and store them in an array
        while (fscanf(file, "%s = \"%[^\"]\"\n", aliases[alias_count].name, aliases[alias_count].command) != EOF) {
            alias_count++;
            if (alias_count >= MAX_ALIASES) {
                break;
            }
        }
        fclose(file);
    }
}

void save_aliases() {
    FILE *file = fopen("aliases.txt", "w");
    if (file != NULL) {
        for (int i = 0; i < alias_count; ++i) {
            fprintf(file, "%s = \"%s\"\n", aliases[i].name, aliases[i].command);
        }
        fclose(file);
    }
}

void display_prompt() {
    char hostname[MAX_INPUT_SIZE];
    gethostname(hostname, sizeof(hostname));
    printf("%s@%s %s --- ", getenv("USER"), hostname, getenv("PWD"));
}

void add_alias(char *alias_name, char *command) {
    if (alias_count < MAX_ALIASES) {
        // Store the new alias in the aliases array and save to file
        strcpy(aliases[alias_count].name, alias_name);
        strcpy(aliases[alias_count].command, command);
        alias_count++;
        save_aliases(); // Save the aliases after adding a new one
    } else {
        printf("Maximum number of aliases reached.\n");
    }
}

char* get_alias_command(char *alias_name) { // Function to get the command associated with an alias
    char* temp;
    int last_alias = 0;
    for (int i = 0; i < alias_count; ++i) {
        if (strcmp(aliases[i].name, alias_name) == 0) {
            temp= aliases[i].command;
            last_alias = 1;
        }
    }
    if(last_alias){
        return temp;
    }
    return NULL;
}

int main() {
    load_aliases(); // Load aliases when the shell starts

    char input[MAX_INPUT_SIZE];
    char *args[MAX_ARG_NUM];
    int should_run = 1;

    while (should_run) {
        display_prompt();
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {         // Read user input
            break;
        }
        if (input[0] == '\0' || input[0] == '\n') {        // empty command doesn't terminate the shell
           continue;
        }

        int arg_count = 0;
        char *token = strtok(input, " \n");
        char *command_test = malloc(512);
        char *notquoted = malloc(512); // Buffer to store not quoted arguments
        char *temp_command = malloc(512);

        int inside_quotes = 0;
        int background = 0; // Flag to indicate background process
        int output_redirect = 0; // Flag for output redirection
        int append_redirect = 0; // Flag for appending output
        int reverse_append_redirect = 0;
        int check_alias = 0;  // Flag to check if "alias" command is encountered
        int reverse_output = 0;
        char *output_file_name; // File to redirect output

        while (token != NULL  && arg_count < MAX_ARG_NUM - 1) { 
            if (token[0] == '"' && check_alias && !inside_quotes) {  // when there is a alias, we should pay attention to first quote since it is the indication for alias command
                inside_quotes = 1; 
                if(strlen(token) == 1){ // there can be one space after first quote
                    token = strtok(NULL," \n");
                    continue;
                }
                if(token[strlen(token) - 1] == '"' && strlen(token) != 1){ //when alias' command is one command such as "ls" or "pwd"
                    inside_quotes = 0; 
                    token[strlen(token) - 1] = '\0'; 
                    arg_count++;
                }
                args[arg_count]= &token[1]; 
                strcpy(command_test, &token[1]);
            } 
            else if (inside_quotes && token[strlen(token) - 1] == '"' && check_alias) { 
                //when we find another quote,we should determine that if it is in the command or it is last quote,end of alias command,
                strcpy(temp_command,token); //temp token is the current token since we should check the other token is null or not,we should keep the old tokenés data
                token = strtok(NULL," \n");
                if(token != NULL){ // the quote part is inside the alias' command such as  alias k = "echo "sa"",it is the middle part quote not end of command
                    args[arg_count] =(char*) malloc(256);
                    strcat(args[arg_count], temp_command);
                    strcat(command_test," ");
                    strcat(command_test,temp_command);
                    args[arg_count][strlen(args[arg_count])] = '\0'; //we don't increase args count since the alias command should one of the args element
                    continue;
                }
                //it is end of alias command
                inside_quotes = 0; 
                temp_command[strlen(temp_command) - 1] = '\0'; 
                args[arg_count] =(char*) malloc(256);
                strcat(command_test," ");
                strcat(command_test,temp_command);
                args[arg_count][strlen(args[arg_count])] = '\0';
                arg_count++; 
                break; 

            } else if (inside_quotes) { // it is the element inside the alias command
                args[arg_count] =(char*) malloc(256);
                strcat(args[arg_count], token);
                strcat(command_test," ");
                strcat(command_test,token);
                args[arg_count][strlen(args[arg_count])] = '\0'; 
            }
            else {
                if(strcmp(token,"alias")== 0){
                    check_alias = 1;
                }
                if (strcmp(token, "&") == 0) {
                    background = 1; // Set background flag
                    break; // Stop parsing further arguments
                } else if (strcmp(token, ">") == 0) {
                    output_redirect = 1; // Set output redirection flag
                    strcat(notquoted,token);
                    strcat(notquoted," ");
                    token = strtok(NULL, " \n"); // Get the next token as the output file
                    if (token != NULL) {
                        output_file_name = token;
                    } else {
                        fprintf(stderr, "Output file not specified.\n");
                        break;
                    }
                    strcat(notquoted,token);
                    strcat(notquoted," ");
                    token = strtok(NULL, " \n"); // Get the next token as the output file
                    if(token != NULL && strcmp(token,"&") == 0){
                        background = 1; // Set background flag
                        strcat(notquoted,token);
                        break; // Stop parsing further arguments
                    }

                } else if (strcmp(token, ">>") == 0) {
                    append_redirect = 1; // Set append output redirection flag
                    strcat(notquoted,token);
                    strcat(notquoted," ");

                    token = strtok(NULL, " \n"); // Get the next token as the output file
                    if (token != NULL) {
                        output_file_name = token;
                    } else {
                        fprintf(stderr, "Output file not specified.\n");
                        break;
                    }

                    strcat(notquoted,token);
                    strcat(notquoted," ");
                    token = strtok(NULL, " \n"); // Get the next token to check background
                    if(token != NULL && strcmp(token,"&") == 0){
                        background = 1; // Set background flag
                        strcat(notquoted,token);
                        break; // Stop parsing further arguments
                    }
                }else if (strcmp(token, ">>>") == 0) {
                    reverse_append_redirect = 1; // Set reverse append output redirection flag
                    strcat(notquoted,token);
                    strcat(notquoted," ");
                    token = strtok(NULL, " \n"); // Get the next token as the output file
                    if (token != NULL) {
                        output_file_name = token;
                    } else {
                        fprintf(stderr, "Output file not specified.\n");
                        break;
                    }
                    strcat(notquoted,token);
                    strcat(notquoted," ");
                    token = strtok(NULL, " \n"); // Get the next token to check background
                    if(token != NULL && strcmp(token,"&") == 0){
                        background = 1; // Set background flag
                        strcat(notquoted,token);
                        break; // Stop parsing further arguments
                    }
                }
                if(!append_redirect && !output_redirect && !reverse_append_redirect){
                    args[arg_count] =(char*) malloc(256);
                    args[arg_count++] =token; // Regular token
                    if(get_alias_command(token) == NULL){  // the token can be alias command
                        strcat(notquoted,token);
                        strcat(notquoted," ");
                    }
                }
            }
            token = strtok(NULL, " \n");
        }
        args[arg_count] = NULL;
        notquoted[strlen(notquoted) - 1] = '\0'; 

        if (strcmp(args[0], "exit") == 0) {
            should_run = 0;
            continue;
        }
        if (strcmp(args[0], "alias") == 0 && arg_count >= 3 && strcmp(args[2], "=") == 0) {
            char alias_name[MAX_INPUT_SIZE];
            char command[MAX_INPUT_SIZE];
            strcpy(alias_name, args[1]);
            strcpy(command, command_test);
            add_alias(alias_name, command);
            continue;
        }
        char* alias_command = get_alias_command(args[0]); // Check if the entered command is an alias
        if (alias_command != NULL) {
            strcpy(input, alias_command);
            strcat(input," ");
            strcat(input,notquoted); //we should add the part outside the quote 
            add_command(input);

            arg_count = 0;
            inside_quotes = 0;
            output_redirect = 0; // Flag for output redirection
            append_redirect = 0; // Flag for appending output
            reverse_append_redirect = 0;
            check_alias = 0;
            reverse_output = 0;
            char* token = strtok(input, " \n");
            //same parsing from the upper part.It is not necessary to add the first 3 if parts, but I added them in case alias comes again.
            while (token != NULL && arg_count < MAX_ARG_NUM - 1) {
                if (token[0] == '"' &&check_alias) {
                    inside_quotes = 1; 
                    if(token[strlen(token) - 1] == '"'){
                        inside_quotes = 0; 
                        token[strlen(token) - 1] = '\0'; 
                        arg_count++;
                    }
                    args[arg_count]= &token[1]; 
                    strcpy(command_test, &token[1]);

                } else if (inside_quotes && token[strlen(token) - 1] == '"' &&check_alias) {
                    inside_quotes = 0; 
                    token[strlen(token) - 1] = '\0'; 
                    args[arg_count] =(char*) malloc(256);
                    strcat(command_test," ");
                    strcat(command_test,token);
                    args[arg_count][strlen(args[arg_count])] = '\0';
                    arg_count++;
                    break; 

                } else if (inside_quotes) {
                    args[arg_count] =(char*) malloc(256);
                    strcat(args[arg_count], token);
                    strcat(command_test," ");
                    strcat(command_test,token);
                    args[arg_count][strlen(args[arg_count])] = '\0'; 

                } else {
                    if (strcmp(token, "&") == 0) {
                        background = 1; // Set background flag
                        break; // Stop parsing further arguments
                    } else if (strcmp(token, ">") == 0) {
                        output_redirect = 1; // Set output redirection flag
                        token = strtok(NULL, " \n"); // Get the next token as the output file
                        if (token != NULL) {
                            output_file_name = token;
                        } else {
                            fprintf(stderr, "Output file not specified.\n");
                            break;
                        }
                        token = strtok(NULL, " \n"); // Get the next token to check background
                        if(token != NULL && strcmp(token,"&") == 0){
                            background = 1; // Set background flag
                            break; // Stop parsing further arguments
                        }
                    } else if (strcmp(token, ">>") == 0) {
                        append_redirect = 1; // Set append output redirection flag
                        token = strtok(NULL, " \n"); // Get the next token to check background
                        if (token != NULL) {
                            output_file_name = token;
                        } else {
                            fprintf(stderr, "Output file not specified.\n");
                            break;
                        }
                        token = strtok(NULL, " \n"); // Get the next token as the output file
                        if(token != NULL && strcmp(token,"&") == 0){
                            background = 1; // Set background flag
                            break; // Stop parsing further arguments
                        }
                    }
                    else if (strcmp(token, ">>>") == 0) {
                        reverse_append_redirect = 1; // Set reverse append output redirection flag
                        token = strtok(NULL, " \n"); // Get the next token as the output file
                        if (token != NULL) {
                            output_file_name = token;
                        } else {
                            fprintf(stderr, "Output file not specified.\n");
                            break;
                        }
                        token = strtok(NULL, " \n"); // Get the next token to check background
                        if(token != NULL && strcmp(token,"&") == 0){
                            background = 1; // Set background flag
                            break; // Stop parsing further arguments
                        }
                    }
                    if(!append_redirect && !output_redirect && !reverse_append_redirect){
                        args[arg_count] =(char*) malloc(256);
                        args[arg_count++] =token; // Regular token
                        if(get_alias_command(token) == NULL){
                            strcat(notquoted,token);
                            strcat(notquoted," ");
                        }
                    }
                }
                token = strtok(NULL, " \n");
            }
            args[arg_count] = NULL;
        }else {
            add_command(args[0]);
        }

        // File descriptor for output redirection
        int file_desc;
        if (output_redirect || append_redirect || reverse_append_redirect) {
            if (append_redirect) {
                file_desc = open(output_file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else if (reverse_append_redirect) {
                file_desc = open(output_file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
                reverse_output = 1; // Set reverse output flag
            } else {
                file_desc = open(output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if (file_desc < 0) {
                perror("File opening error");
                return 1;
            }
        }

        // Pipe creation for inter-process communication
        int pipe_fd[2];
        if (pipe(pipe_fd) < 0) {
            perror("Pipe creation failed");
            exit(EXIT_FAILURE);
        }
        char *path = getenv("PATH");

        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Handling output redirection or reverse append redirection in child process
            if (output_redirect || append_redirect || (reverse_append_redirect && !background)) {
                if (reverse_output) {
                    close(pipe_fd[0]); // Close the read end of the pipe
                    dup2(pipe_fd[1],STDOUT_FILENO); // Redirect standard output (STDOUT) to the write end of the pipe
                    close(pipe_fd[1]); // Close the write end of the pipe after redirection
                }
                else{
                    dup2(file_desc, STDOUT_FILENO);// Redirect standard output (STDOUT) to the specified file descriptor
                    close(file_desc); // Close the file descriptor after redirection
                }
            }
            // Check if it's a reverse append redirection and running in the background
            if (reverse_append_redirect && background) {
                int reverse_pipe[2];
                pipe(reverse_pipe);

                // Forking a grandchild process for reverse append redirection
                pid_t reverse_pid = fork();

                if (reverse_pid == -1) {
                    perror("Fork failed");
                    exit(EXIT_FAILURE);
                } else if (reverse_pid == 0) { // Inside the grandchild process
                    close(reverse_pipe[0]);
                    dup2(reverse_pipe[1], STDOUT_FILENO); // Redirect standard output (STDOUT) to the write end of the pipe
                    close(reverse_pipe[1]);

                    if (strcmp(args[0], "bello") == 0){
                        check_background_processes();
                        display_user_info();
                        exit(EXIT_SUCCESS);
                    }                    
                    execvp(args[0], args); // I use execvp in this part,the other exec part is with path.
                    perror("Execution failed");
                    exit(EXIT_FAILURE);
                }
                else { // Parent of the grandchild process  waits and handles output redirection
                    int status;
                    waitpid(reverse_pid, &status, 0);  // Wait for the grandchild process to finish
                    close(reverse_pipe[1]);  // Close the write end in the parent

                    char buffer[4096];
                    ssize_t bg_num_bytes = read(reverse_pipe[0], buffer, sizeof(buffer) - 1);
                    
                    if (bg_num_bytes > 0) {
                        buffer[bg_num_bytes] = '\0';
                        reverse_string(buffer); // Reverse the content read from the pipe

                        // Open the output file for reverse append redirection
                        int reverse_file = open(output_file_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
                        if (reverse_file == -1) {
                            perror("open");
                            exit(EXIT_FAILURE);
                        }

                        // Write the reversed content to the file
                        write(reverse_file, buffer, strlen(buffer));
                        close(reverse_file);
                    }
                    close(reverse_pipe[0]);  // Close the read end in the parent
                    exit(EXIT_SUCCESS);
                }
            }
            if (strcmp(args[0], "bello") == 0) {
                check_background_processes();
                display_user_info();
                exit(EXIT_SUCCESS);
            }
            // Execution of commands from PATH environment variable
            //execvp(args[0], args);
            if (path != NULL) {
                char temp_path[strlen(path) + 1];
                strcpy(temp_path, path);

                char *dir = strtok(temp_path, ":"); // Tokenizing the PATH variable by ":"

                while (dir != NULL) {
                    // Constructing the full path for the command to be executed
                    char command_path[strlen(dir) + strlen("/") + strlen(args[0]) + 1];
                    char result[10000] = {0};

                    // Creating the full path by concatenating directory and command
                    snprintf(result, sizeof(result), "%s/%s", dir, args[0]);
                    sprintf(command_path, result, dir);
                    
                    // Checking if the constructed command path is executable
                    if (access(command_path, X_OK) != -1) {
                        if (execv(command_path, args) == -1) {    
                        return EXIT_FAILURE;
                        }
                    }
                    dir = strtok(NULL, ":");
                } 
                delete_last_command(); // if command reaches here,it is not executable so it should not be in command_log

                if (errno == ENOENT) {
                    fprintf(stderr, "Command not found: %s\n", args[0]);
                } else {
                    perror("Command execution failed");
                }
                exit(EXIT_FAILURE);        
            }
        } else {
            if (reverse_append_redirect && background){ // Adding the process to the background list and continuing for background execution
                background_processes[background_count++] = pid;
                check_background_processes(); // Check and handle terminated
                continue;
            }
            if (!background) {
                // Wait for foreground process to complete
                waitpid(pid, NULL, 0);
            }
            else {
                // Add the background process to the list
                background_processes[background_count++] = pid;
            }
            // Handling reverse output redirection in the parent process
            if(reverse_output){

                // Close the write end of the pipe in the parent
                close(pipe_fd[1]);
                char buffer[MAX_INPUT_SIZE];
                ssize_t num_bytes = read(pipe_fd[0], buffer, sizeof(buffer)-1);
                if(num_bytes > 0){
                    buffer[num_bytes]  = '\0';
                    reverse_string(buffer); // Reverse the content read from the pipe
                    write(file_desc,buffer,strlen(buffer));
                    close(file_desc);
                }
                close(pipe_fd[0]); // Close the read end of the pipe
            }
            check_background_processes(); // Check and handle terminated
        }
    }

    return 0;
}

