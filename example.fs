: example ( -- )
  0
  begin
    dup . 10 emit
    1+
  dup 10000000 =
  until
;

: fib ( n -- )
  >r 0 1
  begin
    over . cr
    2dup +
    rot drop
  over r@ >
  until
  r> drop
;
