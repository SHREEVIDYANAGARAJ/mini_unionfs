CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 $(shell pkg-config --cflags fuse3)
LDFLAGS  = $(shell pkg-config --libs fuse3)

TARGET = mini_unionfs
SRCS   = main.cpp resolver.cpp \
         p1_metadata.cpp p2_dir.cpp \
         p3_read_cow.cpp p4_write_whiteout.cpp
OBJS   = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean