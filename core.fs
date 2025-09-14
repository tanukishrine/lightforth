create :
  ] create hidden ] exit [

create ; immediate
  parse exit find literal
  ] compile, [
  parse [ find literal
  ] >body execute hidden exit [

: \
  -1 eol !
; immediate

: bye 0 ip ! ;

: clearstack S0 sp ! ;


: '
  parse find
;

: [']
  ' literal
; immediate

: compile ' compile, ;

: hide ' >count dup c@ FLAG_HIDDEN xor swap c! ;


: count dup c@ swap 1+ ;

: cells CELL * ;

: ? @ . ;


\ CONTROL STRUCTURES

: recurse
  latest @ compile,
; immediate

: if
  [ ' 0branch literal ]
  compile,
  here @
  0 ,
; immediate

: then
  dup
  here @ swap negate +
  swap cells latest @ >body + !
; immediate

: else
  [ ' branch literal ]
  compile,
  here @
  0 ,
  swap
  dup
  here @ swap negate +
  swap cells latest @ >body + !
; immediate


: begin
  here @
; immediate

: until
  [ ' 0branch literal ]
  compile,
  here @ negate +
  ,
; immediate

: again
  [ ' branch literal ]
  compile,
  here @ negate +
  ,
; immediate

: while
  [ ' 0branch literal ]
  compile,
  here @
  0 ,
; immediate
: then
  dup
  here @ swap negate +
  swap cells latest @ >body + !
; immediate

: repeat
  [ ' branch literal ]
  compile,
  swap
  here @ negate + ,
  dup
  here @ swap negate +
  swap cells latest @ >body + !
; immediate

\ refactor above


: unless
  ['] not compile,
  ['] if >body execute
; immediate


\ parenthesis comment delimited by ')'
: (
  begin
    char
    dup
    0 = if
      drop exit
    then
    41 = if
      exit
    then
  again
; immediate


\ stack manipulation

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


\ terminal input-output

: cr ( -- )
  10 emit
;

: space ( -- )
  32 emit
;

: spaces ( n -- )
  begin
  dup 0> while
    1- space
  repeat
  drop
;

: type ( c-addr u -- )
  begin
  dup 0> while
    swap
    dup c@ emit
    1+ swap 1-
  repeat
  2drop
;

: &here ( -- addr )
  latest >body
  here @ cells +
;

: callot ( u -- )
  CELL / 1+ allot
;

: parse" ( -- c-addr u )
  parse
  dup >r
  pad swap
  cmove
  pad r>
; immediate

: constant
  create
  literal
  ['] exit compile,
;

: variable
  create
  latest @ >body 3 cells + literal
  ['] exit compile,
  1 allot
;


: ." ( -- ) \ interpret mode only
  char drop
  begin
    char
    dup 0 = if
      drop exit
    then
    dup 34 = if
      drop exit
    then
    emit
  again
;


." Welcome to lightforth" cr

include example.fs
