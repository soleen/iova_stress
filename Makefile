iova_stress: iova_stress.c
	$(CC) -o $@ -static -Os iova_stress.c
clean:
	rm -f iova_stress
.PHONY: clean
