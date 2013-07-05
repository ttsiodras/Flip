TARGETCPP11=FlipCPP
TARGETOCAML=FlipML

CXX=g++

CXXFLAGS=-Wall -Werror -Wextra -Wconversion -Wno-deprecated -Winit-self -Wsign-conversion -Wredundant-decls -Wvla -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor -Woverloaded-virtual -Wlogical-op -Wmissing-include-dirs -Winit-self -Wpointer-arith -Wcast-qual -Wcast-align -Wsign-promo -Wundef
CXXFLAGS+=-O3 -march=native -mtune=native -mmmx -msse
#CXXFLAGS+=-g

OCAMLFLAGS=-unsafe -rectypes -inline 1000 
#OCAMLFLAGS=-annot

all:	$(TARGETCPP11)

$(TARGETCPP11):	Flip.cc
	$(CXX) -o $@ -std=c++11 $(CXXFLAGS) $<

$(TARGETOCAML):	Flip.ml
	ocamlopt $(OCAMLFLAGS) -o ./$@ bigarray.cmxa $<

test:	| $(TARGETCPP11)
	/bin/bash -c "time yes | ./FlipCPP"

clean:
	rm -f $(TARGETCPP11) $(TARGETOCAML) Flip.cm? Flip.o
