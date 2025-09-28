#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#define VERSION 20250928

#define BLOCK_SIZE 1024
#define DICTIONARY_SIZE (BLOCK_SIZE * 64)

#define FLAG_IMMEDIATE 0x80
#define FLAG_HIDDEN    0x40
#define FLAG_PRIMITIVE 0x20
#define FLAG_LENMASK   0x1f

typedef uint8_t byte;
typedef intptr_t cell;
typedef void (*code)();

/* instruction pointer */
code* ip = NULL;

/* file pointer */
FILE* fp = NULL;

/* input buffer */
byte  buffer[BLOCK_SIZE];
byte* in = buffer;

/* end of line */
cell out = 0;

/* dictionary globals */
byte* dictionary = NULL; /* start of dictionary space */
byte* latest     = NULL; /* most recently defined word */
byte* here       = NULL; /* pointer to next free memory */

/* interpreted or complied mode */
cell state = 0;

/* parameter stack */
cell  stack[BLOCK_SIZE];
cell* s0 = stack - 1;
cell* sp = stack - 1;

/* return stack */
cell  rstack[BLOCK_SIZE];
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

/* append byte to latest */
void byte_append(byte x)
{
	*here++ = x;
}

/* append cell to latest */
void cell_append(cell w)
{
	*((cell*) here) = w;
	here += sizeof(cell);
}

/* UNUSED */
/* append string to latest */
void string_append(byte* addr, cell size)
{
	while (size--) byte_append(*addr++);
}

/* align upwards to accept cell type */
void align()
{
	uintptr_t ptr = (uintptr_t) here;
	here = (byte*) ((ptr + sizeof(cell) - 1)
		& ~(sizeof(cell) - 1));
}

/* create new word header */
void header(byte namelen, byte* name)
{
	align();
	byte* head = here;
	cell_append((cell) latest);
	latest = head;
	byte_append(namelen);
	string_append(name, (cell) namelen & FLAG_LENMASK);
	align();
}

/* return addr of name from header*/
void to_count() /* ( header -- count ) */
{
	*sp = (cell) ((byte*) *sp + sizeof(cell));
}

/* parse addr as a counted string */
void count() /* ( count -- addr len ) */
{
	byte* addr = (byte*) pop();
	push((cell) addr + 1);
	push((cell) *addr);
}

/* return the addr of body from header */
void to_body() /* ( header -- body ) */
{
	byte* ptr = (byte*) pop() + sizeof(cell);
	byte len = (*ptr & FLAG_LENMASK) + 1;
	byte rounded = len + (sizeof(cell) - 1) & ~(sizeof(cell) - 1);
	push((cell) (ptr + rounded));
}

/* call to word */
void call()
{
	rpush((cell) (ip + 1));
	ip = (code*) *ip;
}

/* exit from word */
void ret()
{
	ip = (code*) rpop();
}

/* literal */
void lit()
{
	push((cell) *ip++);
}

/* branch ( -- ) */
void branch()
{
	ip = (code*) ((cell) ip + (cell) *ip);
}

/* zbranch ( f -- ) */
void zbranch()
{
	if (pop()) ip++;
	else branch();
}

/* string literal */
void litstring()
{
	push((cell) *ip++);
	push((cell) *ip++);
	cell ptr = (cell) ip + (cell) *sp;
	ip = (code*) ((ptr + sizeof(cell) - 1)
		& ~(sizeof(cell) - 1));
}

/* drop ( a -- ) */
void drop()
{
	sp--;
};

/* dup ( a -- a a ) */
void dup()
{
	sp++; *sp = *(sp - 1);
}

/* over ( a b -- a b a ) */
void over()
{
	sp++;
	*sp = *(sp - 2);
}

/* swap ( a b -- b a ) */
void swap()
{
	cell temp = *sp;
	*sp = *(sp - 1);
	*(sp - 1) = temp; 
}

/* rot ( a b c -- b a c ) */
void rot()
{
	cell temp = *(sp - 2); 
	*(sp - 2) = *(sp - 1); 
	*(sp - 1) = *sp;
	*sp = temp;
}

/* >r ( a -- ) ( R: -- a ) */
void to_r()
{
	*++rp = *sp--;
}

/* r> ( -- a ) ( R: a -- ) */
void r_from()
{
	*++sp = *rp--;
}

/* 2* ( a -- a*2 ) */
void left_shift()
{
	*sp <<= 1;
}

/* 2/ ( a -- a/2 ) */
void right_shift()
{
	*sp >>= 1;
}

/* not ( a -- ~a ) */
void not()
{
	*sp = ~*sp;
}

/* and ( a b -- a&b ) */
void and()
{
	sp--;
	*sp = *sp & *(sp + 1);
}

/* xor ( a b -- a^b ) */
void xor()
{
	sp--;
	*sp = *sp ^ *(sp + 1);
}

/* or ( a b -- a|b ) */
void or()
{
	sp--;
	*sp = *sp | *(sp + 1);
}

/* + ( a b -- a+b ) */
void add()
{
	sp--;
	*sp = *sp + *(sp + 1);
}

/* - ( a b -- a-b ) */
void sub()
{
	sp--;
	*sp = *sp - *(sp + 1);
}

/* * ( a b -- a*b ) */
void mul()
{
	sp--;
	*sp = *sp * *(sp + 1);
}

/* / ( a b -- a/b ) */
void quot()
{
	sp--;
	*sp = *sp / *(sp + 1);
}

/* mod ( a b -- a%b ) */
void rem()
{
	sp--;
	*sp = *sp % *(sp + 1);
}

/* < ( a b -- a<b ) */
void lt()
{
	push((pop() > pop()) ? -1 : 0);
}

/* = ( a b -- a=b ) */
void eq()
{
	push((pop() == pop()) ? -1 : 0);
}

/* > ( a b -- a>b ) */
void gt()
{
	push((pop() < pop()) ? -1 : 0);
}

/* c@ ( addr -- byte ) */
void cfetch()
{
	*sp = *((byte*) *sp);
}

/* c! ( byte addr -- ) */
void cstore()
{
	byte* addr = (byte*) pop();
	byte n = (byte) pop();
	*addr = n;
}

/* @ ( addr -- n ) */
void fetch()
{
	*sp = *((cell*) *sp);
}

/* ! ( n addr -- ) */
void store()
{
	cell* addr = (cell*) pop();
	cell n = pop();
	*addr = n;
}

void addstore()
{
	cell* addr = (cell*) pop();
	cell diff = pop();
	*addr += diff;
}

/* move ( addr0 addr1 n -- ) */
void move()
{
	cell n = pop() * sizeof(cell);
	byte* addr1 = (byte*) pop();
	byte* addr0 = (byte*) pop();
	while (n--) *addr1++ = *addr0++;
}

/* cmove ( addr0 addr1 n -- ) */
void cmove()
{
	cell n = pop();
	byte* addr1 = (byte*) pop();
	byte* addr0 = (byte*) pop();
	while (n--) *addr1++ = *addr0++;
}

/* fill ( addr n byte -- ) */
void fill()
{
	byte x = (byte) pop();
	cell n = (cell) pop();
	byte* addr = (byte*) pop();
	while (n--) *addr++ = x;
}

/* key ( -- char ) */
void key()
{
	push((cell) getchar());
}

/* emit ( char -- ) */
void emit()
{
	putchar((int) pop());
}

/* expect ( addr n -- ) */
void expect()
{
	size_t len = (size_t) pop();
	char* addr = (char*) pop();
	fgets(addr, len, stdin);
}

/* type ( addr n -- ) */
void type()
{
	cell count = pop();
	byte* ptr = (byte*) pop();
	while (count--) putchar(*ptr++);
}

/* parse ( char -- addr len ) */
void parse()
{
	byte x = (byte) pop();
	cell addr = (cell) in;
	cell len = 0;
	while (*in)
	{
		len++;
		if ((byte) *in++ == x) break;
	}
	push(addr);
	push(len);
}


/* in according to the traditions of our great fathers */
void ok()
{
	if (fp) return;
	fputs(" ok\n", stdout);
}

/* fetch next line from input stream to buffer */
void query()
{
	in = buffer;
	if (fp)
	{
		if (fgets(buffer, BLOCK_SIZE, fp)) return;
		fclose(fp);
		fp = NULL;
		ok();
	}
	fgets(buffer, BLOCK_SIZE, stdin);
}

/* reset return stack and call query */
void reset()
{
	out = 0;
	rp = r0;
	query();
}

/* return control to terminal and clears data stack */
void aborts()
{
	fp = NULL;
	sp = s0;
	state = 0;
}

/* parse next word in the input stream */
/* word ( -- addr len | null null ) */
void word()
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
		*sp += 1;
		in++;
	}
}

/* parse string as a number */
/* number ( addr len -- number null | addr len ) */
void number()
{
	cell len = pop();
	cell addr = pop();
	cell count = 0;
	cell result = 0;
	cell negate = 0;
	byte* ptr = (byte*) addr;
	if (*ptr == 0x2d && len > 1)
	{
		negate = -1;
		ptr++;
		count++;
	}
	while (count++ < len)
	{
		result *= 10;
		byte w = *ptr++ - 0x30;
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

cell memequ(byte* addr0, byte* addr1, cell count)
{
	while (count)
	{
		if (*addr0++ != *addr1++) break;
		count--;
	}
	return count;
}

/* find ( addr len -- header | null ) */
void find()
{
	byte len0 = (byte) pop();
	byte* name0 = (byte*) pop();
	cell* ptr = (cell*) latest;
	while (ptr)
	{
		push((cell) ptr);
		to_count();
		count();
		byte len1 = (byte) pop();
		byte* name1 = (byte*) pop();
		if (!(len1 & FLAG_HIDDEN))
		if ((len1 & FLAG_LENMASK) == len0)
		if (!(memequ(name0, name1, len0)))
			break;
		ptr = (cell*) *ptr;
	}
	push((cell) ptr);
}

/* execute ( ptr -- ) */
void execute()
{
	rpush((cell) ip);
	ip = (code*) pop();
}

/* literal ( n -- ) IMMEDIATE */
void literal()
{
	cell_append((cell) lit);
	cell_append((cell) pop());
}

/* compile ( header -- ) */
void compile()
{
	dup();
	to_count();
	byte len = *((byte*) pop());
	to_body();
	if (len & FLAG_PRIMITIVE)
	{
		cell_append(*((cell*) pop()));
	}
	else
	{
		cell_append((cell) call);
		cell_append((cell) pop());
	}
}

void is_out()
{
	push(out);
}

/* interpret ( -- ) */
void interpret()
{
	word(); /* ( -- addr len | null null ) */
	if (!peek())
	{
		drop();
		drop();
		out = -1;
		return;
	}
	number(); /* ( addr len -- numb null | addr len ) */
	if (!peek())
	{
		drop();
		if (state) literal();
		return;
	}
	cell len = *sp;
	cell addr = *(sp - 1);
	find(); /* ( addr len -- header | null ) */
	if (!peek())
	{
		drop();
		fputs("Word not found: ", stdout);
		push(addr);
		push(len);
		type();
		putchar('\n');
		aborts();
		out = -1;
		return;
	}
	if (state)
	{
		dup();
		to_count();
		byte flag = *((byte*) pop()) & FLAG_IMMEDIATE;
		if (flag) goto immediate;
		compile();
		return;
	}
	immediate:
	to_body();
	execute();
}

/* c, ( a -- ) */
void byte_comma()
{
	byte_append((byte) pop());
}

/* , ( a -- ) */
void comma()
{
	cell_append((cell) pop());
}

/* s, ( a -- ) */
void string_comma()
{
	cell len = pop();
	byte* addr = (byte*) pop();
	string_append(addr, len);
}

/* header ( addr len -- ) */
void _header()
{
	byte len = (byte) pop();
	byte* addr = (byte*) pop();
	header(len, addr);
}

/* to immediate mode [ ( -- ) immediate */
void lbrac()
{
	state = 0;
}

/* to compile mode ] ( -- ) */
void rbrac()
{
	state = 1;
}

/* DEBUG */
void dump(byte* p, size_t size)
{
        size_t i = 0, j = 0, c = 0;

        for (i; i < size; i++)
        {
                printf("%02x ", p[i]);
                if (i - j >= 7)
                {
                        for (j; j <= i; j++)
                        {
                                c = (p[j] >= ' ' && p[j] < '~') ? p[j] : '.';
				putchar(c);
                        }
			putchar('\n');
                } 
        }
}

/* dump ( addr len -- ) */
void _dump()
{
	size_t size = (size_t) pop();
	byte* addr = (byte*) pop();
	dump(addr, size);
}

/* ( addr len -- ) */
void include()
{
	size_t len = (size_t) pop();
	byte* addr = (byte*) pop();
	byte* buffer = malloc(len + 1);

	if (!buffer)
	{
		perror(NULL);
		return;	
	}

	byte* ptr = buffer;
	while (len--) *ptr++ = *addr++;
	*ptr = '\0';
	fp = fopen(buffer, "r");
	free(buffer);
}


/* Architecture */
#if defined(__x86_64__) || defined(_M_X64)
byte arch[] = "x86_64";
#elif defined(__i386) || defined(_M_IX86)
byte arch[] = "i386";
#elif defined(__aarch64__)
byte arch[] = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
byte arch[] = "arm";
#elif defined(__riscv)
byte arch[] = "riscv";
#else
byte arch[] = "unknown";
#endif

/* Operating system */
#if defined(_WIN32)
byte os[] = "windows";
#elif defined(__linux__)
byte os[] = "linux";
#elif defined(__APPLE__) && defined(__MACH__)
byte os[] = "macos";
#elif defined(__unix__)
byte os[] = "unix";
#else
byte os[] = "unknown";
#endif


int main()
{
	dictionary = malloc(DICTIONARY_SIZE);

	if (!dictionary)
	{
		perror(NULL);
		return 1;
	}

	here = dictionary;

	header(4 | FLAG_PRIMITIVE, "drop");
	cell_append((cell) drop);
	cell_append((cell) ret);

	header(3 | FLAG_PRIMITIVE, "dup");
	cell_append((cell) dup);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "over");
	cell_append((cell) over);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "swap");
	cell_append((cell) swap);
	cell_append((cell) ret);

	header(3 | FLAG_PRIMITIVE, "rot");
	cell_append((cell) rot);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, ">r");
	cell_append((cell) to_r);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, "r>");
	cell_append((cell) r_from);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, "2*");
	cell_append((cell) left_shift);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, "2/");
	cell_append((cell) right_shift);
	cell_append((cell) ret);

	header(3 | FLAG_PRIMITIVE, "not");
	cell_append((cell) not);
	cell_append((cell) ret);

	header(3 | FLAG_PRIMITIVE, "and");
	cell_append((cell) and);
	cell_append((cell) ret);

	header(3 | FLAG_PRIMITIVE, "xor");
	cell_append((cell) xor);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, "or");
	cell_append((cell) or);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, "+");
	cell_append((cell) add);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, "-");
	cell_append((cell) sub);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, "*");
	cell_append((cell) mul);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, "/");
	cell_append((cell) quot);
	cell_append((cell) ret);

	header(3 | FLAG_PRIMITIVE, "mod");
	cell_append((cell) rem);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, "<");
	cell_append((cell) lt);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, "=");
	cell_append((cell) eq);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, ">");
	cell_append((cell) gt);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, "c@");
	cell_append((cell) cfetch);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, "c!");
	cell_append((cell) cstore);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, "@");
	cell_append((cell) fetch);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, "!");
	cell_append((cell) store);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, "+!");
	cell_append((cell) addstore);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "move");
	cell_append((cell) move);
	cell_append((cell) ret);

	header(5 | FLAG_PRIMITIVE, "cmove");
	cell_append((cell) cmove);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "fill");
	cell_append((cell) fill);
	cell_append((cell) ret);

	header(3 | FLAG_PRIMITIVE, "key");
	cell_append((cell) key);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "emit");
	cell_append((cell) emit);
	cell_append((cell) ret);

	header(6 | FLAG_PRIMITIVE, "expect");
	cell_append((cell) expect);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "type");
	cell_append((cell) type);
	cell_append((cell) ret);

	header(5 | FLAG_PRIMITIVE, "parse");
	cell_append((cell) parse);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "call");
	cell_append((cell) call);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "exit");
	cell_append((cell) ret);
	cell_append((cell) ret);

	header(3 | FLAG_PRIMITIVE, "lit");
	cell_append((cell) lit);
	cell_append((cell) ret);

	header(6 | FLAG_PRIMITIVE, "branch");
	cell_append((cell) branch);
	cell_append((cell) ret);

	header(7 | FLAG_PRIMITIVE, "0branch");
	cell_append((cell) zbranch);
	cell_append((cell) ret);

	header(9 | FLAG_PRIMITIVE, "litstring");
	cell_append((cell) litstring);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, "c,");
	cell_append((cell) byte_comma);
	cell_append((cell) ret);

	header(1 | FLAG_PRIMITIVE, ",");
	cell_append((cell) comma);
	cell_append((cell) ret);

	header(2 | FLAG_PRIMITIVE, "s,");
	cell_append((cell) string_comma);
	cell_append((cell) ret);

	header(5 | FLAG_PRIMITIVE, "align");
	cell_append((cell) align);
	cell_append((cell) ret);

	header(6 | FLAG_PRIMITIVE, "header");
	cell_append((cell) _header);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "word");
	cell_append((cell) word);
	cell_append((cell) ret);

	header(6 | FLAG_PRIMITIVE, "number");
	cell_append((cell) number);
	cell_append((cell) ret);

	header(4 | FLAG_PRIMITIVE, "find");
	cell_append((cell) find);
	cell_append((cell) ret);

	header(7 | FLAG_PRIMITIVE, "execute");
	cell_append((cell) execute);
	cell_append((cell) ret);

	header(7 | FLAG_PRIMITIVE | FLAG_IMMEDIATE, "literal");
	cell_append((cell) literal);
	cell_append((cell) ret);

	header(8 | FLAG_PRIMITIVE, "compile,");
	cell_append((cell) compile);
	cell_append((cell) ret);

	header(5 | FLAG_PRIMITIVE, ">body");
	cell_append((cell) to_body);
	cell_append((cell) ret);

	header(6 | FLAG_PRIMITIVE, ">count");
	cell_append((cell) to_count);
	cell_append((cell) ret);

	header(5 | FLAG_PRIMITIVE, "count");
	cell_append((cell) count);
	cell_append((cell) ret);

	header(10, "dictionary");
	cell_append((cell) lit);
	cell_append((cell) &dictionary);
	cell_append((cell) ret);

	header(6, "latest");
	cell_append((cell) lit);
	cell_append((cell) &latest);
	cell_append((cell) ret);

	header(4, "here");
	cell_append((cell) lit);
	cell_append((cell) &here);
	cell_append((cell) ret);

	header(5, "state");
	cell_append((cell) lit);
	cell_append((cell) &state);
	cell_append((cell) ret);
	
	header(6, "buffer");
	cell_append((cell) lit);
	cell_append((cell) &buffer);
	cell_append((cell) ret);
	
	header(2, "in");
	cell_append((cell) lit);
	cell_append((cell) &in);
	cell_append((cell) ret);
	
	header(2, "sp");
	cell_append((cell) lit);
	cell_append((cell) &sp);
	cell_append((cell) ret);
	
	header(2, "S0");
	cell_append((cell) lit);
	cell_append((cell) s0);
	cell_append((cell) ret);
	
	header(3, "rsp");
	cell_append((cell) lit);
	cell_append((cell) &rp);
	cell_append((cell) ret);
	
	header(3, "RS0");
	cell_append((cell) lit);
	cell_append((cell) r0);
	cell_append((cell) ret);

	header(1 | FLAG_IMMEDIATE, "[");
	cell_append((cell) lit);
	cell_append((cell) 0);
	cell_append((cell) lit);
	cell_append((cell) &state);
	cell_append((cell) store);
	cell_append((cell) ret);

	header(1, "]");
	cell_append((cell) lit);
	cell_append((cell) -1);
	cell_append((cell) lit);
	cell_append((cell) &state);
	cell_append((cell) store);
	cell_append((cell) ret);
	
	header(4, "CELL");
	cell_append((cell) lit);
	cell_append((cell) sizeof(cell));
	cell_append((cell) ret);
	
	header(14, "FLAG_IMMEDIATE");
	cell_append((cell) lit);
	cell_append((cell) FLAG_IMMEDIATE);
	cell_append((cell) ret);
	
	header(11, "FLAG_HIDDEN");
	cell_append((cell) lit);
	cell_append((cell) FLAG_HIDDEN);
	cell_append((cell) ret);
	
	header(14, "FLAG_PRIMITIVE");
	cell_append((cell) lit);
	cell_append((cell) FLAG_PRIMITIVE);
	cell_append((cell) ret);
	
	header(12, "FLAG_LENMASK");
	cell_append((cell) lit);
	cell_append((cell) FLAG_LENMASK);
	cell_append((cell) ret);

	header(4, "quit");
	cell_append((cell) reset);
	cell_append((cell) interpret);
	cell_append((cell) is_out);
	cell_append((cell) zbranch);
	cell_append((cell) -3 * sizeof(cell));
	cell_append((cell) ok);
	cell_append((cell) branch);
	cell_append((cell) -7 * sizeof(cell));

	push((cell) latest);
	to_body();
	ip = (code*) pop();

	header(5, "abort");
	cell_append((cell) aborts);
	cell_append((cell) call);
	cell_append((cell) ip);

	header(7, "include");
	cell_append((cell) word);
	cell_append((cell) include);
	cell_append((cell) ret);

	/* DEBUG */
	header(4 | FLAG_PRIMITIVE, "dump");
	cell_append((cell) _dump);
	cell_append((cell) ret);
	
	printf("Welcome to lightforth\n");
	printf("Built for %s-%s, version %d\n", arch, os, VERSION);
	printf("lightforth comes with ABSOLUTELY NO WARRANTY\n");

	fp = fopen("basic.fs", "r");

	while (ip) (*ip++)();

	return 0;
}
