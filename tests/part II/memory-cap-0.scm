;#lang racket

(define (x c)
  (if (c > 0)
        (let
            ([h (prim hash)])
          (begin
            (prim hash-set h 2 3)
            (x (+ c 1))))
        c))
