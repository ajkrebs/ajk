(begin
  (define (f x)
    (g x (+ x 10)))
  'done)

(define r
  (letrec* ()
           0
           (letrec ()
             1
             (let* ()
               2
               (let ()
                 3
                 (define f (lambda (g) g))
                 (f '4))))))

(begin
  'a
  'b
  'c
  (define (g u v)
    (+ u r v)))

(f 10 11)
