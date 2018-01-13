default: all
all:mrefgen a4vmsim
mrefgen:mrefgen.c
	gcc -o mrefgen mrefgen.c -lm
a4vmsim:a4vmsim.c
	gcc -o a4vmsim a4vmsim.c -lm
clean:
	rm -f mrefgen
	rm -f mrefgen.o
	rm -f a4vmsim
	rm -f a4vmsim.o
	rm -f *.in
	rm -f *.out
tar:
	tar zcvf submit.tar *