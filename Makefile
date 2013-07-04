TARGETCPP11=FlipCPP
TARGETOCAML=FlipML

CXX=g++

CXXFLAGS=-Wall
CXXFLAGS+=-O3 -march=native -mtune=native -mmmx -msse
#CXXFLAGS+=-g

OCAMLFLAGS=-unsafe -rectypes -inline 1000 
#OCAMLFLAGS=-annot

all:	$(TARGETOCAML) $(TARGETCPP11)

world:	$(TARGETCPP11) $(TARGETOCAML)

$(TARGETCPP11):	Flip.cc
	$(CXX) -o $@ -std=c++0x $(CXXFLAGS) $<

$(TARGETOCAML):	Flip.ml
	ocamlopt $(OCAMLFLAGS) -o ./$@ bigarray.cmxa $<

benchmark:	world
	/bin/bash -c "time yes | ./Flip-solve-c++11"

clean:
	rm -f $(TARGETCPP11) $(TARGETOCAML) Flip.cm? Flip.o
