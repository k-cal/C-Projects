# C-Projects

For the most part, these can be compiled with gcc without too much trouble. Just gcc [source] -o [output]

#####adventure
A short text game that involves travelling through different rooms to find an exit room.

The primary purpose of this was to get practice with file I/O, in preparation for subsequent projects in the course.

It's named `adventure.startendmod.c` because it's actually a slight modification on what I turned in. The primary difference is an extra line or two that prevents the starting room from connecting to the ending room.

#####smallsh
A simple shell with a few built-in commands. Aside from running programs (with backgrounding supported by ending a line with &) the built-ins are `exit`, `cd`, and `status`. The purpose of these built-ins is probably fairly clear for the most part.

`exit` exits the shell, `cd` changes directories, and `status` returns the last exit code from the last terminated program (or termination signal, if applicable).

Piping input to and from a file are also supported with "<" and ">".

There's a small extra experiment with pipes (as in between processes) in here as well, because pipes aren't something we covered in too much detail during regular coursework. It's not something that is noticeable from regular use, however. A pipe is used for a minor message between the shell and its forked child behind the scenes.
