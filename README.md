# C-Projects

For the most part, these can be compiled with gcc without too much trouble. Just `gcc [source] -o [output]`.

I went through a phase of trying out 2-space-length indentation for some of these projects to see if it would make code more compact. I'm not sure it really helped much at all, so I'm mostly back to 4-space indents now.

#####adventure
A short text game that involves travelling through different rooms to find an exit room.

The primary purpose of this was to get practice with file I/O, in preparation for subsequent projects in the course. Room files are generated and stored to a temporary folder which is then read back in for playing the game.

#####smallsh
A simple shell with a few built-in commands. Aside from running programs (with backgrounding supported by ending a line with `&`) the built-ins are `exit`, `cd`, and `status`. The purpose of these built-ins is probably fairly clear for the most part.

`exit` exits the shell, `cd` changes directories, and `status` returns the last exit code from the last terminated program (or termination signal, if applicable).

Piping input to and from a file are also supported with `<` and `>`.

There's a small extra experiment with pipes (as in between processes) in here as well, because pipes between processes are something we hadn't covered in too much detail during regular coursework. It's not something that is noticeable from regular use, however. A pipe is used for a minor message between the shell and its forked child behind the scenes.

#####ftserve
A simple file transfer program that supports listing the current directory, changing directories, and sending files. It's meant to be used in conjunction with the ftclient.py file, and the commands `-l`, `-g`, and `-cd`. See the included README for more detailed instructions.
