Albert Chang
CS372-400 Fall 2016
Project 2
FTP client/server
A client/server pair for a simple FTP program.
Usage description will come later (in this readme).



BUILD INSTRUCTIONS:
You can use gcc to compile the server:
"gcc server.c -o ftserve"
or type "make ftserve" because I've included a Makefile with a single target.

The Makefile's single target uses gcc with the exact line I stated above.

The client script, "ftclient.py", is just a script that can be run like any
other python script, so there's nothing to build.

Note that the ftserve executable and the ftclient.py should be placed in
DIFFERENT working directories if running on the same host. Otherwise you'd be
attempting to overwrite files with themselves which would be counterproductive.



USAGE INSTRUCTIONS:
Make sure you've followed the build instructions above before continuing.

Note that ALL port numbers must be in the range of 1025-65535. These programs
do not allow the use of reserved ports.

Remember, you should have the client and server in different directories if
you're running both on the same host. Something like:
    WORKING_DIRECTORY/server/ftserve
    WORKING_DIRECTORY/client/ftclient.py
You don't need this specific directory structure, but what matters is that the
client and server are in different directories.

Inside the "server" directory, run
    "ftserve CONTROL_PORT"
where CONTROL_PORT is your preferred port number for the server to listen on.


Before we get to the client commands, here's a short terminology listing for
arguments that can be used for the client:
HOSTNAME: The IPv4 address of the server (either dotted decimal or 
          actual registered hostname that can be resolved via DNS).
          
CONTROL_PORT: The same port number the server is listening on.

DATA_PORT: A port number that the server will use to connect to the client.
          (This port will be opened by the client, not the server.)
          
          Keep in mind that if the client and server are running on the same
          host, CONTROL_PORT and DATA_PORT must be different ports. You can't
          have two sockets listening on the same port from the same host at
          one time. That would make them the same socket.
          
FILENAME: A filename.

DIRECTORY: A directory name.


From the "client" directory, (on the same or a different host, but it should
at least be on a different terminal to avoid confusing output), you can run
the client with one of the following commands:

FIRST COMMAND: a directory listing
    "python ftclient.py HOSTNAME CONTROL_PORT -l DATA_PORT"

    
SECOND COMMAND: request a file (a "get" command)
    "python ftclient.py flip2.engr.oregonstate.edu CONTROL_PORT -g FILENAME DATA_PORT"

Invalid names will be met by error messages from the server.

With the get command "-g", if the file already exists you will be prompted
whether you wish to overwrite the existing file. At this prompt, type "yes" or
"y" to continue with overwriting the file. (Uppercase is fine too.) Typing
anything else will be treated as a "no", for safety concerns. Safety here is in
the sense that anything that's not explicit consent is taken to be dissent.
("Dissent" may not be the best word here, but it matches well with "consent".)


THIRD COMMAND: change the server's working directory
    "python ftclient.py flip2.engr.oregonstate.edu CONTROL_PORT -cd DIRECTORY"

Do not include a DATA_PORT when using the "-cd" command.
Success or failure will be reported to the client by the server.


FOURTH COMMAND: a bad command (for debugging)
    "python ftclient.py flip2.engr.oregonstate.edu CONTROL_PORT -badcommand DATA_PORT"

The fourth command does nothing useful, but can be used to verify the server
properly responds to bad commands.  t's a bad command that the client
intentionally allows through validation. Even though the DATA_PORT will
never be used, something must still be entered. This command is mostly
treated as a "-l" or "-cd" for validation purposes, even though
it's invalid and will only ever elicit error messages from the server.



ADDITIONAL NOTES:
Though I was adamant about the server and client being placed in different
directories, the existence of the "-cd" command means it's possible to
navigate back to the same directory as the client and attempt to overwrite
files with themselves. Please don't do that, because I'm fairly certain it
will be harmful to your data and not very useful.

Formatting of the directory listing is along the lines of:
<file>	filename1
<file>	filename2
<DIR>	directoryname
Directories are preceded with a <DIR> prefix and files with a <file> prefix.
One is uppercase and the other is lower to set them apart better visually.
The listing is otherwise fairly sparse. I added these labels after adding the
"-cd" command, because it could help the user to figure out when to use "-g"
and when to use "-cd" compared to a label-less listing.