NAME=cryptoutil
SRC=$(wildcard *.cpp)
UNAME_M:=$(shell uname -m)

# SHA-NI object is built on x86_64; runtime CPUID decides whether to use it.
# Compile the SHA-NI file without -march=native so AVX512 does not mix with SHA-NI.
ifeq ($(UNAME_M),x86_64)
	CRYPTO_CXXFLAGS=$(CXXFLAGS) -DHAVE_SHA_NI
	SHA_NI_CXXFLAGS=-O3 -std=c++11 -msse4.1 -msha -DHAVE_SHA_NI
else ifeq ($(UNAME_M),amd64)
	CRYPTO_CXXFLAGS=$(CXXFLAGS) -DHAVE_SHA_NI
	SHA_NI_CXXFLAGS=-O3 -std=c++11 -msse4.1 -msha -DHAVE_SHA_NI
else
	CRYPTO_CXXFLAGS=$(CXXFLAGS)
	SHA_NI_CXXFLAGS=
endif

all:    ${SRC}
	for file in ${SRC} ; do\
		if [ "$$file" = "sha256_shani.cpp" ] && [ -n "${SHA_NI_CXXFLAGS}" ]; then\
			${CXX} -c $$file ${INCLUDE} ${SHA_NI_CXXFLAGS};\
		else\
			${CXX} -c $$file ${INCLUDE} ${CRYPTO_CXXFLAGS};\
		fi;\
	done
	mkdir -p ${LIBDIR}
	ar rvs ${LIBDIR}/lib$(NAME).a *.o

clean:
	rm -rf *.o
