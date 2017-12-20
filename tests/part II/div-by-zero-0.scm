
(define y 10)
(define x 20)
(define z 0)

(define r (let ([lst `(1 2 ,y ,z)])
            (/ (foldl + 0 lst) 0)))
r