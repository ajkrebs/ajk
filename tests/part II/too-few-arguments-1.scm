;#lang racket

(define globe #t)
(define bool-lst '(#t #f #t))
(define (my-or-two-args a b)
         (if a
             a
             b))

(set! globe (foldr (lambda (b a) (and b a)) #t bool-lst))
 
(define george 
  (let ([x '1])
    (begin
      (set! x (* (+ x '3) '5))
      (when (> x '2) (set! x (* (+ x '3) '5)))
      (unless (> x '2) (set! x (* (+ x '4) '2)))
      (when (< x '4) (set! x (* (+ x '9) '4)))
      (unless '#f (set! x (* (+ x '2) '7)))
      (if x (set! x (* x x)) '7)
      (if '#f (set! x '0) '9)
      x)))


(set! globe (and (begin (set! globe #f) globe) #t))
(my-or-two-args globe)