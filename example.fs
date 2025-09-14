: count ( n -- )
  >r 0
  begin
    dup .
    1+
  dup r@ =
  until
  r> drop
;

: fib ( n -- )
  >r 0 >r
  0 1
  begin
    over . cr
    2dup +
    rot drop
  r> 1+ r>
  2dup >r >r
  >
  until
  r> r> 2drop
  2drop
;
