CXX      = g++
CXXFLAGS = -O3 -mbmi2 -fopenmp -no-pie
LDFLAGS  = -fopenmp
LDLIBS   = -lhugetlbfs

SRCS = main.cpp globals.cpp utils.cpp classification.cpp stream.cpp bandwidth.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = benches

all: config.py $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# config.py is a USER INPUT read at runtime by Test.py (not a compile input).
# Auto-create it from the template on first `make`, but never overwrite an
# existing one — local machine edits are preserved. It also survives `clean`.
config.py:
	@cp config_template.py config.py
	@echo ">> config.py created from config_template.py — edit for your machine, then ./Test.sh"

# Force-regenerate config.py from the template (overwrites local edits).
config:
	@cp config_template.py config.py
	@echo ">> config.py regenerated from config_template.py (local edits overwritten)."

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean config
