
(define globe 0)

(let ()
  (begin 
    (set! globe (+ globe 5))))

(set! globe (- globe 3))

(letrec
    ()
  (set! globe (- (* globe 10) 6)))

(/ globe (- (+ (- globe 4) (- globe 10)) 14))

