NAME=cryptoutil
SRC=$(wildcard *.cpp)
UNAME_M:=$(shell uname -m)

# SHA-NI / AVX2 / AVX-512 objects are built on x86_64; runtime CPUID decides.
# Keep those TUs on explicit ISAs so AVX-512 does not mix with SHA-NI or AVX2.
ifeq ($(UNAME_M),x86_64)
	CRYPTO_CXXFLAGS=$(CXXFLAGS) -DHAVE_SHA_NI -DHAVE_AVX2 -DHAVE_AVX512
	SHA_NI_CXXFLAGS=-O3 -std=c++11 -msse4.1 -msha -DHAVE_SHA_NI
	AVX2_CXXFLAGS=-O3 -std=c++11 -mavx2 -mno-avx512f -DHAVE_AVX2
	AVX512_CXXFLAGS=-O3 -std=c++11 -mavx512f -mavx512bw -mavx512vl -DHAVE_AVX512
else ifeq ($(UNAME_M),amd64)
	CRYPTO_CXXFLAGS=$(CXXFLAGS) -DHAVE_SHA_NI -DHAVE_AVX2 -DHAVE_AVX512
	SHA_NI_CXXFLAGS=-O3 -std=c++11 -msse4.1 -msha -DHAVE_SHA_NI
	AVX2_CXXFLAGS=-O3 -std=c++11 -mavx2 -mno-avx512f -DHAVE_AVX2
	AVX512_CXXFLAGS=-O3 -std=c++11 -mavx512f -mavx512bw -mavx512vl -DHAVE_AVX512
else
	CRYPTO_CXXFLAGS=$(CXXFLAGS)
	SHA_NI_CXXFLAGS=
	AVX2_CXXFLAGS=
	AVX512_CXXFLAGS=
endif

all:    ${SRC}
	for file in ${SRC} ; do\
		if [ "$$file" = "sha256_shani.cpp" ] && [ -n "${SHA_NI_CXXFLAGS}" ]; then\
			${CXX} -c $$file ${INCLUDE} ${SHA_NI_CXXFLAGS};\
		elif [ "$$file" = "ripemd160_avx2.cpp" ] && [ -n "${AVX2_CXXFLAGS}" ]; then\
			${CXX} -c $$file ${INCLUDE} ${AVX2_CXXFLAGS};\
		elif [ "$$file" = "ripemd160_avx2x8.cpp" ] && [ -n "${AVX2_CXXFLAGS}" ]; then\
			${CXX} -c $$file ${INCLUDE} ${AVX2_CXXFLAGS};\
		elif [ "$$file" = "ripemd160_avx512.cpp" ] && [ -n "${AVX512_CXXFLAGS}" ]; then\
			${CXX} -c $$file ${INCLUDE} ${AVX512_CXXFLAGS};\
		else\
			${CXX} -c $$file ${INCLUDE} ${CRYPTO_CXXFLAGS};\
		fi;\
	done
	mkdir -p ${LIBDIR}
	ar rvs ${LIBDIR}/lib$(NAME).a *.o

clean:
	rm -rf *.o
