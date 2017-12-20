;#lang racket

(define lam (lambda (x) (+ x 2)))
(define lam0 (lambda (x y z) (+ x y z)))

(define (my-map f list)
  (cond
    [(null? list) '()]
    [(cons (f (car list)) (my-map f (cdr list)))]))

(define list0 (my-map lam '(1 2 3)))

(lam0 (car list0) (car (cdr list0)))

