CC = g++
CC_FLAGS = -std=c++17 -Wall -O2
LD_LIBS = -pthread -lboost_system -lboost_regex -lssl -lcrypto -ldl -lz
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


