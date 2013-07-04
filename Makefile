TARGETCPP11=FlipCPP
TARGETOCAML=FlipML

CXX=g++

CXXFLAGS=-Wall
CXXFLAGS+=-O3 -march=native -mtune=native -mmmx -msse
#CXXFLAGS+=-g

OCAMLFLAGS=-unsafe -rectypes -inline 1000 
#OCAMLFLAGS=-annot

all:	$(TARGETCPP11)

$(TARGETCPP11):	Flip.cc
	$(CXX) -o $@ -std=c++11 $(CXXFLAGS) $<

$(TARGETOCAML):	Flip.ml
	ocamlopt $(OCAMLFLAGS) -o ./$@ bigarray.cmxa $<

test:
	/bin/bash -c "time yes | ./FlipCPP"

clean:
	rm -f $(TARGETCPP11) $(TARGETOCAML) Flip.cm? Flip.o
