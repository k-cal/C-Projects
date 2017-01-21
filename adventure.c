//Albert Chang
//CS344-400
//Program 2 - adventure
//2016-7-14

//There are a few places (four lines) that are commented out for use on Windows systems.
//I included these lines so that people with Windows could enjoy the game too.

#include <stdlib.h>//For rand
#include <stdio.h>  //for i/o
#include <time.h>   //for seeding
#include <unistd.h> //for pid
/* //Windows headers to replace unistd.
#include <direct.h> //_mkdir
#include <process.h> //_getpid
*/
#include <string.h> //for string functions
#include <assert.h> //I assert after malloc. I don't assert for other reasons.
#include <sys/stat.h> //for mkdir
#include <sys/types.h> //for mode_t argument of mkdir

//There's probably no reason for this to exist, but hey, I like enums.
enum ROOMTYPE {
  START_ROOM,
  END_ROOM,
  MID_ROOM
};

//To make things easier while we build up our world.
struct roomStruct {
  char name[7];
  struct roomStruct * neighbor[6];
  int neighborCount;
  int type;
};

//Note the different array here. It will be significant, maybe.
struct playRoom {
  char name[7];
  char * neighborName[6];
  int neighborCount;
  int type;
};

//I forgot to make these prototypes earlier and completely failed my first test compilation.
void initRoomHolder (struct roomStruct ** roomHolder);

void linkRooms(struct roomStruct * room1, struct roomStruct * room2);
void longSolution(struct roomStruct ** roomHolder);
void extraLinks (struct roomStruct ** roomHolder);

void assignNames(struct roomStruct ** roomHolder);

void writeFiles(struct roomStruct ** roomHolder, char * directory);

int intArrayChecker(int * intArray, int size, int test);
int roomArrayChecker(struct roomStruct ** roomArray, int size, struct roomStruct * room);

void shuffleInt(int * intArray, int size);
void shuffleRoom(struct roomStruct ** neighbor, int size);

void playGame(char * directory, char * startRoom);
void readRoom(struct playRoom * currentRoom, char * filename);
//Here's the read function, for grading reasons. (It's the last function in this file.)


int main() {
  srand(time(0)); //Seed our RNG.

  //Making the directory now, because we'll be using it more later.
  char directory[32]; //I don't expect to need anywhere near this many characters.
  //I feel like 16 wouldn't be enough, and 32 is the next power of 2. Everybody likes powers of 2.
  snprintf(directory, 128, "changal.rooms.%d", getpid() );
  mkdir(directory, 0755);
  /* // Replace the two above lines with these for Windows.
  snprintf(directory, 128, "changal.rooms.%d", _getpid() );
  _mkdir(directory);
  */
  //Do we need to give the group and other users permissions too?
  //I'm thinking the person compiling and running this will delete it after,
  //before anybody else can get at the files.
  
  //We need seven rooms only, so no reason to make something larger.
  struct roomStruct * roomHolder[7];
  initRoomHolder(roomHolder);

  roomHolder[0]->type = START_ROOM; //We're going to give random names to these rooms later and read them back from files,
  roomHolder[6]->type = END_ROOM;   //so it's fine to just choose the first and last as START and END.

  longSolution(roomHolder); //Making one possible path immediately. The scenic route.
  assignNames(roomHolder); //We assign room names now, because it will make subsequent work slightly easier.
  extraLinks(roomHolder); //This will add more links to the rooms, since we're now at a straight line.

  writeFiles(roomHolder, directory); //This is the hard part. Or maybe the reading afterwards.
                                     //EDIT: After finishing up, I'd say reading is harder than writing.
  
  //To save us a bit of unnecessary file reads later, keep the filename of the starting room.
  char startRoom[7];
  strcpy(startRoom, roomHolder[0]->name);
    
  //We don't need roomHolder and its rooms anymore. We have well-formatted files to read from.
  int index;
  for (index = 0; index < 7; index++) {
    free(roomHolder[index]);
  }
  
  //We'll read from files directly to play the game,
  //so all we need is the first file to start with.
  playGame(directory, startRoom);
  
  return 0;
}


//This was originally part of the main function, but I figured it seemed like a good candidate
//for being moved to its own function. It only does a very specific thing though.
//roomHolder is the newly created holder of roomStruct
//Post-condition: roomHolder has seven fresh MID rooms.
void initRoomHolder (struct roomStruct ** roomHolder) {
  int index;
  int jndex;

  for (index = 0; index < 7; index++) {
    roomHolder[index] = malloc(sizeof(struct roomStruct) );
    assert(roomHolder[index] != 0); //Making sure we didn't fail to allocate.
    for (jndex = 0; jndex < 6; jndex++) {
      roomHolder[index]->neighbor[jndex] = 0; //null pointers
      roomHolder[index]->neighborCount = 0;
      roomHolder[index]->type = MID_ROOM; //We'll change this later for two rooms.
    }
  }
}


//A simple function that handles room linkage.
//It doesn't check for whether a room is maxed out already (with six links)
//so that needs to be checked elswhere.
//room1 and room2 are two roomStruct
//Post-condition: room1 and room2 are linked, with their counters properly matched up.
void linkRooms(struct roomStruct * room1, struct roomStruct * room2) {
  room1->neighbor[room1->neighborCount] = room2;
  room2->neighbor[room2->neighborCount] = room1;
  room1->neighborCount++;
  room2->neighborCount++;
}


//Creates one possible solution that uses all rooms. Hopefully the player finds a shorter one,
//but I like giving the player the option to travel the world and the seven rooms.
//roomHolder needs seven formatted (with initRoomHolder) rooms
//Post-condition: There's a Hamilton path through the rooms. Just one path.
void longSolution(struct roomStruct ** roomHolder) {
  int currentIndex = 0;
  //This is not a loop index in the traditional sense. It will be jumping around.
  int reachedRooms[6] = {0, -1, -1, -1, -1, -1};
  //The START room will always be the first in this array.
  //It's only 6 in size because the last room will connect to the END room.
  //That might not make much sense now, but it will in time. Discrete math happening here.
  int reachedRoomCounter = 0;
  //This counter will be used as the loop exit condition. Note that it counts 1 less than the number of
  //indices used. That is, the count represents the last filled index.
  int neighborRoom = -1;
  int booleanInt = -1;
  //booleanInt can be 0 for false, 1 for true. -1 is an invalid initialisation just in case.
  do {
    booleanInt = 0;
    do {
      neighborRoom = (rand() % 5) + 1; //Choosing a number between 1 and 5.
      if (!intArrayChecker(reachedRooms, 6, neighborRoom) ) {
        linkRooms(roomHolder[currentIndex], roomHolder[neighborRoom]);
        //I originally wrote the code for linkRooms here (in a slightly different form).
        //Then I realised how annoying it would be to type that out every time I needed to link rooms.
        //So I gave the code its own function.
        
        reachedRoomCounter++;
        reachedRooms[reachedRoomCounter] = neighborRoom;
        //Now the new room index is added to reachedRooms and won't be used again in this longest path.
        currentIndex = neighborRoom;
        booleanInt = 1;
      }
    } while (booleanInt == 0);
  } while (reachedRoomCounter < 4); //Remember, reachedRoomCounter shows us the last filled index.
  //We should now have five rooms linked in some way.
  //The sixth must not be the last room, and the seventh is always the last room. Fairly simple.
  
  int index;
  for (index = 1; index < 6; index++) {
    if (!intArrayChecker(reachedRooms, 6, index) ) {
      linkRooms(roomHolder[currentIndex], roomHolder[index]);
      
      reachedRoomCounter++; //We actually don't need the counter anymore, but might as well update it.
      reachedRooms[reachedRoomCounter] = index;
      break; //break because we only have to add one more room, excluding the END room.
    }
  }
  
  //Remember the index of the last room is size - 1. And size is 7 here.
  linkRooms(roomHolder[reachedRooms[reachedRoomCounter]], roomHolder[6]);
  
  //Now we have a full-length solution.
  //It occurred to me after writing this function that there's no real point to this,
  //but it's too late now. (That is, with an odd number of rooms, and at least three
  //links per room, there will always be a path from START to END.)
}


//"extra" is a misnomer, but it's "extra" in the sense that these links aren't necessary for a Hamilton path.
//roomHolder is expected to already have one path
//Post-condition: There should be at least 11 edges total in the graph.
//                Shouldn't be more than 21.
//                (Note that the current implementation guarantees 12 edges.)
void extraLinks (struct roomStruct ** roomHolder) {
  int edgeCounter;
  //We need a minimum of 3x7, maximum of 6x7, for somewhere between 21 and 42 total links.
  //This includes double counting for both rooms on the sides of the link.
  //Of course, since each edge connects two rooms, we'll have an even number in the end.
  //This means it's actually somewhere between 22 and 42. (11 to 21 edges)

  edgeCounter = 6; //I bet you wonder why I adjusted the count immediately.
                   //Recall we already added a "longest path" for the best game experience.
  
  /* 
  ////Here lies the remains of my old method of randomising the number of links in a room.
  
  int minimumLinks = 11; //Maybe this could have been a preprocessor definition?
  minimumLinks += rand() % 10;
  //Note that we're counting edges, and each edge has two sides.
  
  ////It didn't lead to enough 6-connection rooms for me to be happy with the game.
  */
  //The new method ensures one 6-link room that can serve as a hub for the player.
  //It also means the player can get lucky and win immediately after reaching that room.

  int randomRoom; //We're going to be using this a lot in the upcoming loops.

  int cycler = 6; //This is an index counter. We're going to be using this in the upcoming loop.
  int minReached[7] = {0, 0, 0, 0, 0, 0, 0}; //An array to keep track of whether we have 3 per room.
  int maxReached[7] = {0, 0, 0, 0, 0, 0, 0}; //We want at least one room with six links.
  do {
    randomRoom = rand() % 7;
    if (randomRoom != cycler
        //can't link to self
     && roomHolder[cycler]->neighborCount <= 6
        //can't pass six links
     && roomHolder[randomRoom]->neighborCount <= 6 
        //other room can't pass six links
     && !roomArrayChecker(roomHolder[cycler]->neighbor, 7, roomHolder[randomRoom]) ) {
        //can't double link; no double doors here
      linkRooms(roomHolder[cycler], roomHolder[randomRoom]);
      edgeCounter++;
      
      if (roomHolder[cycler]->neighborCount >= 3) {
        minReached[cycler] = 1;
      }
      if (roomHolder[randomRoom]->neighborCount >= 3) {
        minReached[randomRoom] = 1;
      }
      
      if (roomHolder[cycler]->neighborCount == 6) {
        maxReached[cycler] = 1;
      }
      if (roomHolder[randomRoom]->neighborCount == 6) {
        maxReached[randomRoom] = 1;
      }
    }
    cycler = (cycler + 1) % 7; //Mod to loop through indices repeatedly.
    //We started at the last room because I wanted to, no special reason for it.
  } while (!intArrayChecker(maxReached, 7, 1) );
  
  //A second loop to add companions to any stragglers that failed to reach the minimum of 3
  // before that one overachiever got a full friend list.
  int index;
  for (index = 0; index < 7; index++) {
    if (!minReached[index]) {
      do {
        randomRoom = rand() % 7;
        if (randomRoom != index
            //we dropped the check for "index" link count because it hasn't even reached 3 links.
         && roomHolder[randomRoom]->neighborCount <= 6 
         && !roomArrayChecker(roomHolder[index]->neighbor, 7, roomHolder[randomRoom]) ) {
          linkRooms(roomHolder[index], roomHolder[randomRoom]);
          edgeCounter++;
          
          if (roomHolder[index]->neighborCount >= 3) {
            minReached[index] = 1;
          }
          if (roomHolder[randomRoom]->neighborCount >= 3) {
            minReached[randomRoom] = 1;
          }
        }
      } while (!minReached[index]);
    }
    //Might as well handle the link shuffling here.
    shuffleRoom(roomHolder[index]->neighbor, roomHolder[index]->neighborCount);
  }
}


//Simply shuffles the ten names and assigns seven of them to the rooms.
//roomHolder doesn't need to be initialised here, but it's expected to be
//Post-condition: Each room has a name. Shuffled from a list of ten.
void assignNames(struct roomStruct ** roomHolder) {
  //I feel like this would be more useful as a global variable,
  //but "no globals" is a good habit.
  char *nameList[10];
  nameList[0] = "phreak";
  nameList[1] = "phrexi";
  nameList[2] = "pseudo";
  nameList[3] = "psuede";
  nameList[4] = "carrot";
  nameList[5] = "dantea";
  nameList[6] = "dainty";
  nameList[7] = "porque";
  nameList[8] = "bakone";
  nameList[9] = "carnot";

  int nameIndices[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  shuffleInt(nameIndices, 10);
  
  int index;
  
  for (index = 0; index < 7; index++) {
    strcpy(roomHolder[index]->name, nameList[nameIndices[index]]);
    //Kind of strange to shuffle integers rather than the names themselves, but this is how I did it.
    //So we need to consider two arrays to get the source name.
    //The first 7 indices are used, the last three are just trashed. Such is life.
  }
}


//The important part of this assignment. (Not the hardest though. Reading is the hardest.)
//roomHolder is an array of pointer to roomStruct, all formatted and ready to go
//directory is the directory our stuff is going in
//Post-condition: Files for rooms are in the provided directory.
void writeFiles(struct roomStruct ** roomHolder, char * directory) {
  char filename[64]; //I can't imagine I'd need more than 32,
                     //and I didn't want to use 32 again (like with directory char array).
  int index;
  int jndex;  //I'm rather fond of "jndex". I find it hilarious. Perhaps other people don't.
              //I pronounce it like "djinn-decks", if you're curious.
  
  for (index = 0; index < 7; index++) {
    snprintf(filename, 64, "%s/%s", directory, roomHolder[index]->name);
    FILE *currentFile = fopen(filename, "w");
    
    fprintf(currentFile, "ROOM NAME: %s\n", roomHolder[index]->name);
    
    for (jndex = 0; jndex < roomHolder[index]->neighborCount; jndex++) {
      fprintf(currentFile, "CONNECTION %d: %s\n", jndex + 1, roomHolder[index]->neighbor[jndex]->name);
    }
    
    fprintf(currentFile, "ROOM TYPE: ");
    
    if (roomHolder[index]->type == START_ROOM) {
      fprintf(currentFile, "START_ROOM");
    }
    else if (roomHolder[index]->type == END_ROOM) {
      fprintf(currentFile, "END_ROOM");
    }
    else {
      fprintf(currentFile, "MID_ROOM");
    }
    
    //All well-formatted text files in unix end in a LF, if I understand right.
    fprintf(currentFile, "\n");
    
    fclose(currentFile);
  }
}


//This probably doesn't deserve its own function, but it makes writing other functions easier.
//intArray is an array of ints
//size is the size of the array
//test is the element (the integer) to look for)
//Returns 1 if integer is found in array, 0 if not.
int intArrayChecker(int * intArray, int size, int test) {
  int index;
  for (index = 0; index < size; index++) {
    if (intArray[index] == test) {
      return 1;
    }
  }
  return 0;
}


//Same as above, but for room pointers. Fun. The name is somewhat inaccurate given that this
//is checking an array of room pointers, but roomPointerArrayChecker is kind of a mouthful.
//roomArray is what I've usually been calling "roomHolder", but named differently to indicate
//the slightly different function of this function.
//size is the size of the roomArray
//room is a pointer to the room we're looking for
//Returns 1 if room is present in array (pointer to that room), 0 if not.
int roomArrayChecker(struct roomStruct ** roomArray, int size, struct roomStruct * room) {
  int index;
  for (index = 0; index < size; index++) {
    if (roomArray[index] == room) {
      return 1;
    }
  }
  return 0;
}


//This function is used to shuffle room names by index. Kind of a roundabout way, but it helps
//to avoid creating many temporary strings in memory. I feel like things might get weird if we
//did that.
//intArray is an array of integers
//size is that array's size
//Post-condition: The array is shuffled, but not in a particularly good way.
//                Cryptologically speaking.
void shuffleInt(int * intArray, int size) {
  int index;   //I use long variable names to avoid confusion, but it doesn't really help here.
  int counter; //"counter" and "index" are kind of interchangeable.
  int temp;
  for (counter = 0; counter < size; counter++) {
    index = rand() % size;
    temp = intArray[index];
    intArray[index] = intArray[counter];
    intArray[counter] = temp;
  }
}


//This shuffles roomStruct pointers. I tend to think of pointers as numbers,
//so it's not all that different from shuffleInt. Just a different data type.
//neighbor is an array of pointers to roomStruct
//(The name "neighbor" kind of gives away the fact that this function was originally part of
// a different, longer function)
//size is that array's size
//Post-condition: The array is shuffled, but not in a particularly good way.
//                Cryptologically speaking.
void shuffleRoom(struct roomStruct ** neighbor, int size) {
  int index;
  int counter; //See above comment on the usefulness of naming these "counter" and "index".
  struct roomStruct * temp;
  for (counter = 0; counter < size; counter++) {
    index = rand() % size;
    temp = neighbor[index];
    neighbor[index] = neighbor[counter];
    neighbor[counter] = temp;
  }
}


//This is a mostly self-contained function to handle the game portion.
//It takes two arguments, the directory and the first room's name.
//Post-condition: You won!
//                Maybe you lost. Doesn't really matter.
//                You can only lose if you take over 500 steps.
void playGame(char * directory, char * startRoom) {
  struct playRoom currentRoom;
  
  int index;
  for (index = 0; index < 6; index++) {
    currentRoom.neighborName[index] = malloc(7);
    assert(currentRoom.neighborName[index] != 0);
  }
  
  char filename[64];
  snprintf(filename, 64, "%s/%s", directory, startRoom);
  
  readRoom(&currentRoom, filename);
  
  char * path[500]; //I would walk 500 miles
                    //but I wouldn't walk 500 more

  for (index = 0; index < 500; index++) {
    path[index] = malloc(7);
    assert(path[index] != 0);
  }
  int pathCounter = 0;
  
  char userChoice[8];
  //I don't need to read more than six characters entered.
  //But I allow exactly one extra.
  
  do {
  printf("CURRENT LOCATION: %s\n", currentRoom.name);
  
  printf("POSSIBLE CONNECTIONS: "); 
  for (index = 0; index < currentRoom.neighborCount; index++) {
    printf("%s", currentRoom.neighborName[index]);
    
    //We print the .\n when we reach the last neighbor. No earlier.
    if (index == currentRoom.neighborCount - 1) {
      printf(".\n");
    }
    else {
      printf(", ");
    }
  }
  
  printf("WHERE TO? >");
  //fgets(userChoice, 7, stdin);
  
  //Changing to not use fgets
  fscanf(stdin, "%7[^\n]", userChoice);
  //Make sure the player doesn't enter carrotttt or something.
  
  while (getchar() != '\n'); //A fun way to eat newlines that I learned in 261.
                             //Still not completely sure why it works.
  printf("\n"); //An extra blank line after the user's return.
  
  for (index = 0; index < currentRoom.neighborCount; index++) {
    if (strcmp(currentRoom.neighborName[index], userChoice) == 0) {
      snprintf(filename, 64, "%s/%s", directory, currentRoom.neighborName[index]);
      strcpy(path[pathCounter], currentRoom.neighborName[index]);
      pathCounter++;
      break; //If we found the room, we're good to exit the loop.
    }
  }
  
  //The only error condition I've programmed in.
  if (pathCounter > 500) {
    fprintf(stderr, "SORRY, BUT YOU HAVE WALKED 500 MILES\n"
                    "AND YOU CAN'T WALK 500 MORE.\n"
                    "(EACH ROOM IS CONNECTED BY A PATH OF 1 MILE)\n");
    
    //This is an forced exit condition, but we still free up memory. Good habits.
    for (index = 0; index < 500; index++) {
      free(path[index]);
    }
    
    for (index = 0; index < 6; index++) {
      free(currentRoom.neighborName[index]);
    }
    exit(1);
  }
  
  //An unusual way of checking for errors. I like it though.
  if (index == currentRoom.neighborCount) {
    printf("HUH? I DON'T UNDERSTAND THAT ROOM. TRY AGAIN.\n\n");
  }
  
  readRoom(&currentRoom, filename);
  } while (currentRoom.type != END_ROOM);
  
  //It's time to start winning.
  printf("YOU HAVE FOUND THE END ROOM. CONGRATULATIONS!\n"
         "YOU TOOK %d STEPS. YOUR PATH TO VICTORY WAS:\n", pathCounter);
  
  for (index = 0; index < pathCounter; index++) {
    printf("%s\n", path[index]);
  }
  
  printf("\n"); //An extra newline for better formatting.
  
  //And the freeing of memory.
  for (index = 0; index < 500; index++) {
    free(path[index]);
  }
  
  for (index = 0; index < 6; index++) {
    free(currentRoom.neighborName[index]);
  }
}


//This function reads a room from a file into the playRoom struct.
//Note that only one playRoom struct is ever used. Maybe a waste of a struct?
//currentRoom is a pointer to a playRoom
//filename is the filename to read in
//Post-condition: currentRoom is wiped and replaced with the room read in.
void readRoom(struct playRoom * currentRoom, char * filename) {
  char currentLine[32]; //I counted, and my files all have pretty short lines.
                        //Maximum character count is in "ROOM TYPE: START_ROOM"
  char neighborName[7]; //I love the convenience of six-letter names.
                        //Maybe I should have went lower, with five or four?
                        //But four-letter names might be offensive.
  char typeName[11]; //"START_ROOM" is ten characters.
                     //I just realised I could have used the same temporary string
                     //for both types and room names. Oh well.
                     
  //readRoom also initialises the room. Strange, I know.
  //This was originally done in playGame, but it made more sense here.
  currentRoom->neighborCount = 0;
  
  FILE *currentFile = fopen(filename, "r");
  
  //Read the first line first. It will always be the name.
  fgets(currentLine, 32, currentFile);
  sscanf(currentLine, "ROOM NAME: %6[^\n]", currentRoom->name);
  //The [^\n] method should include spaces as well. But we don't use spaces.
  //I still have the width specifiers (6 for names, 10 for types) to avoid overflows.
  
  //Looping to read links while not null.
  while (fgets(currentLine, 32, currentFile) != 0) {
    if (sscanf(currentLine, "CONNECTION %*d: %6[^\n]", neighborName) == 1) {
      strcpy(currentRoom->neighborName[currentRoom->neighborCount], neighborName);
      currentRoom->neighborCount++;
    }
  }
  
  //Pretty sure that last loop will blow past the room type line, which we still need to read.
  rewind(currentFile);
  //Looping the fgets again, simply to get back to the last line.
  while (fgets(currentLine, 32, currentFile) != 0) {
    if (sscanf(currentLine, "ROOM TYPE: %10[^\n]", typeName) == 1) {
      //This first conditional will always hit for the first room during regular use.
      if (strcmp(typeName, "START_ROOM") == 0) {
        currentRoom->type = START_ROOM;
      }
      //This second will hit for the final room before the game ends.
      else if (strcmp(typeName, "END_ROOM") == 0) {
        currentRoom->type = END_ROOM;
      }
      //All other rooms are MID, so not much of a check here.
      else {
        currentRoom->type = MID_ROOM;
      }
    }
  }
  
  fclose(currentFile);
}