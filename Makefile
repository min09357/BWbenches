CXX      = g++
CXXFLAGS = -O3 -mbmi2 -fopenmp -no-pie
LDFLAGS  = -fopenmp
LDLIBS   = -lhugetlbfs

SRCS = main.cpp globals.cpp utils.cpp classification.cpp stream.cpp bandwidth.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = benches

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
