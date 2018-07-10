phttpget: phttpget.c
	cc -O2 -std=c99 -pedantic -Wall -Wextra -Werror -g -o phttpget phttpget.c

clean:
	rm phttpget
