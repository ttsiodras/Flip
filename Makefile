# Note: To compile with my Linux-based iPhone cross-compiler,
# I had to remove "assert" - because it uses 'abort',
# which is apparently missing from my cross compiler's RTL.

CXXFLAGS=-Wall -g

# Better error reporting
#CXX=clang++
CXX=g++

# You can uncomment this one, to use the C++11 version
# TARGET= Flip-solve-c++11
TARGETCPP=Flip-solve
TARGETCPP11=Flip-solve-c++11
TARGETOCAML=Flip

all:	$(TARGETOCAML) $(TARGETCPP11)

world:	$(TARGETCPP11) $(TARGETOCAML) # $(TARGETCPP)

$(TARGETCPP): $(TARGETCPP).cc
	$(CXX) -O3 -o $@ $(CXXFLAGS) $<

$(TARGETCPP11):	$(TARGETCPP11).cc
	$(CXX) -O3 -march=native -mtune=native -mmmx -msse -std=c++0x -o $@ $(CXXFLAGS) $<
	#$(CXX) -g -std=c++0x -o $@ $(CXXFLAGS) $<

$(TARGETOCAML):	$(TARGETOCAML).ml
	#ocamlopt -annot -o ./$@ bigarray.cmxa $<
	ocamlopt -unsafe -rectypes -inline 1000 -o ./$@ bigarray.cmxa $<

cross:
	arm-apple-darwin-g++ -DNDEBUG Flip-solve.cc -o Flip-iOS

benchmark:	world
	/usr/bin/time /bin/bash -c "yes | ./Flip-solve-c++11"

clean:
	rm -f $(TARGETCPP) $(TARGETCPP11) $(TARGETOCAML) data.rgb  Flip.cm? Flip.o
