
(define y 10)
(define z 20)

(let ([k (lambda (x y z) (+ x y z))])
      (set! y (k '1 '2 '3)))

(y 'hello)