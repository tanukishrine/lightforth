#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK 1024
#define FLAG_IMMEDIATE	0x80
#define FLAG_HIDDEN	0x40
#define FLAG_PRIMITIVE	0x20
#define FLAG_LENMASK	0x1f

typedef intptr_t cell;
typedef void (*code)();

/* dictionary structure */
typedef struct word
{
	struct word* link;
	char namelen; /* and flags */
	char name[31];
	code data[];
} word;

/* instruction pointer */
code* ip = NULL;

/* global variables */
cell state = 0;		/* interpreted or compile mode */
word* latest = NULL;	/* pointer to latest defined word */
cell here = 0;		/* data index to thereof */
cell eol = 0;		/* end of line flag */
FILE* fp = NULL;	/* file pointer */

/* parameter stack */
cell stack[BLOCK];
cell* s0 = stack - 1;
cell* sp = stack - 1;

/* return stack */
cell rstack[BLOCK];
cell* r0 = rstack - 1;
cell* rp = rstack - 1;

/* parameter stack macros */
void push(cell w) { *++sp = w; }
cell pop() { return *sp--; }
cell peek() { return *sp; }

/* return stack macros */
void rpush(cell w) { *++rp = w; }
cell rpop() { return *rp--; }
cell rpeek() { return *rp; }


/* memory copy */
void memmov(char* addr1, char* addr2, char n)
{
	while (n--) *addr1++ = *addr2++;
}

/* memory comparison */
cell memequ(char* addr0, char* addr1, char n)
{
	while (n--) if (*addr0++ != *addr1++) return n + 1;
	return 0;
}

/* create new word header */
void word_header(char namelen, char* name, char flags)
{
	word* new = malloc(sizeof(word));
	if (!new) { perror(NULL); exit(EXIT_FAILURE); }
	new -> link = latest;
	new -> namelen = namelen | flags;
	memmov(new -> name, name, namelen);
	latest = new;
	here = 0;
}

/* allot n cells to latest */
void word_allot(cell n)
{
	size_t size = sizeof(word) + sizeof(cell) * here + sizeof(cell) * n;
	word* new = realloc(latest, size);
	if (!new) { perror(NULL); exit(EXIT_FAILURE); }
	latest = new;
	here += n;
}

/* append word to latest */
void word_append(code w)
{
	word_allot(1);
	latest -> data[here - 1] = w;
}


/* stack manipulation */

void drop() { sp--; }

void dup() { sp++; *sp = *(sp - 1); }

void over() { sp++; *sp = *(sp - 2); }

void swap()
{
	cell temp = *sp;
	*sp = *(sp - 1);
	*(sp - 1) = temp;
}

void rot()
{
	cell temp = *(sp - 2);
	*(sp - 2) = *(sp - 1);
	*(sp - 1) = *sp;
	*sp = temp;
}

void to_r() { *++rp = *sp--; }

void r_from() { *++sp = *rp--; }

void r_fetch() { *++sp = *rp; }

void depth() { *++sp = sp - s0; }


/* comparison */

void lt() { push((pop() > pop()) ? -1 : 0); }

void eq() { push((pop() == pop()) ? -1 : 0); }

void gt() { push((pop() < pop()) ? -1 : 0); }

void ltz() { push((pop() < 0) ? -1 : 0); }

void eqz() { push((pop() == 0) ? -1 : 0); }

void gtz() { push((pop() > 0) ? -1 : 0); }


/* arithmetic and logical */

void add() { sp--; *sp = *sp + *(sp + 1); }

void sub() { sp--; *sp = *sp - *(sp + 1); }

void incr() { *sp += 1; }

void decr() { *sp -= 1; }

void ls() { *sp <<= 1; }

void rs() { *sp >>= 1; }

void mul() { sp--; *sp = *sp * *(sp + 1); }

void quot() { sp--; *sp = *sp / *(sp + 1); }

void rem() { sp--; *sp = *sp % *(sp + 1); }

void max() { sp--; if (*sp < *(sp + 1)) *sp = *(sp + 1); }

void min() { sp--; if (*sp > *(sp + 1)) *sp = *(sp + 1); }

void mag() { if (*sp < 0) *sp = -*sp; }

void negate() { *sp = -*sp; }

void and() { sp--; *sp = *sp & *(sp + 1); }

void or() { sp--; *sp = *sp | *(sp + 1); }

void xor() { sp--; *sp = *sp ^ *(sp + 1); }

void not() { *sp = ~*sp; }


/* memory */

void fetch()
{
	*sp = *((cell*) *sp);
}

void store()
{
	cell* addr = (cell*) pop();
	cell n = pop();
	*addr = n;
}

void cfetch()
{
	*sp = *((char*) *sp);
}

void cstore()
{
	char* addr = (char*) pop();
	char byte = (char) pop();
	*addr = byte;
}

void addstore()
{
	cell* addr = (cell*) pop();
	cell diff = pop();
	*addr += diff;
}

void move() /* ( addr1 addr2 n -- ) */
{
	size_t n = (size_t) pop();
	void* addr2 = (void*) pop();
	void* addr1 = (void*) pop();
	memmov(addr2, addr1, n * sizeof(cell));
}

void cmove() /* ( addr1 addr2 n -- ) */
{
	size_t n = (size_t) pop();
	void* addr2 = (void*) pop();
	void* addr1 = (void*) pop();
	memmov(addr2, addr1, n);
}

void fill()
{
	char byte = (char) pop();
	cell n = (cell) pop();
	char* addr = (char*) pop();
	while (n--) *addr++ = byte;
}


/* terminal input-output */

void key() { push((cell) getchar()); }

void emit() { putchar((int) pop()); }

void dot() { printf("%ld ", pop()); }


/* call to words */
void call() { rpush((cell) (ip + 1)); ip = (code*) *ip; }

/* exit from words */
void ret() { ip = (code*) rpop(); }

/* literals */
void lit() { push((cell) *ip++); }


/* branching */

void branch() { ip += (cell) *ip; }

void zbranch() { if (pop()) ip++; else branch(); }


/* macro for built-in primivites*/
void word_primitive(char namelen, char* name, char flags, code func)
{
	word_header(namelen, name, flags | FLAG_PRIMITIVE);
	word_append(func);
	word_append(ret);
}

/* macro for built-in primivites*/
void word_literal(char namelen, char* name, code x)
{
	word_header(namelen, name, 0);
	word_append(lit);
	word_append(x);
	word_append(ret);
}

/* input buffer */
char buffer[BLOCK];
char* in = buffer;

char pad[BLOCK];

/* compiler */

void _char() /* ( -- byte ) */
{
	push((cell) *in++);
}

void parse() /* ( -- addr len | null null ) */
{
	while (*in <= 0x20)
	{
		if (*in == 0x00)
		{
			push(0);
			push(0);
			return;
		}
		in++;
	}
	push((cell) in++);
	push(1);
	while (*in > 0x20)
	{
		incr();
		in++;
	}
}

void number() /* ( addr len -- addr len | number null ) */
{
	cell len = pop();
	cell addr = pop();
	cell count = 0;
	cell result = 0;
	cell negate = 0;
	char* ptr = (char*) addr;
	if (*ptr == 0x2d)
	{
		negate = -1;
		ptr++;
		count++;
	}
	while (count++ < len)
	{
		result *= 10;
		char w = *ptr++ - 0x30;
		if (w < 0 || w > 9)
		{
			push(addr);
			push(len);
			return;
		}
		result += (cell) w;
	}
	if (negate) result = -result;
	push(result);
	push(0);
}

void find() /* ( addr len -- header ) */
{
	char len = (char) pop();
	char* name = (char*) pop();
	word* current = latest;
	while (current)
	{
		if (!(current -> namelen & FLAG_HIDDEN))
		if ((current -> namelen & FLAG_LENMASK) == len)
		if (!(memequ(current -> name, name, len)))
			break;
		current = current -> link;
	}
	push((cell) current);
}

void to_count() /* >count ( header -- count ) */
{
	*sp = (cell) &(((word*) *sp) -> namelen);
}

void to_data()
{
	*sp = (cell) &(((word*) *sp) -> data);
}

void to_body() /* >body ( header -- body ) */
{
	*sp = (cell) (((word*) *sp) -> data);
}

void execute() /* ( body -- ) */
{
	rpush((cell) ip);
	ip = (code*) pop();
}

void literal() /* ( n -- ) */
{
	word_append(lit);
	word_append((code) pop());
}

void comma() /* ( n -- ) */
{
	word_append((code) pop());
}

void allot() /* ( n -- ) */
{
	word_allot(pop());
}

char peek_count()
{
	return ((word*) peek()) -> namelen;
}

void compile() /* compile, ( header -- ) */
{
	char flag = peek_count() & FLAG_PRIMITIVE;
	to_body();
	if (flag)
	{
		word_append((code) *((cell*) pop()));
	}
	else
	{
		word_append(call);
		word_append((code) pop());
	}
}

void lbrac() { state = 0; }

void rbrac() { state = 1; }

void immediate() { latest -> namelen ^= FLAG_IMMEDIATE; }

void hidden() { latest -> namelen ^= FLAG_HIDDEN; }

void create() /* ( "<spaces><name>" -- ) */
{
	parse();
	char namelen = (char) pop();
	char* name = (char*) pop();
	word_header(namelen, name, 0);
}

/* literal strings */
/* to be added... */

void query()
{
	in = buffer;
	if (fp)
	{
		if (fgets(buffer, BLOCK, fp)) return;
		fclose(fp);
		fp = NULL;
	}
	fgets(buffer, BLOCK, stdin);
}

void interpret()
{
	parse();
	if (!peek())
	{
		drop();
		drop();
		eol = 1;
		return;
	}
	number();
	if (!peek())
	{
		drop();
		if (state) literal();
		return;
	}
	find();
	if (!peek())
	{
		drop();
		printf("Word not found.\n");
		return;
	}
	if (state)
	{
		if (peek_count() & FLAG_IMMEDIATE)
		{
			to_body();
			execute();
		}
		else
		{
			compile();
		}
	}
	else
	{
		to_body();
		execute();
	}
}

void quit() { eol = 0; rp = r0; query(); }

void fetch_eol() { push(eol); }

void ok() { if (!fp) fputs(" ok\n", stdout); }

void printword() /* DEBUG */
{
	printf("link: %p\n", latest -> link);
	printf("namelen: %d\n", latest -> namelen /* & FLAG_LENMASK */);
	printf("name: %s\n", latest -> name);
	cell count = 0;
	while (count < here) printf("code: %ld\n", (cell) (latest -> data[count++]));
	printf("\n");
}

void printstack() /* DEBUG */
{
	printf("s: ");
	cell* ptr = s0 + 1;
	while (ptr <= sp) printf("%ld ", *ptr++);
}

void include()
{
	parse();
	char length = (char) pop();
	char* addr = (char*) pop();
	char buffer[length + 1];
	memmov(buffer, addr, length);
	buffer[length] = 0;
	fp = fopen(buffer, "r");
}

void init()
{
	/* stack manipulation */
	word_primitive(4, "drop", 0, drop);
	word_primitive(3, "dup", 0, dup);
	word_primitive(4, "over", 0, over);
	word_primitive(4, "swap", 0, swap);
	word_primitive(3, "rot", 0, rot);
	word_primitive(2, ">r", 0, to_r); /* potential issue, compile only? */
	word_primitive(2, "r>", 0, r_from); /* potential issue, compile only? */
	word_primitive(2, "r@", 0, r_fetch);
	word_primitive(5, "depth", 0, depth);

	/* comparison */
	word_primitive(1, "<", 0, lt);
	word_primitive(1, "=", 0, eq);
	word_primitive(1, ">", 0, gt);
	word_primitive(2, "0<", 0, ltz);
	word_primitive(2, "0=", 0, eqz);
	word_primitive(2, "0>", 0, gtz);

	/* arithmetic and logical */
	word_primitive(1, "+", 0, add);
	word_primitive(2, "1+", 0, incr);
	word_primitive(2, "1-", 0, decr);
	word_primitive(2, "2*", 0, ls);
	word_primitive(2, "2/", 0, rs);
	word_primitive(1, "*", 0, mul);
	word_primitive(1, "/", 0, quot);
	word_primitive(3, "mod", 0, rem);
	word_primitive(3, "max", 0, max);
	word_primitive(3, "min", 0, min);
	word_primitive(3, "abs", 0, mag);
	word_primitive(6, "negate", 0, negate);
	word_primitive(3, "and", 0, and);
	word_primitive(2, "or", 0, or);
	word_primitive(3, "xor", 0, xor);
	word_primitive(3, "not", 0, not);

	/* memory */
	word_primitive(1, "@", 0, fetch);
	word_primitive(1, "!", 0, store);
	word_primitive(2, "c@", 0, cfetch);
	word_primitive(2, "c!", 0, cstore);
	word_primitive(2, "+!", 0, addstore);
	word_primitive(4, "move", 0, move);
	word_primitive(5, "cmove", 0, cmove);
	word_primitive(4, "fill", 0, fill);

	/* terminal input-output */
	word_primitive(3, "key", 0, key);
	word_primitive(4, "emit", 0, emit);
	word_primitive(1, ".", 0, dot);

	/* special words */
	word_primitive(4, "call", 0, call);
	word_primitive(4, "exit", 0, ret);
	word_primitive(3, "lit", 0, lit);
	word_primitive(6, "branch", 0, branch);
	word_primitive(7, "0branch", 0, zbranch);
	word_literal(2, "ip", (code) &ip);
	word_literal(3, "eol", (code) &eol);

	/* compiler */
	word_literal(6, "buffer", (code) &buffer);
	word_literal(3, ">in", (code) &in);
	word_literal(3, "pad", (code) &pad);
	word_primitive(4, "char", 0, _char);
	word_primitive(5, "parse", 0, parse);
	word_primitive(6, "number", 0, number);
	word_primitive(4, "find", 0, find);
	word_primitive(7, "execute", 0, execute);
	word_primitive(7, "literal", 0, literal);
	word_primitive(1, ",", 0, comma);
	word_primitive(5, "allot", 0, allot);
	word_primitive(8, "compile,", 0, compile);
	word_literal(4, "here", (code) &here);
	word_literal(5, "state", (code) &state);
	word_primitive(1, "[", FLAG_IMMEDIATE, lbrac);
	word_primitive(1, "]", 0, rbrac);
	word_primitive(9, "immediate", FLAG_IMMEDIATE, immediate);
	word_primitive(6, "hidden", 0, hidden);
	word_primitive(6, ">count", 0, to_count);
	word_primitive(5, ">data", 0, to_data);
	word_primitive(5, ">body", 0, to_body); /* merge >data with >body */
	word_literal(6, "latest", (code) &latest);

	/* defining words */
	word_primitive(6, "create", 0, create);

	/* stack pointers */
	word_literal(2, "S0", (code) s0);
	word_literal(2, "sp", (code) &sp);
	word_literal(2, "R0", (code) s0);
	word_literal(2, "rp", (code) &sp);

	/* constants */
	word_literal(4, "CELL", (code) sizeof(cell));
	word_literal(5, "BLOCK", (code) BLOCK);
	word_literal(7, "VERSION", (code) 1);
	word_literal(14, "FLAG_IMMEDIATE", (code) FLAG_IMMEDIATE);
	word_literal(11, "FLAG_HIDDEN", (code) FLAG_HIDDEN);
	word_literal(14, "FLAG_PRIMITIVE", (code) FLAG_PRIMITIVE);
	word_literal(12, "FLAG_LENMASK", (code) FLAG_LENMASK);

	/* include file */
	word_primitive(7, "include", 0, include);

	/* DEBUG */
	word_primitive(2, ".s", 0, printstack);
	word_primitive(5, "debug", 0, printword);

	/* quit */
	word_header(4, "quit", 0);
	word_append(quit);
	word_append(interpret);
	word_append(fetch_eol);
	word_append(zbranch);
	word_append((code) -3);
	word_append(ok);
	word_append(branch);
	word_append((code) -7);
}

int main()
{
	init();
	fp = fopen("core.fs", "r");
	ip = (code*) (latest -> data);
	while (ip) (*ip++)();
	return 0;
}
