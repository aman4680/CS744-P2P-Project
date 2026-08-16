
CXX = g++
CXXFLAGS = -std=c++17 -Wall -g -Iinclude -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare
LDFLAGS = -lpthread -lcrypto -luuid


SRC_DIR = src
OBJ_DIR = build
INCLUDE_DIR = include


MAIN1_SRC = peer.cpp helper_peer.cpp sha256.cpp
MAIN2_SRC = tracker.cpp helper_tracker.cpp sha256.cpp


MAIN1_OBJS = $(addprefix $(OBJ_DIR)/, $(MAIN1_SRC:.cpp=.o))
MAIN2_OBJS = $(addprefix $(OBJ_DIR)/, $(MAIN2_SRC:.cpp=.o))


EXECUTABLES = peer tracker


.PHONY: all clean

all: $(EXECUTABLES)


peer: $(MAIN1_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

tracker: $(MAIN2_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@


clean:
	rm -rf $(OBJ_DIR) $(EXECUTABLES)