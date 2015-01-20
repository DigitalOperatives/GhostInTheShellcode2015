.PHONY : all
all : edgy.dbg edgy

edgy.dbg : edgy.cpp
	g++ --std=c++11 -Wall -Wextra -g -lz $< -o $@

edgy : edgy.cpp
	g++ --std=c++11 -Wall -Wextra -DNDEBUG -O3 -lz $< -o $@

.PHONY : clean
clean :
	rm -f edgy edgy.dbg
