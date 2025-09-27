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


\ CONTROL FLOW

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

\ fetch nth cell from stack, counting from 0
: pick ( n -- )
  2 +
  CELL *
  sp @
  swap -
  @
;

\ fetch top of return stack
: r@ ( -- n )
  rsp @
  CELL -
  @
;

\ get current stack depth
: depth ( -- n )
  sp @
  S0 -
  CELL -
  CELL /
;


\ STRINGS 

\ convert cell count to bytes
: cells ( count -- bytes ) CELL * ;

\ return ascii value for '"'
: '"' ( -- 34 ) [ char " ] literal ;

\ parse string from input buffer, delimited by '"'
: parse"
  '"' parse
  swap 1 +
  swap 2 -
;

\ store string into dictionary space
: s" ( -- addr len )
  state @ if
    postpone litstring
    here @ 2 cells + ,
    parse" dup ,
    begin
    dup while
      1 - swap
      dup c@ c,
      1 + swap
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


\ MISC

\ exit forth
: bye ( -- ) 0 >r ;

\ print newline
: cr ( -- ) 10 emit ;

\ print null string
: puts ( addr -- )
  begin
  dup c@ while
    dup c@ emit
    1 +
  repeat
  drop
;

: space
  32 emit
;

: words
  latest @
  begin
    dup >count count
    FLAG_LENMASK and type space
    @
  dup 0 = until
  drop
;

\ see memory content of latest word
: recent ( -- )
  latest @ 20 cells dump
;

\ see memory content of next word
: see ( n "name" -- )
  cells '
  dup 0 = if
    ." Word not found" cr
    2drop exit
  then
  swap dump
;


\ STARTUP MESSAGE
." Welcome to lightforth" cr
." Built for " arch puts ." -" os puts ." , version 20250926" cr
." lightforth comes with ABSOLUTELY NO WARRANTY" cr
