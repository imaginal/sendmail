all:
	gcc -Wall -Werror sendmail.c -o sendmail

clean:
	rm -f sendmail