#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int retVal = 0;

    if (cmd == NULL)
    {
        perror("do_system: cmd was NULL. Pass a valid command");
        return false;
    }

    retVal = system(cmd);

    if(retVal != 0){
        perror("Failed when calling system()");
        return false;
    }

    // printf("retVal: %d\n", retVal);



    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    // Process commands as they've come in
    // If arg 0 is full path to program to execute, let's check that
    if(command[0][0] != '/'){
        perror("do_exec: First argument was not absolute path to exectuable.");
        return false;
    }
    pid_t parentPID = getpid();
    pid_t childPID = fork();
    
    if(getpid() == parentPID){
        // printf("Parent PID: %d\n", parentPID);
        // printf("Child PID: %d\n", childPID);
    }
    
    if(childPID == 0){
        // This is the child thread
        // printf("ZANE Child calling execv with args\n");
        // for(int i = 0; i < count; i++){
            // printf("%d: %s\n", i, command[i]);
        // }
        execv(command[0], command);
    } else if (getpid() == parentPID){
        // This is the parent thread
        int status;
        // printf("Parent starting to wait on child...\n");
        
        int rc = waitpid(childPID, &status, 0);
        
        if(rc < 0){
            return false; // rc will be <0 when child exits with an error
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return true;
        }
    }



    va_end(args);

    return false;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];
/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    if(outputfile == NULL){va_end(args);
        perror("do_exec: First argument was not full path to file to write to");
        return false;
    }

    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    pid_t parentPID = getpid();
    pid_t childPID = fork();
    if(childPID == -1)
    {
        perror("Error in trying to create child fork");
        return false;
    }
    
    if(getpid() == parentPID){
        // printf("Parent PID: %d\n", parentPID);
        // printf("Child PID: %d\n", childPID);
    }
    
    if(childPID == 0){
        // This is the child thread
        // printf("ZANE Child calling execv with args\n");
        // for(int i = 0; i < count; i++){
            // printf("%d: %s\n", i, command[i]);
        // }
        if(dup2(fd, STDOUT_FILENO) < 0){
            perror("dup2 failed");
            exit(0);
        }

        close(fd);
        execv(command[0], command);

        perror("execv");
        _exit(EXIT_FAILURE);
    } else if (getpid() == parentPID){
        // This is the parent thread
        int status;
        close(fd);
                
        int rc = waitpid(childPID, &status, 0);
        
        if(rc < 0){
            va_end(args);
            return false; // rc will be <0 when child exits with an error
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            va_end(args);
            return true; //If these are good then we successfully did it
        }

        
    }

    

    va_end(args);

    return false;
}
