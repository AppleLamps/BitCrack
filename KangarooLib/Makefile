CPPSRC:=$(wildcard *.cpp)
OBJS:=$(CPPSRC:.cpp=.o)

all:	$(OBJS)
	mkdir -p $(LIBDIR)
	ar rcs $(LIBDIR)/libkangaroo.a $(OBJS)

%.o:	%.cpp
	$(CXX) -c $< $(INCLUDE) $(CXXFLAGS)

clean:
	rm -rf *.o
	rm -rf libkangaroo.a
