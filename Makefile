# rule of thumb: build one .o file per .c/.cpp file and one make all target that remakes everything
# this makes building faster since each time you change something, it make only rebuilds the object file for the file you changed
# it doesn't waste time rebuilding stuff you didn't edit
# compiling: checks syntax, expands #includes and #defines, outputs object file
# linking: links together libraries for the functions needed, outputs one combined binary file in memory to run
# target: dependencies
# the shell commands that are needed to build that target
# the target is imageview, and it depends on the object file
# we need C++, the object file, -o imageview tells the compiler to put the output of this build into imageview target
# the stuff underneath is the recepie we need to build this target
# we need the executable to include the object file, C++, and the libraries
# since this is the linking stage and happens after the compiling, we don't need the flags here
# adding the @ silences all the build info when i type make
# Cince I am using c++, g++. Use CXX everywhere that C was.
# CXX is the C++ compiler
CXX= g++

# -g will add compiler info, -Wall will show warnings, c++17 is the version of C++ we are using
# need to include the shell package config command because that tells the C++ compiler where to find the opencv library
# we are writing our own makefile and not using Cmake which is why we need to tell it where the library path is
# need to know where the library of files is during compiling to create the object file
CXXFLAGS= -g -Wall -std=c++17 $(shell pkg-config --cflags opencv4 sndfile portaudio-2.0) -pthread

# libraries I want to link with this project
# need to include the library path while linking to create the executable
LIBS = $(shell pkg-config --libs opencv4 sndfile portaudio-2.0) -pthread

# list all the object files
OBJS = main.o audio_player.o dsp.o

# all is just all of the targets
all: main

# since the main target uses all the object files put the OBJS there
main: $(OBJS)
	@$(CXX) $(OBJS) -o main $(LIBS)

#grayscale and sobel just use their own respective object files so dont need all OBJS
# dont need these as separate targets because just helper functions
# if they were to be runnable files they would need to be targets
# grayscale: grayscale.o
# 	@$(CXX) grayscale.o main.o -o grayscale $(LIBS)

# sobel: sobel.o
# 	@$(CXX) sobel.o main.o -o sobel $(LIBS)

# to create the object file, it depends on the c++ file and the header file
# since this is what runs at compile time, we need to include the flags
# -c tells the compiler to just compile and stop after that, don't link yet
# -o <filename> tells the compiler to put the output of this build into imageview.o
main.o: main.cpp main.h
	@$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

audio_player.o: audio_player.cpp audio_player.h
	@$(CXX) $(CXXFLAGS) -c audio_player.cpp -o audio_player.o

dsp.o: dsp.cpp dsp.h
	@$(CXX) $(CXXFLAGS) -c dsp.cpp -o dsp.o

# for cleaning
clean:
	rm -f *.o main audio_player dsp