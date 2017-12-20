;#lang racket
; testing strings as key values.

(begin
   (define r 220)
   (let ([h (prim hash)])
     (let ([h0 (prim hash-set h "key0" 2)])
       (let ([h1 (prim hash-set h0 "key1" 2)])
         (let ([v1 (prim hash-ref h1 "key0")])
           (set! r (cons v1 (prim hash-count h1)))))))
   r)