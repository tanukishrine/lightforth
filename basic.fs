word create header ]
  word header exit
[

create immediate ]
  latest @ >count
  dup c@ FLAG_IMMEDIATE xor
  swap c!
  exit
[ immediate

create hidden ]
  latest @ >count
  dup c@ FLAG_HIDDEN xor
  swap c!
  exit
[

create : ]
  create hidden
  ]
  exit
[

create ; immediate
  word exit find literal
  ] compile, [
  word [ find literal
  ] >body execute hidden exit [

: char
  word drop c@
;

: (
  [ char ) ] literal parse drop drop
; immediate

: \
  [ 0 ] literal parse drop drop
; immediate

\ for compiling immediate words during compilation
: [compile] ( "name" -- )
  word find compile,
; immediate 

\ 'tick' find the header of a word
: ' ( "name" -- header )
  word find
;

\ for during compilation mode
: ['] ( "name" -- header ) '
; immediate

\ compile word to current definition
: postpone ( "name" -- )
  '
  [compile] literal
  ['] compile, literal compile,
; immediate

\ convert cell count to bytes
: cells ( count -- bytes )
  CELL *
;


\ CONTROL STRUCTURES

: recurse ( -- ) \ redefine, to prevent stack overflow
  latest @ compile,
; immediate

: if ( -- here )
  postpone 0branch
  here @
  0 ,
; immediate

: then ( here -- )
  dup here @ swap -
  swap !
; immediate

: else ( here0 -- here1 )
  postpone branch
  here @
  0 ,
  swap dup here @ swap -
  swap !
; immediate

: begin ( -- here )
  here @
; immediate

: until ( here -- )
  postpone 0branch
  here @ - ,
; immediate

: again ( here -- )
  postpone branch
  here @ - ,
; immediate

: while ( here0 -- here0 here1 )
  postpone 0branch
  here @
  0 ,
; immediate

: repeat ( here0 here1 -- )
\ ['] branch literal compile,
  postpone branch
  swap here @ - ,
  dup here @ swap -
  swap !
; immediate

: unless ( -- here )
  postpone not
  [compile] if
; immediate

: do ( -- here )
  postpone >r
  postpone lit
  1 ,
  postpone -
  postpone >r
  here @
; immediate

: loop ( here -- )
  postpone r>
  postpone r>
  postpone lit
  1 ,
  postpone +
  postpone over
  postpone over
  postpone >r
  postpone >r
  postpone <
  postpone 0branch
  here @ - ,
  postpone r>
  postpone r>
  postpone drop
  postpone drop
; immediate

: +loop ( here -- )
  postpone r>
  postpone swap 
  postpone r>
  postpone +
  postpone over
  postpone over
  postpone >r
  postpone >r
  postpone <
  postpone 0branch
  here @ - ,
  postpone r>
  postpone r>
  postpone drop
  postpone drop
; immediate

: i ( -- i )
  r> r> r>
  dup >r
  swap >r
  swap >r
;

: j ( -- j )
  r> r> r> r> r>
  dup >r
  swap >r
  swap >r
  swap >r
  swap >r
;

: leave ( -- )
  r> r> r> drop dup >r >r >r
;


\ STACK MANIPULATION

: nip ( a b -- b )
  swap drop
;

: tuck ( a b -- b a b )
  swap over
;

: 2dup ( a b -- a b a b )
  over over
;

: 2drop ( a b -- )
  drop drop
;

\ fetch nth cell from stack
: pick ( n -- )
  2 +
  cells
  sp @
  swap -
  @
;

\ duplicate only if non-zero
: ?dup ( n -- ?n )
  dup if dup then
;

\ fetch top of return stack
: r@ ( -- n )
  rsp @
  CELL -
  @
;

\ drop top of return stack
: rdrop ( R: n -- )
  r> r>
  drop >r
;

\ get current stack depth
: depth ( -- n )
  sp @
  S0 -
  CELL -
  CELL /
;


\ COMPARISON

: <> ( n1 n2 -- f )
  = not
;

: 0< ( n -- f )
  0 <
;

: 0= ( n -- f )
  0 =
;

: 0> ( n -- f )
  0 >
;

: 0<> ( n -- f )
  0 <>
;


\ ARITHMETIC AND LOGICAL

: 1+ ( n -- n+1 )
  1 +
;

: 1- ( n -- n-1 )
  1 -
;

: 2+ ( n -- n+2 )
  2 +
;

: 2- ( n -- n-2 )
  2 -
;

: cell+ ( n -- n+CELL )
  CELL +
;

: cell- ( n -- n-CELL )
  CELL -
;

: /mod ( n1 n2 -- rem quot )
  2dup / >r mod r>
;

: max ( n1 n2 -- max )
  2dup > if drop else nip then
;

: min ( n1 n2 -- min )
  2dup < if drop else nip then
;

: negate ( n -- -n )
  0 swap -
;

: abs ( n -- |n| )
  dup 0< if negate then
;


\ MEMORY

\ print content at "addr"
: ? ( addr -- )
  @ .
;


\ DEFINING WORDS

: constant ( n "name" -- )
  create
  postpone lit
  ,
  postpone exit
;

: allot ( n -- )
  begin
  dup while
    1- 0 c,
  repeat
  drop
;

: variable ( "name" -- )
  here @ CELL allot
  constant
;

\ potential array implimentation
\ : array ( n "name" -- )
\   create
\   postpone lit
\   here @ 2 cells + ,
\   postpone exit
\   cells allot
\ ;

: forget ( "name" -- )
  word find
  dup @ latest !
  here !
;


\ TERMINAL INPUT-OUTPUT

\ print newline
: cr ( -- ) 10 emit ;

\ print space
: space ( -- ) 32 emit ;

\ print "n" count of spaces
: spaces ( n -- )
  begin
  dup while
    1- space
  repeat
  drop
;

\ return ascii value for '-'
: '-' ( -- 45 )
  [ char - ] literal
;

\ return ascii value for '0'
: '0' ( -- 48 )
  [ char 0 ] literal
;

\ return ascii value for 'a'
: 'a' ( -- 97 )
  [ char a ] literal
;

\ return ascii value for '['
: '[' ( -- 91 )
  [ char [ ] literal
;

\ return ascii value for ']'
: ']' ( -- 93 )
  [ char ] ] literal
;

\ print "n" as a decimal
: . ( n -- )
  dup 0< if
    '-' emit
    negate
  then
  0 >r
  begin
    r> 1+ >r
    10 /mod
  dup 0= until
  drop
  begin
  r@ while
    r> 1- >r
    '0' + emit
  repeat
  rdrop
  space
;

\ show stack content
: .s ( -- )
  '[' emit space
  depth
  begin
  dup while
    dup pick .
    1-
  repeat
  drop
  ']' emit space
;


\ STRINGS 

\ return ascii value for '"'
: '"' ( -- 34 ) [ char " ] literal ;

\ parse string from input buffer, delimited by '"'
: parse"
  '"' parse
  swap 1+
  swap 2-
;

\ prints null string
: puts ( addr -- )
  begin
  dup c@ while
    dup c@ emit
    1+
  repeat
  drop
;

\ store string into dictionary space
: s" ( -- addr len )
  state @ if
    postpone litstring
    here @ 2 cells + ,
    parse" dup ,
    begin
    dup while
      1- swap
      dup c@ c,
      1+ swap
    repeat
    2drop
    align
  else
    parse" >r
    here @ r@
    cmove
    here @ r>
  then
; immediate

\ print string
: ." ( -- )
  state @ if
    [compile] s"
    postpone type
  else
    parse"
    type
  then
; immediate


\ MISCELLANEOUS

\ scratch buffer, temporary storage space
create pad ( -- addr )
  ' lit compile,
  here @ 2 cells + ,
  ' exit compile,
  64 allot

\ exit forth
: bye ( -- ) 0 >r ;

\ list all defined words
: words
  latest @
  begin
    dup >count count
    FLAG_LENMASK and type space
    @
  dup 0 = until
  drop
;

\ memory content of latest word
: recent ( n -- )
  latest @ cells dump
;

\ memory content of next word
: see ( n "name" -- )
  cells '
  dup 0 = if
    ." Unknown word" cr
    2drop exit
  then
  swap dump
;
