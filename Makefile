can2joy: can2joy.c
	gcc -o $@ -O $^ -Wall -Wextra


.PHONY: clean
clean:
	rm -f can2joy
