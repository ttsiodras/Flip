TARGETCPP11=FlipCPP
TARGETOCAML=FlipML
TARGETS=

OCAMLOPT_EXISTS=$(shell which ocamlopt)
ifneq ($(OCAMLOPT_EXISTS),)
TARGETS:=$(TARGETS) $(TARGETOCAML)
else
$(warning Missing ocamlopt, cannot compile OCaml code...)
endif

GPLUSPLU_EXISTS=$(shell which g++)
ifneq ($(GPLUSPLU_EXISTS),)
TARGETS:=$(TARGETS) $(TARGETCPP11)
else
$(warning Missing g++, cannot compile C++ code...)
endif

CXX=g++
CXXFLAGS=-Wall -Werror -Wextra -Wconversion -Wno-deprecated -Winit-self -Wsign-conversion -Wredundant-decls -Wvla -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor -Woverloaded-virtual -Wlogical-op -Wmissing-include-dirs -Winit-self -Wpointer-arith -Wcast-qual -Wcast-align -Wsign-promo -Wundef
CXXFLAGS+=-O3 -march=native -mtune=native -mmmx -msse
#CXXFLAGS+=-g

OCAMLFLAGS=-unsafe -rectypes -inline 1000 
#OCAMLFLAGS=-annot

all:	$(TARGETS)

$(TARGETCPP11):	Flip.cc
	$(CXX) -o $@ -std=c++11 $(CXXFLAGS) $<

$(TARGETOCAML):	Flip.ml
	ocamlopt $(OCAMLFLAGS) -o ./$@ bigarray.cmxa $<

test:	| $(TARGETCPP11)
	bash -c "time yes | ./FlipCPP"

clean:
	rm -f $(TARGETCPP11) $(TARGETOCAML) Flip.cm? Flip.o
