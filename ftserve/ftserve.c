//Albert Chang
//CS372-400 Fall 2016
//Project 2
//FTP server
//A server for a simple FTP program.
//The server listens on a port (specified on command line).
//Clients can request directory listing and files.

//EXTRA CREDIT(S)
//Though I wrote the program intending only to support text files,
//I've tested with a randomly generated file of 512 MiB (as well as various smaller
//files up to about 10 MiB that aren't randomly generated) and found this works without error.
//As such, I'm making claim to that extra-credit point suggested in the instructions.

//I also added directory changing. It's fairly easy, though I made it slightly more
//difficult for myself by choosing to use pipes to relay information from forked children.

//Filenames with spaces are supported. More info in the README and client comments.

//Like last time, I also still have basic checks for connection termination.
//They're not particularly hard, but since they weren't worth points last
//time, they're probably not worth points this time. If you want to test it out,
//send an interrupt signal to the client or server while transferring a large
//file (I tested with 512 MiB files.) If the client is interrupted, the server
//will state that sending the file failed. If the server is interrupted, the
//client will state that the connection with the server was lost.


//INITIAL SOURCES
//These sources guide much of my knowledge on sockets from CS344.

//Beej's guide is always a huge help for socket programming.
//http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
//I won't link it every time I use it for reference, but I'll mention it when I do.

//Something that I may have forgotten to mention in Project 1 is this source:
//http://www.linuxhowtos.org/C_C++/socket.htm
//This was a reference in CS344 and much of how I initiate connections is taken
//from the sample code there. You can see this particularly in how I name the 
//"cliLen", "cliAddr", and "servAddr" variables, though my preference for camelcase
//differs from the source's use of underscores. My all lowercase names for various
//socket fd variables like "sockfd" is also based on the conventions in that source.


//ACTUAL CODE STARTS AROUND HERE

//You can play around with this value if you want to change the maximum number of clients
//that can be serviced simultaneously. I'm not sure it's really helpful for this program.
#define MAX_CONNECTIONS 5

//You can play around with this value to change the maximum amount of a file loaded into memory
//at once. This will also be the maximum number of characters that can exist in the directory 
//listing, so be careful not to make it too low. 1024 seems like a good absolute minimum.
#define MAX_CHUNK 1024

//Some arbitrary fd numbers for a pipe. They can be changed too.
#define INPIPE 27
#define OUTPIPE 36


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ctype.h> //for isdigit (even though we've already made this the hard way in MASM)

#include <sys/socket.h> //sockets
#include <netinet/in.h> //inet

#include <fcntl.h>  //fd and related functions
#include <unistd.h> //close, fork
//I recall programs working without unistd.h in CS344, but that seems like bad practice.

#include <sys/wait.h> //zombie stuff (not sure we need it, but we used it in CS344)
#include <dirent.h> //directory listing

#include <netdb.h> //Not sure I need this now, but here it is again anyway.
//I recall needing it for something back in CS344, but I really can't recall what that is now.

#include <sys/stat.h> //Found useful code for quickly getting filesize.
//http://stackoverflow.com/questions/8236/how-do-you-determine-the-size-of-a-file-in-c
//This comment and source was originally from a CS344 program.


//Prototypes. Everybody loves prototypes.

//These functions do most of the heavy lifting for making the server work.
int initialSetup(char** commandArgs);
void serverLoop(char** commandArgs, int controlsockfd);
void commandParser(char ** commandArgs, int newcontrolsockfd, char * clientAddress, char * clientInput, int * smallpipe);
void listDirectory(char ** commandArgs, int newcontrolsockfd, char * clientAddress, int dataPort);
void attemptSend(char ** commandArgs, int newcontrolsockfd, char * filename, char * clientAddress, int dataPort);

//James Bond would call this function Q.
//This is a helper as well, but I wanted to call attention to the name so it gets its own line.
int quartermaster(char ** commandArgs, char * clientAddress, int dataPort);

//Some small helpers.
int filesize(char *filename);
int isDirectory(char *filename);
void sendClientError(int newcontrolsockfd, char * errorMessage);
void tokenHelper(char * tokenBuffer[], char * inputString);



int main(int argc, char** argv) {
    //Validate input (just the port, because there's no other input to the server)
    if (argc < 2) {
        fprintf(stderr, "usage: %s <listening_port>\n", argv[0]);
        exit(0);
    }
    
    int index = 0;
    
    while (argv[1][index] != '\0') {
        if (!isdigit(argv[1][index]) ) {
            fprintf(stderr, "usage: %s <listening_port>\n       port had non-numeric input\n", argv[0]);
            exit(0);
        }
        index++;
    }
    
    int portnum = atoi(argv[1]);
    
    //Ports are reserved up to 1024.
    if (portnum <= 1024 || portnum > 65535) {
            fprintf(stderr, "usage: %s <listening_port>\n       valid port range 1025-65535\n", argv[0]);
            exit(0);
    }
    
    //Now we actually use that information.
    int controlsockfd;
    controlsockfd = initialSetup(argv);
  
  serverLoop(argv, controlsockfd);
  
  //This is where the program never reaches.
  
  return 0;
}



//Function for initial setup of server (creating the socket for listening.)
//As with Program 1, most of this code is reused from my CS344 work.
//I can provide the old CS344 files if there's any interest.
//Parameter: takes argv, directly from main (called "commandArgs" here)
//           (validation of argv is done outside of setup)
//           All that's needed is the port number,
//           but having argv here allows fancier error messages.
//Returns: If successful, an integer socket fd for use.
int initialSetup(char** commandArgs) {  
    //We'll be returning this later.
    int sockfd;
   
    struct sockaddr_in servAddr;
    
    int optval = 1;
    
    //Get that socket ready.
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) {
        fprintf(stderr, "%s: couldn't create socket\n", commandArgs[0]);
        exit(1);
    }
    
    //Seems like a good idea to avoid EADDRINUSE
    //I'm not really sure why optval is used as a boolean and passed by address.
    //I know we do need some sort of boolean when setting flags,
    //but passing by address (with size) seems strange for a simple 0 or 1.
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) < 0) {
        fprintf(stderr, "%s: couldn't set SO_REUSEADDR\n", commandArgs[0]);
        exit(1);
    };
                                                      //As before in Program 1,
    servAddr.sin_family = AF_INET;                    //AFI_INET for IPv4
    servAddr.sin_port = htons(atoi(commandArgs[1]) ); //sin_port in proper network order
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);     //This part is different from the client code.
    //(Way back in CS344) Read about htonl for INADDR_ANY online and it seems to be unnecessary.
    //Something about good practice though.
    //INADDR_ANY should just be saying to use "any" local IP address, if I understand right.
    
    if (bind(sockfd, (struct sockaddr *)  &servAddr, sizeof(servAddr) ) < 0) {
        fprintf(stderr, "%s: couldn't bind socket\n", commandArgs[0]);
        exit(1);
    }
    
    //We're not listening here, this is just setup.
    return sockfd;
}



//Function for actually running the server.
//This doesn't need to be its own function, but it makes things easier for me to follow.
//Parameter: argv is taken again, not because it's needed, but for nicer error messages
//           controlsockfd is the same fd created by initialSetup
//Returns nothing, because this is an infinite loop.
void serverLoop(char** commandArgs, int controlsockfd) {
    int newcontrolsockfd;
    
    int pid;
    
    //Using a pipe for directory changing.
    //I could also get rid of the forking, but I like having forked children do all work.
    int smallpipe[2];
    
    socklen_t cliLen;
    struct sockaddr_in cliAddr;
      
    int received, sent, pipeRead;
    
    //You might remember I used 512 and 1024 as buffer sizes for the chat program.
    //512 is probably enough for most filenames. I think 255 or 256 is the max length.
    char clientInput[512];   //Arbitrarily big.
    char clientAddress[17];  //Dotted decimal is 16 char max. 17 is for the null terminator.
    
    char directoryName[256]; //For changing directories.
    
    //Listening could be done in the initial setup, as long as accepting is done in the loop.
    //But I still feel like listening and accepting should be relatively "near" each other.
    //For conceptual reasons.
    if (listen(controlsockfd, MAX_CONNECTIONS) < 0) {
      fprintf(stderr, "%s: the control socket couldn't listen\n", commandArgs[0]);
      exit(1);
    }
    
    //Not sure if this is necessary, but I used this in CS344.
    //Handling requests will require forking, and forking will lead to zombies
    //after children are done doing their thing.
    //Thinking about forks and execs makes me all nostaglic and confused.
    struct sigaction childAction;
    childAction.sa_handler = SIG_IGN;
    if (sigaction(SIGCHLD, &childAction, 0) < 0) {
        fprintf(stderr, "%s: couldn't change response to SIGCHLD\n", commandArgs[0]);
        exit(1);
    }
    
    fprintf(stdout, "Server open on port %s.\n\n", commandArgs[1]);
    fflush(stdout);
    
    //Pipe setup. After initial steps, so maybe out of order.
    smallpipe[0] = INPIPE;
    smallpipe[1] = OUTPIPE;
    
    
    //Infinite loop right here.
    while (1) {
        
        //Back when I first used this in CS344, I thought it was strange how sizes
        //in bytes couldn't be represented by regular integers.
        //I still think it's strange to require types like socklen_t.
        cliLen = sizeof(cliAddr);
        
        //Initialising directory name to empty string. Just in case.
        directoryName[0] = '\0';
        
        //Piping is just for cd, but we still consider execution a failure if piping doesn't succeed.
        if (pipe(smallpipe) == -1) {
            fprintf(stderr, "%s: failed to pipe\n", commandArgs[0]);
            continue;
        }
        
        //Get ready to do stuff.
        newcontrolsockfd = accept(controlsockfd, (struct sockaddr *) &cliAddr, &cliLen);
        
        if (newcontrolsockfd < 0) {
            fprintf(stderr, "%s: failed to accept incoming client control connection\n", commandArgs[0]);
            continue; //Go block some more on accept.
        }
        
        pid = fork();
        
        if (pid < 0) {
          //Forking can fail, but we ignore it and go back to accept more.
          close(newcontrolsockfd);
          fprintf(stderr, "%s: failed to fork\n", commandArgs[0]);
          continue; //Go block some more on accept.
        }
        
        //Child process does stuff.
        //Child process never uses "continue", because that would result in a hilarious fork bomb.
        //(This "fork bomb" comment is repeated from a CS344 program. I still find it hilarious.)
        else if (pid == 0) {
            //I had fun figuring out pipes in CS344.
            close(smallpipe[0]);
            
            //Getting connection info comes from reading manual pages and Beej's guide on the structs.
            //https://www.gnu.org/software/libc/manual/html_node/Host-Address-Functions.html
            //Beej's guide doesn't need to be linked all over, since we all know where it's at.
            inet_ntop(AF_INET, &(cliAddr.sin_addr), clientAddress, 17);
            fprintf(stdout, "Incoming connection from %s:%d.\n", clientAddress, ntohs(cliAddr.sin_port));
            fflush(stdout);
            
            received = recv(newcontrolsockfd, clientInput, 1024, 0);
            
            if (received < 0) {
                fprintf(stderr, "%s: error receiving client command\n", commandArgs[0]);
                exit(1);
            }
            
            //All parsing is done in a separate function.
            //Then inside that function, many things are done in sub-functions.
            commandParser(commandArgs, newcontrolsockfd, clientAddress, clientInput, smallpipe);
            
            exit(0);
        }
        
        //Back in the parent now.
        else {
            //Close the unused end of the pipe first.
            close(smallpipe[1]);
            //See if anything's coming out of the pipe.
            pipeRead = read(smallpipe[0], directoryName, 256);
            if (pipeRead > 0) {
                chdir(directoryName);
                fprintf(stdout, "Directory has been changed to '%s'.\n\n", directoryName);
                fflush(stdout);
            }
            //Close pipe after use.
            close(smallpipe[0]);
            //Parent never services clients, so the connection is closed for the parent.
            close(newcontrolsockfd);
        }
    }
}




//This is pretty much just a part of normal server activity,
//but having it as a separate function makes it easier for me to read.
//Parameter: sockfd for control messages
//           buffer with client's command
//Returns nothing, just a function that goes through certain steps.
void commandParser(char ** commandArgs, int newcontrolsockfd, char * clientAddress, char * clientInput, int * smallpipe) {
    //Reusing an idea from the CS344 shell assignment here.
    char * tokenBuffer[4];
    
    char * list = "-l";
    char * get = "-g";  
    char * cd = "-cd";
    char * okString = "OK";
    
    int index, sent;
    
    //initialise token buffer to all null pointers.
    //I like having the buffer be one larger than necessary.
    //A habit from CS344.
    for (index = 0; index < 4; index++) {
        tokenBuffer[index] = 0;
    }
    
    tokenHelper(tokenBuffer, clientInput);
    
    /*
    //Debug printing.
    index = 0;

    for (index = 0; index < 3; index++) {
    fprintf(stdout, "token %d: %s\n", index, tokenBuffer[index]);
    }
    */
    
    if (strcmp(tokenBuffer[0], cd) == 0) {
        fprintf(stdout, "Client requested to change directory to '%s'.\n", tokenBuffer[1]);
        fflush(stdout);
        if (chdir(tokenBuffer[1]) == 0) {
            //Not actually an error, but might as well use the error helper.
            //No status message, because it's printed by the parent.
            //+1 because we need that null terminator in the byte-stream.
            write(smallpipe[1], tokenBuffer[1], strlen(tokenBuffer[1]) + 1);
            close(smallpipe[1]);
            sent = send(newcontrolsockfd, okString, strlen(okString), 0);
            if (sent < 0) {
                fprintf(stderr, "%s: failed to communicate over control connection.\n\n", commandArgs[0]);
                exit(1);
            }
        }
        else {
            fprintf(stdout, "No such directory.\nSending error message through port %s.\n\n", commandArgs[1]);
            fflush(stdout);
            sendClientError(newcontrolsockfd, "No such directory");
        }
    }
    
    else if (strcmp(tokenBuffer[0], list) == 0) {
        close(smallpipe[1]);
        fprintf(stdout, "Directory listing requested on port %s.\n", tokenBuffer[1]);
        fflush(stdout);
        listDirectory(commandArgs, newcontrolsockfd, clientAddress, atoi(tokenBuffer[1]));
    }
    
    //Note about data port:
    //Port numbers are validated on client side.
    //So server assumes they're valid and just tries to use them.
    
    else if (strcmp(tokenBuffer[0], get) == 0) {
        close(smallpipe[1]);
        //Named "attemptSend" because it can fail if filename is invalid.
        fprintf(stdout, "File '%s' requested on port %s.\n", tokenBuffer[1], tokenBuffer[2]);
        fflush(stdout);
        attemptSend(commandArgs, newcontrolsockfd, tokenBuffer[1], clientAddress, atoi(tokenBuffer[2]));
    }
    
    else {
        close(smallpipe[1]);
        fprintf(stdout, "Client sent invalid command.\nSending error message through port %s.\n\n", commandArgs[1]);
        fflush(stdout);
        sendClientError(newcontrolsockfd, "Invalid command, please use -l, -g, or -cd");
    }
}



//A helper for token creation.
//Moved out from commandParser for debugging purposes.
//Left out because why not? More modularisation is always fine.
//Parameter: a buffer to hold pointers to tokens
//           a string to tokenise
//Returns nothing.
void tokenHelper(char * tokenBuffer[], char * inputString) {
    int tokenIndex = 0;
    char * token;
    
    //Only accept tabs as delimiters. This allows spaces in filenames.
    token = strtok(inputString, "\t");
    while (token != 0) {
        tokenBuffer[tokenIndex] = token;
        tokenIndex++;
        /*
        //Debug printing.
        fprintf(stdout, "token: %s\n", token);
        */
        
        //Loop is based on tokens,
        //but we want 3 tokens at most.
        //command, filename, data port
        if (tokenIndex >= 3) {
            break;
        }
        
        token = strtok(0, "\t");
    }
}


//A function to do the directory listing.
//Like before, this doesn't need to be a separate function. Just easier to read.
//Parameter: argv, just in case parameters are needed
//           sockfd for sending control messages (shouldn't be necessary actually)
//           client address for sending listing
//           client port (data port) for sending listing
//Returns nothing.
//Sources for figuring out how to use dirent header and related functions:
//https://www.lemoda.net/c/list-directory/
//https://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html
//The first source was how I originally learned about directory access,
// and the second was one I saw later and found more compact and pleasant.
void listDirectory(char ** commandArgs, int newcontrolsockfd, char * clientAddress, int dataPort) {
    //A huge buffer for big directories.
    //Pretty sure this won't fill up completely.
    char listingBuffer[MAX_CHUNK];
    //A small buffer for holding filesize as string.
    char sizeBuffer[16];
       
    //This will only ever hold "OK"
    char okBuffer[3];
    
    //Some other string literals for quick comparisons.
    char * okString = "OK";
    
    /* //No longer in use.
    char * rootDir = ".";
    char * backDir = "..";
    */
    
    //The directory manipulation was adapted from the source above, in the function header.
    DIR * directory;
    struct dirent * currentFile;
    
    int sent, received, datasockfd;
    
    //An initialisation. Usually I wouldn't bother initialising buffers because sprintf would come later.
    //But directory entries will be concatenated with strcat, so we should initialise to an empty string.
    sprintf(listingBuffer, "");
    
    //No longer using "rootDir" as alias.
    directory = opendir(".");
    
    if (directory == 0) {
        fprintf(stderr, "%s: failed to open current directory for listing\n\n", commandArgs[0]);
        sendClientError(newcontrolsockfd, "Directory IO error");
        exit(1);
    }
    
    //Loop while entries are read from the directory.
    while (currentFile = readdir(directory) ) {
        //Don't pass along dots. Commented out because of the addition of "-cd" command.
        /*
        if (strcmp(currentFile->d_name, rootDir) == 0 
         || strcmp(currentFile->d_name, backDir) == 0) {
             continue;
         }
        */
        
        //If directory, add <DIR>, otherwise add <file>.
        if (isDirectory(currentFile->d_name)) {
            strcat(listingBuffer, "<DIR>\t");
        }
        else {
            strcat(listingBuffer, "<file>\t");
        }
        //Add the name, then add a newline.
        strcat(listingBuffer, currentFile->d_name);
        strcat(listingBuffer, "\n");
    }
    
    //Removing the final newline. Looks better.
    listingBuffer[strlen(listingBuffer) - 1] = '\0';
    
    if (closedir(directory) != 0) {
        fprintf(stderr, "%s: failed to close current directory after listing\n\n", commandArgs[0]);
        sendClientError(newcontrolsockfd, "Directory IO error");
        exit(1);
    }
    
    //Rare, but it could happen.
    //(It actually shouldn't because of the existence of "." and ".." as directories.
    if (strlen(listingBuffer) == 0) {
        fprintf(stdout, "Empty directory, altering listing to notify client.\n");
        fflush(stdout);
        sprintf(listingBuffer, "---DIRECTORY IS EMPTY---");
    }
    
    //Sending an "OK" message first. (If not "OK", the client knows not to bother opening data socket.)
    //The "directory IO error" can be sent before this is reached.
    sent = send(newcontrolsockfd, okString, strlen(okString), 0);
    if (sent < 0) {
        fprintf(stderr, "%s: failed to communicate over control connection.\n\n", commandArgs[0]);
        exit(1);
    }
    
    //It may seem strange to implement this minor handshaking, but I want the server to be sure
    //that the client is ready before attempting to send any data.
    received = recv(newcontrolsockfd, okBuffer, sizeof(okBuffer), 0);
    if (received < 0 || strcmp(okBuffer, okString) != 0) {
        fprintf(stdout, "Did not receive ready message for data socket on %s:%d.\n\n", clientAddress, dataPort);
        fflush(stdout);
        exit(1);
    }
    //I "reset" the okBuffer after verification for possible reuse.
    okBuffer[0] = '\0';
    
    //Making data connection.
    datasockfd = quartermaster(commandArgs, clientAddress, dataPort);
    
    //Quick status message.
    fprintf(stdout, "Sending directory contents to %s:%d.\n", clientAddress, dataPort);
    fflush(stdout);
    
    //Saving the length of listing buffer to a string for sending first. Then sending.
    sprintf(sizeBuffer, "%d", strlen(listingBuffer));
    sent = send(newcontrolsockfd, sizeBuffer, strlen(sizeBuffer), 0);
    if (sent < 0) {
        fprintf(stderr, "%s: failed to communicate over control connection.\n\n", commandArgs[0]);
        exit(1);
    }
    
    /*
    //Debug printing. I feel like it makes sense to show what is to be sent before sending.
    //But the operation can't be cancelled, so it might not be particularly useful.
    fprintf(stdout, "sending directory structure:\n%s\n", listingBuffer);
    */
    
    //If you compare this to the attemptSend function below, you'll see that there's less
    //concern over sending all bytes here in the listing function. The basic idea is that
    //if the listing is corrupted somehow, the client can always request the listing again.
    //Relatively speaking, it's not much data. Just a short string of all the filenames
    //in the current directory concatenated together.
    sent = send(datasockfd, listingBuffer, strlen(listingBuffer), 0);
    if (sent < 0) {
        fprintf(stderr, "%s: failed to send directory listing to %s:%d.\n\n", commandArgs[0], clientAddress, dataPort);
        //Still close upon error.
        close(datasockfd);
        exit(1);
    }
    
    //I added this after getting the program working completely for grading purposes.
    //I think it makes sense for client to send a final "OK" message.
    received = recv(newcontrolsockfd, okBuffer, sizeof(okBuffer), 0);
    if (received < 0 || strcmp(okBuffer, okString) != 0) {
        fprintf(stdout, "Did not receive client success response over control connection.\n\n");
        //Note that we don't exit after this error message.
        //This is because we still need to close the socket afterwards.
    }
    else {
        fprintf(stdout, "Client has indicated successful transfer, closing data connection.\n\n");
        fflush(stdout);
    }
    
    //Close the socket after sending, because it won't be reused.
    //Client only requests one item (file or listing) per execution, so it would be closed on client's end as well.
    close(datasockfd);
}



//A function that handles sending of files.
//Same as before, doesn't need to be a function but makes things easier to follow.
//Parameter: sockfd for control messages (socket for file connection will be handled later)
//           buffer with client's desired filename (doesn't need to be valid)
//Returns nothing.
void attemptSend(char ** commandArgs, int newcontrolsockfd, char * filename, char * clientAddress, int dataPort) {
    //A small buffer for holding filesize as string.
    char sizeBuffer[16];
    //No fileBuffer (listingBuffer) this time because we'll allocate a buffer on the heap.
    
    FILE * reqFile = 0; //Have to open a file for reading (and later sending).
    
    //This will only ever hold "OK" or "XX"
    char okBuffer[3];
    
    //Some other string literals for quick comparisons.
    char * okString = "OK";
    char * xxString = "XX"; //XX is "not OK", essentially. Just wanted to keep it to two letter status codes.
    
    /* //No longer in use.
    char * rootDir = ".";
    char * backDir = "..";
    */
    
    char * fileBuffer;  //This one will be allocated to later.
    
    //Most of these integers will be useful.
    int sent, received, datasockfd, reqSize, bufferSize, currentRead, amountRead, leftToSend, totalSent;
    
    /* //No longer necessary.
    if (strcmp(filename, rootDir) == 0 || strcmp(filename, backDir) == 0) {
        fprintf(stdout, "Invalid filename specified.\nSending error message through port %s.\n\n", commandArgs[1]);
        fflush(stdout);
        sendClientError(newcontrolsockfd, "invalid filename specified");
        exit(1);
    }
    */
    
    reqFile = fopen(filename, "r"); //Read only, since we're just sending a file.
    if (reqFile == 0) {
        fprintf(stdout, "File '%s' not found or inaccessible.\nSending error message through port %s.\n\n", 
                filename, commandArgs[1]);
        fflush(stdout);
        sendClientError(newcontrolsockfd, "File not found or inaccessible");
        exit(1);
    }
    
    //A new check related to the cd command, because it makes sense.
    if (isDirectory(filename)) {
        fclose(reqFile); //Close the file pointer. We won't be using it.
        fprintf(stdout, "'%s' is a directory.\nSending error message through port %s.\n\n", 
                filename, commandArgs[1]);
        fflush(stdout);
        sendClientError(newcontrolsockfd, "Requested resource was a directory, not a file");
        exit(1);
    }
    //The g command can't be used to change directories.
    
    //This is done with a helper, see below.
    reqSize = filesize(filename);
    
    //I think it's good to know filesize beforehand.
    if (reqSize < 0) {
        fprintf(stdout, "Couldn't get filesize of file '%s'.\nSending error message through port %s.\n\n",
                filename, commandArgs[1]);
        fflush(stdout);
        sendClientError(newcontrolsockfd, "Error determining size of file");
        exit(1);
    }
    
    //Getting the filesize ready to send. (But not sending yet.)
    sprintf(sizeBuffer, "%d", reqSize);
    
    //Sending an "OK" message first. (If not "OK", the client knows not to bother opening data socket.)
    //The "directory IO error" can be sent before this is reached.
    sent = send(newcontrolsockfd, okString, strlen(okString), 0);
    if (sent < 0) {
        fprintf(stderr, "%s: failed to communicate over control connection.\n\n", commandArgs[0]);
        exit(1);
    }
    
    //It may seem strange to implement this minor handshaking, but I want the server to be sure
    //that the client is ready before attempting to send data.
    received = recv(newcontrolsockfd, okBuffer, sizeof(okBuffer), 0);
    if (strcmp(okBuffer, xxString) == 0) {
        fprintf(stdout, "Client has cancelled transfer request.\n\n");
        fflush(stdout);
        exit(1);
    }
    else if (received < 0 || strcmp(okBuffer, okString) != 0) {
        fprintf(stdout, "Did not receive ready message for data socket on %s:%d.\n\n", clientAddress, dataPort);
        fflush(stdout);
        exit(1);
    }
    
    //I "reset" the okBuffer after verification for possible reuse.
    //With the new "XX" code, it might be better to call this a status buffer and increase the size.
    okBuffer[0] = '\0';
    
    //Making data connection. Note that we'll have to close datasockfd now upon errors.
    datasockfd = quartermaster(commandArgs, clientAddress, dataPort);
    
    //Sending filesize.
    sent = send(newcontrolsockfd, sizeBuffer, strlen(sizeBuffer), 0);
    if (sent < 0) {
        fprintf(stderr, "%s: failed to communicate over control connection.\n\n", commandArgs[0]);
        close(datasockfd);
        exit(1);
    }
    
    //Getting client confirmation that it's ready to go with file writing.
    received = recv(newcontrolsockfd, okBuffer, sizeof(okBuffer), 0);
    if (received < 0 || strcmp(okBuffer, okString) != 0) {
        fprintf(stdout, "Did not receive ready message from client for writing file.\n\n");
        fflush(stdout);
        close(datasockfd);
        exit(1);
    }
    
    okBuffer[0] = '\0';
    
    //Quick status message.
    fprintf(stdout, "Sending '%s' to %s:%d.\n%s bytes.\n", filename, clientAddress, dataPort, sizeBuffer);
    fflush(stdout);
    
    //Preparing for file read and sending.
    //Maximum file buffer size can be changed on top (towards the top of this file) if desired.
    if (reqSize > MAX_CHUNK) {
        bufferSize = MAX_CHUNK;
    }
    else {
        bufferSize = reqSize;
    }
    
    fileBuffer = malloc(bufferSize * sizeof(char) );
    amountRead = 0; //Some minor initialisation
    
    //Some minor send protection (to prevent incomplete sends) is adapted from Beej's guide.
    //Specifically section 7.3, on partial sends.
    //If you compare this to the listDirectory function above, you'll see that there's much
    //more done to prevent bum sends. For a FTP program, it's important that the data being
    //sent is what's intended. Bad sends leads to bad files, which is bad all around for a
    //program that exists only to help get data from one host to another.
    //We first check if bufferSize > 0 because 0-byte files are meaningless to send.
    if (bufferSize > 0) {
        //Just looping until end-of-file.
        while (!feof(reqFile)) {
            //I've never used fread before. (I have used fgets to read newline-less files before.)
            //My method may not be the best, but it's based on reading from reference material here:
            //http://www.cplusplus.com/reference/cstdio/fread/
            currentRead = fread(fileBuffer, sizeof(char), bufferSize, reqFile);
            amountRead += currentRead;
            leftToSend = currentRead;
            totalSent = 0;
            while (totalSent < currentRead) {
                sent = send(datasockfd, fileBuffer + totalSent, leftToSend, 0);
                leftToSend -= sent;
                totalSent += sent;
                if (sent < 0) {
                    fprintf(stderr, "%s: failed to send complete file to %s:%d.\n\n", commandArgs[0], clientAddress, dataPort);
                    close(datasockfd);
                    exit(1);
                }
            }
        }
    }
    
    //Make sure the whole file was read. But it's not like the client can do much about it if server failed.
    //Only thing to do is try again. Client does get the filesize ahead of time, so it's possible for the client
    //to do a quick verification against the size. No hash to verify against though, which greatly lessens the
    //reliability of this server.
    if (amountRead < reqSize) {
        fprintf(stderr, "%s: failed to send complete file to %s:%d.\n\n", commandArgs[0], clientAddress, dataPort);
        close(datasockfd);
        exit(1);
    }
    
    //Close the file after reading.
    fclose(reqFile);
    
    //Same extra "OK" as in the listing function.
    received = recv(newcontrolsockfd, okBuffer, sizeof(okBuffer), 0);
    if (received < 0 || strcmp(okBuffer, okString) != 0) {
        fprintf(stdout, "Did not receive client success response over control connection.\n\n");
        fflush(stdout);
    }
    else {
        fprintf(stdout, "Client has indicated successful transfer, closing data connection.\n\n");
        fflush(stdout);
    }
    
    //No memory leaks.
    free(fileBuffer);
    
    //Close the socket after sending, because it won't be reused.
    //Client only requests one item (file or listing) per execution, so it would be closed on client's end as well.
    close(datasockfd);
}



//A function to return filesize. Adapted from this stackoverflow source.
//http://stackoverflow.com/questions/8236/how-do-you-determine-the-size-of-a-file-in-c
//(The source was mentioned above, but here it is again.)
//Parameter: filename, a string representing filename (in current working directory)
//Returns size of file in bytes or -1 if something goes wrong.
//(st_size is type "off_t", but I try to keep things as integers if possible.)
int filesize(char *filename) {
    struct stat tempStat;
    
    if (stat(filename, &tempStat) == 0) {
        return tempStat.st_size;
    }
    return -1;
}
//This helper function just now was one I used in CS344 as well. I simply copied it over here,
//with no change. I'm not sure it's the best way to get filesize, but it works.



//A new helper, to determine if a directory entry (dirent) is a directory.
//Here's the source of this method, from PSkocik's post:
//http://stackoverflow.com/questions/39429803/how-to-list-first-level-directories-only-in-c/39430337
//Parameter: filename string (same idea as filesize helper)
//Returns the result of the S_ISDIR macro, which should be 0 if NOT a directory.
//(The error condition is 0 as well, because errors aren't directories.)
int isDirectory(char *filename) {
    struct stat tempStat;
    
    if (stat(filename, &tempStat) == 0) {
        return S_ISDIR(tempStat.st_mode);
    }
    //An error condition. Shouldn't be reached, hopefully.
    return 0;
}
//This helper function is based off the previous filesize helper.



//A small helper for sending error messages to client upon failure. Useful, I think.
//Parameter: control socket fd, for sending error message
//           a buffer with the error message to send
//Returns nothing.
void sendClientError(int newcontrolsockfd, char * errorMessage) {
    int sent, received;
    
    char okBuffer[3];
    char * okString = "OK";
    
    //Of course, sending the error message can fail too.
    //That's just kind of disappointing, but it could happen.
    //Notice the lack of "exit" in the error conditions here.
    //Exiting will happen back in whatever part of the code called sendClientError.
    sent = send(newcontrolsockfd, errorMessage, strlen(errorMessage), 0);
    if (sent < 0) {
        fprintf(stderr, "Failed to send error message over control connection.\n\n");
    }
    else {
        //Client says "OK" as thanks for the error message.
        received = recv(newcontrolsockfd, okBuffer, sizeof(okBuffer), 0);
        if (received < 0 || strcmp(okBuffer, okString) != 0) {
            fprintf(stdout, "Did not receive confirmation for receipt of error message.\n\n");
            fflush(stdout);
        }
    }
    //The client closes first, but the server closes as well for best practices.
    close(newcontrolsockfd);
}



//The function that creates connection Q for sending data (or just directory listing.)
//Parameter: as before, takes argv as first parameter for error messages
//           second parameter is client's address
//           third parameter is the port to contact client for data connection
//Returns an integer socket fd for use.
int quartermaster(char ** commandArgs, char * clientAddress, int dataPort) {
    //This code is reused from my own chatclient.c code in the first program.
    //Of course, that code was mostly reused from my old CS344 code as well.
    int sockfd;
    struct sockaddr_in cliAddr;
    struct hostent *client;
    
    //Resolve host.
    //Note that Beej recommends against this, but I used gethostbyname in my old CS344 program.
    //This is a "don't fix what isn't broken" situation.
    client = gethostbyname(clientAddress);
    if (client == 0) {
        fprintf(stderr, "%s: could not resolve host '%s'\n\n", commandArgs[0], clientAddress);
        exit(1);
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) {
        fprintf(stderr, "%s: failed to create data socket\n\n", commandArgs[0]);
        exit(1);
    }
    
    //This next part is still reused from my old CS344 program.
    //Beej's guide regards this as the "old method" that is less useful compared to getaddrinfo().
    //But it works for me, so I'll stick with it. My main reference here was the CS344 textbook.
    //See "The Linux Programming Interface" by Michael Kerrisk, Chapters 56 to 60.
    cliAddr.sin_family = AF_INET;                         //AF_INET is for IPv4.
    cliAddr.sin_port = htons(dataPort);                   //sin_port is just the port number, with proper network order via htons.
    memcpy(&cliAddr.sin_addr, client->h_addr_list[0], 4); //Copying over the IPv4 address found by gethostbyname. 4 byte size because IPv4.
    //The memcpy works because h_addr_list is an array of in_addr structs, as mentioned in Beej's guide.
    
    if (connect(sockfd, (struct sockaddr *) &cliAddr, sizeof(cliAddr) ) < 0) {
        fprintf(stderr, "%s: failed to connect to %s on port %d\n\n", commandArgs[0], clientAddress, dataPort);
        exit(1);
    }
    
    fprintf(stdout, "Data connection established with %s:%d.\n", clientAddress, dataPort);
    fflush(stdout);
    
    return sockfd;
}
