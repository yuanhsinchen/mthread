# CMPSC 473, Project 2, starter kit

# Source, library, include, and object files
SRC = pr2.c
LIB = 
INC = 
OBJ = pr2

# Different versions of the POSIX Standard for Unix
POSIX_2001 = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
POSIX_2008 = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700

# set the default action to do nothing
dummy:

# Compile for Solaris, Linux, Mac OS X; all warnings turned on

sun:  $(SRC) $(LIB) $(INC)
	c99 $(POSIX_2001) -v -o $(OBJ) $(SRC) $(LIB)
	lint -Xc99 $(SRC) $(LIB)

linux: $(SRC) $(LIB) $(INC)
	gcc $(SRC) -std=c99 $(POSIX_2008) -Wall -Wextra -lpthread -o $(OBJ) $(LIB)

mac: $(SRC) $(LIB) $(INC)
	gcc -std=c99 -Wall -Wextra -o $(OBJ) $(SRC) $(LIB)

clean:
	rm $(OBJ)
