CC = $(CROSS_COMPILE)gcc
iova_stress: iova_stress.c
	$(CC) -Werror -Wformat -lrt -o  $@ -static -Os iova_stress.c
clean:
	rm -f iova_stress
.PHONY: clean
