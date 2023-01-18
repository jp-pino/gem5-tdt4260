FROM gcr.io/gem5-test/ubuntu-20.04_all-dependencies

RUN mkdir /gem5
WORKDIR /gem5

ADD . /gem5

RUN scons build/X86/gem5.opt -j 6