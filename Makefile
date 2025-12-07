build:
	gcc kernel.c -o lightforth

run: build
	./lightforth

clean:
	rm -rf lightforth
