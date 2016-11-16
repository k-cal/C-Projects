//Albert Chang
//CS344-400
//Assignment 3 - smallsh
//2016/7/22

//note on fprintf:
//I originally had all errors to stderr, and all regular output to stdout.
//But then I figured, since the shell is kind of where everything is meant to be seen anyway,
//I changed all of the fprintf to send to stdout instead of stderr. It doesn't make much
//difference for grading, but just something I was thinking about.

//I've added helpful messages telling the user that cd only takes one argument,
//and exit and status take none. This doesn't prevent them from working, but just provides
//a friendly hint (while still executing the built-in normally, ignoring extraneous arguments).

//I also added a message about inputs after &, since the & must be at the end of the line.
//My input handler stops reading immediately after it encounters the &, so I make sure to tell
//the user that any subsequent input is not taken into consideration.

//2016-7-31 edit: I changed some (not all) of the fprintf to use stderr again.
//I used a somewhat subjective assessment of what's an error and what's a message to the user.

#include <stdlib.h>    //I'm not even sure what's in stdlib and stdio anymore.
#include <stdio.h>     //I just know I always end up needing it anyway.
#include <string.h>    //For getline, pretty sure fgets isn't as good for the full-line experience.
                       //Also some other string functions I think, like strtok.
#include <signal.h>    //Pretty sure I need this for signal action.
#include <unistd.h>    //For some system calls (dup2, fork, maybe others).
#include <fcntl.h>     //For open and its flags (also some pipe action I added later).
#include <sys/types.h> //I think I added this for size_t. getline didn't work with regular integers.

//Seems important, so getting this done early.
#define MAX_LINE 2048
#define MAX_ARG 512

//Arbitrary numbers that I like for pipe file descriptors.
#define INPIPE  27
#define OUTPIPE 36
//I had some fun figuring out how FD_CLOEXEC works, and now I have pipes.
//It's to fix an issue that's not required for the assignment, but it should fix it correctly.
//The grading script never tests for invalid executable names in running background processes.
//The old method (without pipes) would simply print "background process is pid" after the fork.
//But this doesn't take into account the possibility of a failed execution in the background.
//The new method uses a pipe to send a basic signal (meaning a simple message), and if the signal
//is sent (or any message at all) it means the exec failed (the signal is only attempted after
//failure) and the parent shell knows to print out "yourExecutable: no such file or directory".


//everythingStruct (a struct so big it gets a header like a function)
//This is exactly what it sounds like. A struct to handle everything that comes up.
//Think of it like the main "working state" of the shell.
//It's going to be passed all over. Kind of like globals, so somewhat poor form.
//But the good thing about keeping everything in the struct is that the "globals"
//are only exposed when I specifically want to access it. No variable name pollution from true globals.
//Some fixed-size arrays that aren't for arguments use MAX_ARG size,
//There's no significance to this. Just thought 512 was a good maximum.
//A thought: tokenBuffer is like argv, and we could easily add an int argc.
//           Not sure if we need argc yet, but if we do, then it makes sense 
//           to rename tokenBuffer to argv. For consistency.
//           (I added argc later, but argv is separate from tokenBuffer now)
struct everythingStruct {
    char * userBuffer; //the user's line
    char * tokenBuffer[MAX_ARG]; //lines need to become separate words later
    
    int argc;             //I said I'd rename tokenBuffer to argv if I decided to use argc,
    char * argv[MAX_ARG]; //but I changed my mind. So now argv is its own thing.
    
    char * inputRedirect; //we can assign tokens to these later as necessary
    char * outputRedirect;
    
    int attemptIR; //It just occurred to me that malformed command lines might lead
    int attemptOR; //to weird behavior, so these flags help me keep track of that.
    
    int background; //not sure how to use this yet, but makes sense to have a flag
    int forkpid; //moved this into everythingStruct as well because of some extra separation of functions

    char statusMsg[MAX_ARG]; //feel like we'll be needing to keep track of this somehow
    int status; //waitpid needs an int for the status number, and here it is
    
    int children[MAX_ARG]; //to keep track of rowdy child processes
    int childCount; //to aid in keeping track of those rowdy children
    //idea is to update array of background processes for mass termination later,
    //but kill signals sent early might mean we need some type of management
    
    int exitFlag; //this used to be in main, but main no more
    
    int smallpipe[2]; //just something I want to test out
    char pipeStatus[MAX_ARG]; //only going to use a few characters so this is way too huge
    int pipeRead;
    
    struct sigaction normalSA; //These are in the struct so they can be easily passed to functions.
    struct sigaction ignoreSA; //But the functions could easily use local sigaction as well.
};
//That was a pretty long header for a struct. Longer than my function headers.
//I actually started the basic input gathering (playing around with getline and strtok)
//before creating this struct. Then I thought it would be convenient to have all the
//data I need for shell operations ready at all times, and I moved the buffers into
//the struct, along with any other variables I've created along the way.

//Now that I'm finished, I'd say that it wouldn't be too hard to separate out the contents
//of the struct into smaller structs that are focused on specific tasks. But while that would
//look nicer (and keep things in their own little compartments), it doesn't actually do much
//for the code's logic. I'll leave it as is for now.

//Off the top of my head, I'd say:
//userBuffer, tokenBuffer, argv, argc would go in one struct (one of argv and tokenBuffer could be dropped entirely)
//inputRedirect, outputRedirect, attemptIR, attemptOR, background, forkpid, statusMsg, status in a second one
//smallpipe, pipeStatus, pipeRead in a third (perhaps as a member of the second struct)
//children, childCount in a fourth
//The two sigaction in a fifth (or simply dropped entirely as a struct,
//                              since they could easily be localised to the functions they're used in)
//exitFlag could go back to being a variable in main (simply passed into tokenParser as a pointer to integer)


//Prototypes here
void inputHandler(struct everythingStruct *); //This gets a line from the user.
void tokenHandler(struct everythingStruct *); //This makes a line into tokens.
void tokenParser(struct everythingStruct *); //This reads the tokens.
void executor(struct everythingStruct *); //This executes outside stuff.
void backgroundCheck(struct everythingStruct *); //This checks if background children finished.
  //Remember not to change status for this one.
  
void overseer(struct everythingStruct *);
//This function is out of order because it was just moved out of the tokenParser.

void initialInit(struct everythingStruct *);
void loopInit(struct everythingStruct *);
//Same goes for these two, out of main.

int arrayChecker(int * array, int size, int test);
//This is just a small helper I made for backgroundCheck.

//Shells don't take arguments, right? Not until runtime at least.
int main() {
  int index;
  
  struct everythingStruct shellState;

  //Some initialisation to start with.
  initialInit(&shellState);
  
  do {
    //Some initialisation is done within the loop.
    loopInit(&shellState);
    
    //check for background here, following Ben's hint
    backgroundCheck(&shellState);
  
    inputHandler(&shellState);
    tokenHandler(&shellState);
         
    //Only do stuff if not blank and not comment
    if (!(shellState.tokenBuffer[0] == 0 || shellState.tokenBuffer[0][0] == '#')) {
      tokenParser(&shellState);
    }

  } while (!shellState.exitFlag);
  
  //Not sure if SIGTERM is better than SIGKILL
  //Sticking with SIGTERM for now, since Ben's been hinting at it being a better idea.
  for (index = 0; index < shellState.childCount; index++) {
    fprintf(stdout, "sending SIGTERM to pid %d\n", shellState.children[index]);
    fflush(stdout);
    kill(shellState.children[index], SIGTERM);
  }
  
  free(shellState.userBuffer);
  //tokenBuffer's pointers points to parts of userBuffer,
  //so we shouldn't need to free it (same goes for argv)
  //(same goes for other pointers too, but I'm not going to list all of them here)

  return 0;
}


//initialInit
//Just a few lines moved out from main.
//shellState: the new everythingStruct to be populated with everything
//Post-condition: Some initial settings are set for the shellState
void initialInit(struct everythingStruct * shellState) {
  int index;
  
  shellState->userBuffer = 0;
  shellState->childCount = 0;
  shellState->exitFlag = 0;
  
  shellState->ignoreSA.sa_handler = SIG_IGN;
  sigaction(SIGINT, &(shellState->ignoreSA), &(shellState->normalSA) );
  
  shellState->smallpipe[0] = INPIPE;
  shellState->smallpipe[1] = OUTPIPE;
  shellState->pipeRead = 0;
      
  for (index = 0; index < MAX_ARG; index++) {
    shellState->statusMsg[index] = '\0'; //I like nulling out character arrays for initialisation.
    shellState->pipeStatus[index] = '\0';
    shellState->children[index] = 0;
    shellState->tokenBuffer[index] = 0;
    shellState->argv[index] = 0;
  }
  //Maybe this default status message is too cheeky. Just maybe.
  strcpy(shellState->statusMsg, "no exit status yet, try running something");
}


//loopInit
//Just a few more lines moved out of main
//shellState: the everythingStruct that is to be re-initialised (for user input)
//Post-condition: shellState is ready to take user input again (and then get parsed by functions)
void loopInit(struct everythingStruct * shellState) {
    shellState->inputRedirect = 0;
    shellState->outputRedirect = 0;
    shellState->attemptIR = 0;
    shellState->attemptOR = 0;
    shellState->background = 0;
    shellState->argc = 0;
}


//inputHandler
//Kind of a misnomer. This gets input, but doesn't really handle it in any meaningful way.
//It's just grabbed and returned.
//I added the everythingStruct a little bit late into development, so the logic here might seem
//strange at times. Many of these functions used to return something instead of being void.
//shellState: every function gets this, it's all the data we could possibly need in one place
//post-condition: user's line should be read into userBuffer inside shellState
void inputHandler(struct everythingStruct * shellState) {
  int bytesRead;
  size_t max_line = MAX_LINE; //Need a pointer to this for getline.
  
  //I guess getline must use the heap? 0 by default to show unused-ness.
  //This comment doesn't make as much sense now since I introduced the everythingStruct.
  //Just know that userBuffer was originally a local variable here.
  
  //Using fprintf because why not. fprintf to stdout is the same as just printf though.
  fprintf(stdout, ": ");
  fflush(stdout);
  bytesRead = getline(&(shellState->userBuffer), &max_line, stdin);
  //No asserts this time, because we're in a shell. Feels inappropriate.
  if (shellState->userBuffer == 0) {
    fprintf(stderr, "ERROR: couldn't get input with getline\n");
    fflush(stderr);
    exit(1);
  }
  
  //userBuffer ends with a newline apparently. (getline includes the newline at the end of the string)
  shellState->userBuffer[bytesRead - 1] = '\0'; //This should take care of it.
}


//tokenHandler
//Again a misnomer. This splits the input into tokens, but doesn't handle it much.
//shellState: the everything of everything here
//Pre-condition: userBuffer needs to be valid
//Post-condition: userBuffer (from shellState) is parsed into tokens in tokenBuffer (in shellState).
//                argc (a counter in shellState) is properly added to as well.
void tokenHandler(struct everythingStruct * shellState) {

  int tokenIndex = 0;
  
  int counterCounter = 0; //I find this variable name funny.
                          //The purpose will be explained later.
  
  char * token;
  
  //Pretty sure we only accept spaces as delimiters.
  token = strtok(shellState->userBuffer, " ");
  while (token != 0) {
    shellState->tokenBuffer[tokenIndex] = token;
    
    tokenIndex++;
    
    if (token != 0 //Can't be attempting to access null tokens.
     && counterCounter == 0 //No need to continue this check if we've already set the flag.
     && (strcmp(token, "<") == 0
      || strcmp(token, ">") == 0
      || strcmp(token, "&") == 0) ) {
      //Time to explain. argc is the "argument counter". It goes up by 1
      //for each token added, to make things a bit simpler later for exec.
      //But if we hit a <, >, or &, we know we're done with command line arguments.
      //At least for exec. The redirection is still a command line argument,
      //but one that will be handled by this shell. So argc stops increasing.
      //counterCounter is a boolean flag that counters counters.
      counterCounter = 1;
      
      //This & check occurs before others because it requires more prior knowledge to handle.
      //From the perspective of the shell, that is.
      if (strcmp(token, "&") == 0) {
        shellState->background = 1;
        token = strtok(0, " ");
        break; //Since "&" must be the last word, we don't have to keep reading tokens.
      }        //We still attempt one more read though.
    }
    
    if (!counterCounter) { //Alternative name idea: counterBreaker
    shellState->argc++;
    }
    
    token = strtok(0, " ");
  }
  
  if (token != 0) {
    fprintf(stdout, "note: & must be final word on line, so any input after & is ignored\n");
    fflush(stdout);
  }
  
  shellState->tokenBuffer[tokenIndex] = 0; //a null pointer
  //Just in case, so we don't try to read past the useful end of the array.
}


//tokenParser
//Finally, a function that is named in a reasonable way. Relatively speaking.
//This function handles built-ins (and forking), but execution is handled by
//the next function in the chain.
//shellState: same as before, everything's here
//Pre-condition: tokenBuffer needs to be valid
//Post-condition: Either a built-in is run, or a fork happens and there's slight prep
//                for execution of an outside executable file.
//                "Slight prep" is mainly handling wait conditions.
void tokenParser(struct everythingStruct * shellState) {
  //check for exit first, simplest command
  if (strcmp(shellState->tokenBuffer[0], "exit") == 0) {
    shellState->exitFlag = 1; //Set the exit flag.
    if (shellState->argc > 1) {
      fprintf(stdout, "note: exit doesn't take arguments (arguments ignored)\n");
      fflush(stdout);
    }
  }
  else if (strcmp(shellState->tokenBuffer[0], "cd") == 0) {
    //This works because we set a 0 pointer at the end of tokenBuffer.
    if (shellState->tokenBuffer[1]) {
      //The "if" is only for an error, which is unusual but better than having a "continue;"
      if (!(chdir(shellState->tokenBuffer[1]) == 0) ) {
        fprintf(stderr, "%s: no such file or directory\n", shellState->tokenBuffer[1]);
        fflush(stderr);
      }
      else if (shellState->argc > 2) {
        fprintf(stdout, "note: cd only takes one argument (arguments after first ignored)\n");
        fflush(stdout);
      }
    }
    else { 
      //If tokenBuffer[1] doesn't exist, it means cd had no arguments.
      //If cd had no arguments, then we're headed home, like ET.
      chdir(getenv("HOME") );
    }
  }
  else if (strcmp(shellState->tokenBuffer[0], "status") == 0) {
    fprintf(stdout, "%s\n", shellState->statusMsg);
    fflush(stdout);
    if (shellState->argc > 1) {
      fprintf(stdout, "note: status doesn't take arguments (arguments ignored)\n");
      fflush(stdout);
    }
  }
  else {
    if (shellState->background) { //pipe if we're backgrounding
      if (pipe(shellState->smallpipe) == -1) {
        //I only ever use perror for pipe errors. Maybe inconsistent.
        //But I don't know how to best describe pipe errors.
        //I don't have much experience using them, aside from in this project.
        perror("Piping Failed");
        exit(1);
      }
      //I thought about making the pipe non-blocking, but then decided against it.
      //Blocking helps to prevent weird race conditions, so it's a good thing.
      // if (fcntl(shellState->smallpipe[0], F_SETFL, O_NONBLOCK) == -1) {
        // perror("Failed to unblock pipe");
        // exit(1);
      // }
    }
    shellState->forkpid = fork();
    
    if (shellState->forkpid == 0) { //Process execution is handled elsewhere.
      if (shellState->background) {
        close(shellState->smallpipe[0]);
        if (fcntl(shellState->smallpipe[1], F_SETFD, FD_CLOEXEC) == -1) {
          perror("Failed to set close-on-exec flag");
          exit(1);
        }
      }
      executor(shellState);
    }
    else if (shellState->forkpid < 0) { //The error state.
      perror("Failure to Fork");
      exit(1);
    }
    else { //For the parent. To be honest, this part should probably be a separate function as well.
      if (shellState->background) {
        close(shellState->smallpipe[1]);
      }
      overseer(shellState);
    }      //Just moved it to a separate function. Hope nothing goes wrong.
  }
}


//overseer
//Just moving a little bit of tokenParser into its own function.
//The overseer watches over the executor.
//So this is just what the original parent does while the child executes something.
//shellState: same as always
//Pre-condition: The forked child of the shell is attempting to execute something
//Post-condition: The parent shell either waits (for foreground) or continues as usual
//                (for background) with proper changes to the children array.
void overseer(struct everythingStruct * shellState) {
  if (shellState->background) {
    //Check to see if the child shell has anything to report. If so, it means it failed to exec.
    //Reading up to MAX_ARG because it seems like a good limit to me. (512 bytes)
    //The child only ever writes 5 bytes.
    shellState->pipeRead = read(shellState->smallpipe[0], shellState->pipeStatus, MAX_ARG);
    //(If not, this means the child shell got killed by exec. RIP)
    if (shellState->pipeRead <= 0) {
      shellState->children[shellState->childCount] = shellState->forkpid;
      shellState->childCount++;
      fprintf(stdout, "background pid is %d\n", shellState->forkpid);
      fflush(stdout);
    }
    else {
      fprintf(stderr, "%s: no such file or directory\n", shellState->tokenBuffer[0]);
      fflush(stderr);
    }
    close(shellState->smallpipe[0]); //Close the pipe because we're done.
  }
  else { //The non-background code is here.
    waitpid(shellState->forkpid, &(shellState->status), 0);
    
    if (WIFEXITED(shellState->status) ) {
      sprintf(shellState->statusMsg, "exit value %d", WEXITSTATUS(shellState->status) );
    }
    //Not sure how signaling works yet, but I can get this message in at least.
    //(It ended up working as expected after I figured out how to use sigaction.)
    //(The fprintf was added afterwards, if not clear.)
    if (WIFSIGNALED(shellState->status) ) {
      sprintf(shellState->statusMsg, "terminated by signal %d", WTERMSIG(shellState->status) );
      fprintf(stdout, "\n%s\n", shellState->statusMsg);
      fflush(stdout);
    }
  }
}

//executor
//Sounds morbid, doesn't it? This function kills files so that their spirits can be
//harvested as processes. But the files still exist after, for re-execution.
//shellState: contains everything we might need
//Pre-condition: tokenBuffer needs to be valid (and the first token needs to not be
//               be a built-in). argc should be valid too.
//Post-condition: An executable is run (with possible arguments and redirection) or
//                if not found, this smallsh process (which is a forked clone) simply
//                spits out an error and dies by the executor.
void executor(struct everythingStruct * shellState) {
  //Make sure this child process can be interrupted properly.
  sigaction(SIGINT, &(shellState->normalSA), 0);
  
  char * token; //This is going to look really familiar.
  int argvIndex = 0;
  int cancel = 0; //A flag that can be used to call off execution.
  
  while (argvIndex < shellState->argc) {
    shellState->argv[argvIndex] = shellState->tokenBuffer[argvIndex];
    argvIndex++;
  }
  shellState->argv[argvIndex] == 0; //Can't forget that ending null.
  //That should fill argv up for us.
  //But we're not done yet.
  
  token = shellState->tokenBuffer[argvIndex];
  
  //Figuring out special symbols first.
  while (token != 0) {
    if (strcmp(token, "&") == 0) {
      break; //The background symbol must be last in line, so we're done.
    }
    //It occurred to me after writing this that this next part would have made more sense inside tokenHandler.
    //(I think I added the counterCounter in tokenHandler after writing this part.)
    if (strcmp(token, "<") == 0) {
      argvIndex++;
      shellState->attemptIR = 1;
      shellState->inputRedirect = shellState->tokenBuffer[argvIndex];
    }
    if (strcmp(token, ">") == 0) {
      argvIndex++;
      shellState->attemptOR = 1;
      shellState->outputRedirect = shellState->tokenBuffer[argvIndex];
    }
    argvIndex++;
    token = shellState->tokenBuffer[argvIndex];
  }
  
  //Two ways of using these, so we can just declare them out here.
  int inputFD;
  int outputFD;
  
  if (shellState->attemptIR) {
    inputFD = open(shellState->inputRedirect, O_RDONLY);
    //Handling the dup2 inside a conditional, maybe weird.
    if ( (inputFD < 0) || (dup2(inputFD, 0) < 0) ) {
      fprintf(stderr, "cannot open %s for input\n", shellState->inputRedirect);
      fflush(stderr);
      exit(1);
    }
  }
  
  if (shellState->attemptOR) {
    outputFD = open(shellState->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    //Feels kind of weird to be using flags instead of just "0644", but the reference
    //page I was reading for the "O_" flags also had "S_" flags to try out.
    //Left off user-execute, not sure about best practices here.
    if ( (outputFD < 0) || (dup2(outputFD, 1) < 0) ) {
      fprintf(stderr, "cannot open %s for input\n", shellState->inputRedirect);
      fflush(stderr);
      exit(1);
    }
  }
  
  //Background checks just deal with sigaction and force some redirects if necessary.
  if (shellState->background) {
    //If running a background process, we prevent SIGINT yet again. Some roundabout logic here.
    sigaction(SIGINT, &(shellState->ignoreSA), &(shellState->normalSA) );
    
    if (!shellState->attemptIR) { //We force the IR to dev/null.
      inputFD = open("/dev/null", O_RDONLY);
      //Handling the dup2 inside a conditional, maybe weird.
      if (dup2(inputFD, 0) < 0) {
        fprintf(stderr, "cannot open /dev/null/ for input\n");
        fflush(stderr);
        exit(1);
      }
    }
    
    if (!shellState->attemptOR) { //We force the OR to dev/null.
      outputFD = open("/dev/null", O_WRONLY);
      //Still handling the dup2 inside a conditional.
      if (dup2(outputFD, 1) < 0) {
        fprintf(stderr, "cannot open /dev/null/ for output\n");
        fflush(stderr);
        exit(1);
      }
    }
  }
  
  execvp(shellState->tokenBuffer[0], shellState->argv);
  
  
  //If we're still here, the exec failed.
  if (shellState->background) {                 //Let the parent know we failed to execute from the background.
    write(shellState->smallpipe[1], "bail", 5); //Write 5 bytes, if I'm thinking right. "b a i l \0"
    close(shellState->smallpipe[1]);            //Close the pipe because we're done.
    //I'm kind of assuming the pipe can hold at least 512 bytes. Probably a safe assumption?
    //Though I only write 5 bytes, so it might not be an issue.
    //You might recall that on the parent side, the contents of the pipe aren't even checked.
    //All that's checked is whether anything at all was written.
  }
  else { //If in the foreground, go ahead and print the error here.
    fprintf(stderr, "%s: no such file or directory\n", shellState->tokenBuffer[0]);
    fflush(stderr);
  }
  exit(1);
  //I used tokenBuffer[0] instead of argv[0] here just to change things up.
  //Their first values should be the same, of course.
}


//backgroundCheck
//Pretty much written to match the instructor's suggestion, a function that runs before
//input is gathered each time through the main loop to check for background
//processes finishing and continuing to the after-afterlife.
//(Remember, processes have already been executed and are running in the afterlife.)
//shellState: actually not used much this time, just an update on the children array
//Pre-condition: The children array should be valid (the count too).
//Post-condition: If any background processes finished their work (or got otherwise
//                terminated) a message is printed and the shellState's children array
//                is properly updated to reflect this.
void backgroundCheck(struct everythingStruct * shellState) {
  int piddler;
  int status;
  
  //These next two are for after the check.
  int index;
  int termindex;
  
  //Could go through the children array, but -1 should get all children.
  
  piddler = waitpid(-1, &status, WNOHANG);
  
  //This while helps to catch multiple kids finishing up.
  while (piddler > 0) {
  
    termindex = arrayChecker(shellState->children, shellState->childCount, piddler);
    
    if (termindex >= 0) {
      if (WIFEXITED(status) ) {
        fprintf(stdout, "background pid %d is done: exit value %d\n", piddler,
                WEXITSTATUS(status) );
        fflush(stdout);
      }
      //Not sure how sending signals to background processes work, but the message is here already.
      //(Like the previous attempt, this ended up working once I figured out how to use kill.)
      if (WIFSIGNALED(status) ) {
        fprintf(stdout, "background pid %d is done: terminated by signal %d\n", piddler,
                WTERMSIG(status) );
        fflush(stdout);
      }
      
      //Should we clean up the children array? Not sure this matters, but we'll do it anyway.
      
      for (index = termindex; index < shellState->childCount - 1; index++) {
        shellState->children[index] = shellState->children[index + 1];
      }
      shellState->childCount--;
    }
    
    piddler = waitpid(-1, &status, WNOHANG);
  }
}


//arrayChecker
//First function in this whole file that doesn't use shellState.
//Just a simple search through an unsorted array.
//array: integer array
//size: size of that array
//test: integer to look for
//Returns the index of the test integer if found, -1 if not found.
int arrayChecker(int * array, int size, int test) {
  int index;
  for (index = 0; index < size; index++) {
    if (array[index] == test) {
      return index;
    }
  }
  return -1;
}