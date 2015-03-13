CC = g++
CC_FLAGS = -std=c++11 -Wall -O2
LD_LIBS = -pthread -L/usr/lib/x86_64-linux-gnu -lboost_system -lboost_regex
# -ltcmalloc_minimal 

EXEC = asb
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

# Main target
$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) $(LD_LIBS)

%.o: %.cpp
	$(CC) -c $(CC_FLAGS) $< -o $@

clean:
	rm -rf $(EXEC) $(OBJECTS)


