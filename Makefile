phttpget: phttpget.c
	cc -O0 -std=c99 -pedantic -Wall -Wextra -Werror -g -o phttpget phttpget.c

clean:
	rm phttpget
