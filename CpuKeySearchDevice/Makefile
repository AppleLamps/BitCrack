NAME=CpuKeySearchDevice
CPPSRC:=$(wildcard *.cpp)

all:
	for file in ${CPPSRC} ; do\
		${CXX} -c $$file ${INCLUDE} ${CXXFLAGS};\
	done

	ar rvs ${LIBDIR}/lib$(NAME).a *.o

clean:
	rm -f *.o
	rm -f *.a
