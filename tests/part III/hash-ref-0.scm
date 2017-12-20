;#lang racket

(begin
   (define r 220)
   (let ([h (prim hash)])
     (let ([h0 (prim hash-set h 1 2)])
       (let ([h1 (prim hash-set h0 3 2)])
         (let ([h2 (prim hash-remove h1 3)]) 
             (let ([v1 (prim hash-ref h2 24 (lambda () 'lol))])
               (set! r v1))))))
   r)