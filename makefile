###
### @Author - Alexander Drath
### @Date - 5/18/19
### @Description: The makefile for the smallsh program

# Project Name
#
PROJ = CS344_Project_3_Drath_Alexander

# The Compiler: gcc for C program
#
CC = gcc

# Compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
#
CFLAGS = -std=c99
CFLAGS = -g
#CFLAGS = -Wall

# name of the outputfile executable:
#
OUTPUTFILE = smallsh

# name of the object file
#
OBJS = smallsh.o

# name of source files
SRCS = smallsh.c

# Compiling the object files using the compilers
#
$(OUTPUTFILE): $(OBJS)
	$(CC) $(CFLAGS) -o $(OUTPUTFILE) $(SRCS)

# Making the object files for the program 
#
${OBJS}: ${SRCS}
	${CC} ${CFLAGS} -c $(SRCS)

# Cleaning the object files and program made by the makefile
#
clean:
	$(RM) $(OBJS) *zip $(OUTPUTFILE)

# Make a zip file of all of the source files and the makefile
#
zip:
	zip $(PROJ) .zip *.c makefile README.txt