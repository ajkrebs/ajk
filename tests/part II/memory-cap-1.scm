

(define x '(1 2 3 4))

(map (lambda (y) (set! x (cons y x)) (range (1 10000000))))
