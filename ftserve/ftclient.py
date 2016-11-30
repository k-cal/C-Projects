#Albert Chang
#CS372-400 Fall 2016
#Project 2
#FTP client
#A client for a simple FTP program.
#The client can contact the server to request directory listing and files.
#Usage instructions can be obtained by running the program without arguments.

import socket
import sys
import os

#As before, I'm wrapping up the entire program in a function.
def ftpclient(argv):
    #This function makes the data connection (for accepting data back from server).
    #Takes a port number for socket creation.
    #Returns the socket.
    #(This function is at the top because it's the first helper I made.)
    def quartermaster(port):
        serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        #I believe this makes sure the port will be reusable after quitting.
        #Not sure it really matters, but helps when debugging.
        serverSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        serverSocket.bind(('',port))
        serverSocket.listen(1)
        
        return serverSocket
    
    
    #Just for fun, I implemented a very basic validator for URLs.
    #Takes an URL string, and returns true or false.
    #This is very basic, and nowhere near the level of a proper URL validator.
    def basicUrlValidator(URL):
        lowerURL = URL.lower()
        lowerValid = "abcdefghijklmnopqrstuvwxyz0123456789"
        if (lowerURL[0] not in lowerValid 
         or lowerURL[len(lowerURL) - 1] not in lowerValid):
            return False
        #I originally checked for dots, but that breaks "localhost".
        #Not that we'd use "localhost", of course. That's poor form.
        return True
    
    
    #This function makes sure everything is correct. (If things are bad, the program exits.)
    #Takes the argv straight from the "main" part of the program.
    #Nothing is returned, and certain numbers are recalculated later.
    #Not the most efficient method, but for encapsulation.
    def validator(argv):
        #First check if the number of arguments is correct.
        #1-length argv is special, means the user wants usage instructions.
        if len(argv) == 1:
            print("Usage: to request directory listing:\n"
                + "         '<server address> <server port> -l <data port>'\n"
                + "       to request a specific file:\n"
                + "         '<server address> <server port> -g <filename> <data port>'\n"
                + "       to request directory change:\n"
                + "         '<server address> <server port> -cd <directory name>'\n"
                + "       <server port> and <data port> need to be valid integers\n"
                + "       in the range of 1025-65535, inclusive.")
            exit(0)
        elif len(argv) < 5:
            print("Too few arguments, exiting.\n"
                + "To get help, run this program without arguments.")
            exit(0)
        elif len(argv) > 6:
            print("Too many arguments, exiting.\n"
                + "To get help, run this program without arguments.")
            exit(0)
            
        for index in xrange(len(argv)):
            if len(argv[index]) == 0:
                print("One or more null string arguments, exiting.\n"
                    + "To get help, run this program without arguments.")
                exit(0)
            
        #Checking server address and port first.
        if not basicUrlValidator(argv[1]):
            print("Server address appears to be invalid, exiting.\n"
                + "To get help, run this program without arguments.")
            exit(0)
        try:
            serverPort = int(argv[2])
        except:
            print("Non-numeric server port specified, exiting.\n"
                 + "To get help, run this program without arguments.")
            exit(1)
        if serverPort <= 1024 or serverPort > 65535:
            print("Invalid server port specified, exiting.\n"
                 + "To get help, run this program without arguments.")
            exit(0)
            
        #Validation block for commands.
        if argv[3] == "-l":
            if len(argv) > 5:
                print("Too many arguments for command '-l', exiting.\n"
                    + "To get help, run this program without any arguments.")
                exit(0)
            #I haven't used Python exceptions in a very long time. Perhaps over a year.
            try:
                dataPort = int(argv[4])
            except:
                print("Non-numeric data port specified, exiting.\n"
                    + "To get help, run this program without any arguments.")
                exit(0)
        elif argv[3] == "-g":
            if len(argv) < 6:
                print("Data port not specified, exiting.\n"
                    + "To get help, run this program without any arguments.")
                exit(0)
            try:
                dataPort = int(argv[5])
            except:
                print("Non-numeric data port specified, exiting.\n"
                    + "To get help, run this program without any arguments.")
                exit(0)
        elif argv[3] == "-cd":
                if len(argv) > 5:
                    print("Too many arguments for command '-cd', exiting.\n"
                        + "To get help, run this program without any arguments.")
                    exit(0)
                return #Like with badcommand below, we skip data port validation.
        #This bad command is only for testing. All other invalid commands are caught by validation.
        elif argv[3] == "-badcommand":
            print("Sending bad command to server (-badcommand is a debugging command)")
            return #We skip data port validation because we just want to send "-badcommand"
        else:
            print("You appear to have entered an invalid command.\n"
                 + "To get help, run this program without any arguments.")
            exit(0)
        
        #data port validation
        if dataPort <= 1024 or dataPort > 65535:
            print("Invalid data port specified, exiting.\n"
                 + "To get help, run this program without any arguments.")
            exit(0)
    
    
    #Function to make contact with server.
    #Like the function before, it takes argv.
    #Returns a socket for control connection.
    def makeContact(argv):
        exthost = argv[1]
        serverPort = int(argv[2])
        try:
            controlSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            controlSocket.connect((exthost, serverPort))
        except:
            print("Couldn't connect to " + exthost + ":" + sys.argv[2] + ", exiting.")
            exit(0)
        
        return controlSocket
    
    
    #Function to send request to server.
    #Takes argv and the control socket.
    #Returns nothing.
    #Arguments are joined by tabs to allow for spaces in filenames.
    def makeRequest(argv, controlSocket):
        clientMessage = sys.argv[3] + "\t" + argv[4]
        if argv[3] == "-g":
            clientMessage = clientMessage + "\t" + argv[5]
        clientMessage = clientMessage + "\0"
        controlSocket.send(clientMessage)
    
    
    #Function to handle everything back from the server.
    #It makes use of helper functions though, so it's not actually doing everything.
    #Takes argv and the control socket
    #Returns nothing.
    def receiveData(argv, controlSocket):
        #First get the status back for the request. "OK", or some error message.
        statusMessage = controlSocket.recv(4096)
        
        #Only open dataSocket if everything appears fine server-side.
        if statusMessage == "OK":
            if argv[3] == "-cd":
                #Kind of simplistic, but if the change succeeded, we're done.
                print("Server directory successfully changed.")
                return
            
            elif argv[3] == "-g":
                dataPort = int(argv[5])
            else: #else is "-l"
                dataPort = int(argv[4])
            
            dataSocket = quartermaster(dataPort)
            
            #get
            if argv[3] == "-g":
                fileWriter(argv, controlSocket, dataSocket)
            
            #list
            elif argv[3] == "-l":
                directoryLister(argv, controlSocket, dataSocket)
            
            else: #This else should never be reached.
                print("Your command was invalid, so something is wrong with the server,\n"
                    + "this client, or both.")
            
            #But the client still has to stop listening on the data socket.
            dataSocket.close()
        else:
            print("Error! " + argv[1] + ":" + argv[2] + " says:\n" + statusMessage)
            controlSocket.send("OK\0")
    
    
    #A separate function for file writing. (The actual message is received in the previous function.)
    #Takes argv, the control socket, and the data socket
    #Returns nothing.
    def fileWriter(argv, controlSocket, dataSocket):
        if os.path.exists(argv[4]):
            userInput = raw_input("File already exists. Overwrite (y/n)? ")
            if not (userInput.lower() == "yes" or userInput.lower() == "y"):
                print("File will not be overwritten. Aborting.")
                controlSocket.send("XX\0")
                return
        
        #Implicit else: start the process.
        controlSocket.send("OK\0")
        dataConnectionSocket, dataAddr = dataSocket.accept()
        strMessageSize = controlSocket.recv(4096)
        messageSize = int(strMessageSize)         #Getting an integer form.
        writeFile = open(argv[4], "w")
        #Tell server we're ready to write.
        controlSocket.send("OK\0")
        print("Receiving '" + sys.argv[4] + "' from " 
            + argv[1] + " on port " + argv[5] + ".\n"
            + strMessageSize + " bytes.")
        
        #Getting data. Initialising a few variables first.
        amountWritten = 0
        dataMessage = ''
        
        #Loop to get data until file is fully written (or transmission fails).
        while amountWritten < messageSize:
            dataMessage = dataConnectionSocket.recv(4096)
            #Server may send larger or smaller chunks, but it works fine to
            #have the client attempt to grab 4096 bytes at a time.
            #Both server and client are sending/receiving until full filesize
            #has been transferred, and both are aware of the filesize.
            writeFile.write(dataMessage)
            amountWritten += len(dataMessage)
            #All the messages should add up to the full filesize.
            #If connection is lost (recv gets nothing), we exit as well.
            if len(dataMessage) == 0:
                print("Connection with server has been lost. File is incomplete.")
                controlSocket.close()
                dataConnectionSocket.close() #Other end is closed or interrupted.
                dataSocket.close()
                exit(1)
        
        writeFile.close()
        print("File transfer and writing complete.")
        #Tell server we're done.
        controlSocket.send("OK\0")
        #The server should be the one that closes data connection,
        #following assignment instructions.
        #But I still prefer to close on both ends for best practice.
        dataConnectionSocket.close()
    
    
    #I originally didn't have a function for this, but figured might as well.
    #Takes argv, the control socket, and the data socket
    #Returns nothing.
    def directoryLister(argv, controlSocket, dataSocket):
        controlSocket.send("OK\0")
        dataConnectionSocket, dataAddr = dataSocket.accept()
        strMessageSize = controlSocket.recv(4096)
        messageSize = int(strMessageSize)         #Getting an integer form.
        print("Receiving directory structure from " 
            + argv[1] + " on port " + argv[4] + ".\n"
            + strMessageSize + " bytes.")
        dataMessage = ''
        failFlag = False #Using a boolean, even though we could just compare lengths.
        
        #Attempt to receive the entire message size at once this time,
        #because there's an assumption that it's a small string.
        #The loop still continues until all data is read, of course.
        while len(dataMessage) < messageSize:
            currentMessage = dataConnectionSocket.recv(messageSize)
            dataMessage = dataMessage + currentMessage
            #Minor error checking. Will probably never come up.
            #If we recv nothing, the connection is broken.
            if len(currentMessage) == 0:
                failFlag = True
                break
        
        #Two types of headings. The error heading will probably never come up.
        if failFlag:
            print("CONNECTION WITH SERVER HAS BEEN LOST. INCOMPLETE DIRECTORY LISTING:")
        else:
            print("DIRECTORY LISTING:")
        
        print("TYPE\tNAME\n" + dataMessage)
        
        #If the connection was interrupted, we exit early after incomplete printing.
        if failFlag:
            controlSocket.close()
            dataConnectionSocket.close() #Other end is closed or interrupted.
            dataSocket.close()
            exit(1)
        
        #Tell server we're done. (But not if failFlag is set.)
        controlSocket.send("OK\0")
        #The server should be the one that closes data connection,
        #following assignment instructions.
        #But I still prefer to close on both ends for best practice.
        dataConnectionSocket.close()
    
    
    #The rest of this is the "main" part of the function and program.
    #It's very short, because everything is stuffed into functions.
    
    #First we validate.
    validator(argv)
    
    #If we're still fine, we make the connection.
    controlSocket = makeContact(argv)
    
    #If contact succeeds, we send the request.
    makeRequest(argv, controlSocket)
    
    #After sending request, we wait for data back from the server.
    receiveData(argv, controlSocket)
    
    #Of course, after getting data, we're done.
    #The controlSocket is closed outside of other helper functions.
    controlSocket.close()


#The script needs to actually run the program.
ftpclient(sys.argv)