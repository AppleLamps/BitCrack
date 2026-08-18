CPPSRC:=$(wildcard *.cpp)

all:
	${CXX} -o kangaroo.bin ${CPPSRC} ${INCLUDE} ${CXXFLAGS} ${LIBS} -lkangaroo -lsecp256k1 -lcryptoutil -lutil -lcmdparse -llogger
	mkdir -p $(BINDIR)
	cp kangaroo.bin $(BINDIR)/kangaroo

clean:
	rm -rf kangaroo.bin
