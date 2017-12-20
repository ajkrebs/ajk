#lang racket

(require "utils.rkt")

(provide simplify-ae
         remove-varargs
         callsites-lambdas
         shallow
         compute-vararg-set
         0-cfa
         successors
         closure-convert
         proc->llvm)


; Whether to optimize vararg conversion using static analysis (0-cfa) results
(define optimize-varargs #t)


; Pass that removes lambdas and datums as atomic and forces them to be let-bound
;   ...also performs a few small optimizations
(define (simplify-ae e)
  (define (wrap-aes aes wrap)
    (match-define (cons xs wrap+)
                  (foldr (lambda (ae xs+wrap)
                           (define gx (gensym 'arg))
                           (if (symbol? ae)
                               (cons (cons ae (car xs+wrap))
                                     (cdr xs+wrap))
                               (cons (cons gx (car xs+wrap))
                                     (lambda (e)
                                       (match ae
                                              [`(lambda ,xs ,body) 
                                               `(let ([,gx (lambda ,xs ,(simplify-ae body))])
                                                  ,((cdr xs+wrap) e))]
                                              [`',dat
                                               `(let ([,gx ',dat])
                                                  ,((cdr xs+wrap) e))])))))
                         (cons '() wrap)
                         aes))
    (wrap+ xs))
  (match e
         [`(let ([,x (lambda ,xs ,elam)]) ,e0)
          `(let ([,x (lambda ,xs ,(simplify-ae elam))]) ,(simplify-ae e0))]

         [`(let ([,x ',dat]) ,e0)
          `(let ([,x ',dat]) ,(simplify-ae e0))]

         [`(let ([,x (prim ,op ,aes ...)]) ,e0)
          (wrap-aes aes (lambda (xs) `(let ([,x (prim ,op ,@xs)]) ,(simplify-ae e0))))]
         [`(let ([,x (apply-prim ,op ,aes ...)]) ,e0)
          (wrap-aes aes (lambda (xs) `(let ([,x (apply-prim ,op ,@xs)]) ,(simplify-ae e0))))]

         [`(if (lambda . ,_) ,et ,ef)
          (simplify-ae et)]
         [`(if '#f ,et ,ef)
          (simplify-ae ef)]
         [`(if ',dat ,et ,ef)
          (simplify-ae et)]
         [`(if ,(? symbol? x) ,et ,ef)
          `(if ,x ,(simplify-ae et) ,(simplify-ae ef))]

         [`(apply (lambda ,xs ,ebody) ,ae)
          (define xf (gensym 'lam))
          (simplify-ae
           `(let ([,xf (lambda ,xs ,ebody)])
              (apply ,xf ,ae)))]
         [`(apply ,(? symbol? x) (lambda ,xs ,ebody))
          (define xf (gensym 'lam))
          (simplify-ae
           `(let ([,xf (lambda ,xs ,ebody)])
              (apply ,x ,xf)))]
         [`(apply ',dat ,ae)
          (pretty-print '(warning: applying datum))
          `',dat]
         [`(apply ,(? symbol? x) ',dat)
          (define xd (gensym 'datum))
          `(let ([,xd ',dat])
             (apply ,x ,xd))]
         [`(apply ,(? symbol? x0) ,(? symbol? x1))
          `(apply ,x0 ,x1)]
         
         [`(,aes ...)
          (wrap-aes aes (lambda (xs) xs))]))


(define (successors exp store) 
  (define (lookup x) (hash-ref store x set)) 
  (match exp
         [`(let ([,x (quote ,dat)]) ,e0)
          (cons (set e0) store)]
         [`(let ([,x (lambda ,xs ,elam)]) ,e0)
          (cons (set e0) (store-extend store x `(lambda ,xs ,elam)))]
         [`(let ([,x (prim vector ,as ...)]) ,e0)
          (cons (set e0) (store-extend store x (set-union (set x) (foldl set-union (set) (map lookup as)))))]
         [`(let ([,x (apply-prim vector ,args)]) ,e0)
          (cons (set e0) (store-extend store x (set-union (set x) (lookup args))))]
         [`(let ([,x (prim make-vector ,a ,b)]) ,e0)
          (cons (set e0) (store-extend store x (set-union (set x) (lookup a) (lookup b))))]
         [`(let ([,x (apply-prim make-vector ,args)]) ,e0)
          (cons (set e0) (store-extend store x (set-union (set x) (lookup args))))]
         [`(let ([,x (prim vector-set! ,a ,b ,c)]) ,e0)
          (cons (set e0) (foldl (lambda (a store) (store-extend store a (lookup c)))
                                store
                                (filter symbol? (set->list (lookup a)))))]
         [`(let ([,x (apply-prim vector-set! ,a)]) ,e0)
          (cons (set e0) (foldl (lambda (a store) (store-extend store a (lookup a)))
                                store
                                (filter symbol? (set->list (lookup a)))))]
         [`(let ([,x (prim vector-ref ,a ,bs ...)]) ,e0)
          (cons (set e0) (store-extend store x (foldl (lambda (a vs)
                                                        (set-union vs (lookup a)))
                                                      (set)
                                                      (filter symbol? (set->list (lookup a))))))]
         [`(let ([,x (apply-prim vector-ref ,args)]) ,e0)
          (cons (set e0) (store-extend store x (foldl (lambda (a vs)
                                                        (set-union vs (lookup a)))
                                                      (set)
                                                      (filter symbol? (set->list (lookup args))))))]
         [`(let ([,x (prim ,op ,as ...)]) ,e0)
          (cons (set e0) (store-extend store x (foldl set-union (set) (map lookup as))))]
         [`(let ([,x (apply-prim ,op ,a)]) ,e0)
          (cons (set e0) (store-extend store x (lookup a)))]
         [`(if ,ae ,e0 ,e1)
          (cons (set e0 e1) store)]
         [`(apply ,f ,a)
          (define vs (lookup a))
          (foldl (lambda (lam succs+store)
                   (match-define (cons succs store) succs+store)
                   (match lam
                          [`(lambda (,xs ...) ,e0)
                           (cons (set-add succs e0)
                                 (foldl (lambda (x store)
                                          (store-extend store x vs))
                                        store
                                        xs))]
                          [`(lambda ,(? symbol? x) ,e0)
                           (cons (set-add succs e0)
                                 (store-extend store x vs))]
                          [else succs+store]))
                 (cons (set) store)
                 (set->list (lookup f)))]
         [`(,f ,as ...) 
          (foldl (lambda (lam succs+store)
                   (match-define (cons succs store) succs+store)
                   (match lam
                          [`(lambda (,xs ...) ,e0)
                           #:when (= (length xs) (length as))
                           (cons (set-add succs e0)
                                 (foldl (lambda (x a store)
                                          (store-extend store x (lookup a)))
                                        store
                                        xs
                                        as))]
                          [`(lambda ,(? symbol? x) ,e0)
                           (cons (set-add succs e0)
                                 (foldl (lambda (a store)
                                          (store-extend store x (lookup a)))
                                        store
                                        as))]
                          [else succs+store]))
                 (cons (set) store)
                 (set->list (lookup f)))]))


; utility functions
(define (graph-extend graph e0 succs)
  (foldl (lambda (succ graph)
           (graph-extend graph succ (set)))
         (hash-set graph e0 (set-union succs (hash-ref graph e0 set)))
         (set->list succs)))
(define (store-extend store x val)
  (hash-set store x (set-union (store-ref store x) (if (set? val) val (set val)))))
(define (store-ref store x)
  (hash-ref store x set))


; Run 0-CFA analysis (times out and returns (cons #f #f) if too expensive)
(define (0-cfa exp)
  (define start-ms (current-milliseconds))
  (let loop ([graph (hash exp (set))]
             [store (hash)])
    (match-define (cons graph+ store+)
                  (foldl (lambda (e0 graph+store)
                           (match-define (cons graph store) graph+store)
                           (match-define (cons succs store+)
                                         (successors e0 store))
                           (cons (graph-extend graph e0 succs)
                                 store+))
                         (cons graph store)
                         (hash-keys graph)))
    (if (and (equal? graph graph+)
             (equal? store store+))
        (cons graph+ store+)
        (if (> (- (current-milliseconds) start-ms) 8000)
            (begin #;(pretty-print '0-cfa-timed-out)
                   (cons #f #f))
            (loop graph+ store+)))))



; computes a mapping from callsites or lambdas to the corresponding lambdas or callsites (respectively)
(define (callsites-lambdas cfg store)
  (define calls-to-lambdas
   (foldl (lambda (e mp)
            (match e
                   [`(apply ,f ,args)
                      (hash-set mp e (list->set (filter (not/c symbol?) (set->list (hash-ref store f set)))))]
                   [`(,(? (and/c symbol? (not/c reserved?)) f) ,(? symbol? xs) ...)
                      (hash-set mp e (list->set (filter (not/c symbol?) (set->list (hash-ref store f set)))))]
                   [else mp]))
          (hash)
          (hash-keys cfg)))
  ; union calls-to-lambdas with the reverse of itself (lambdas-to-callsites)
  (foldl (lambda (call mp)
           (foldl (lambda (lam mp)
                    (hash-set mp lam (set-add (hash-ref mp lam set) call)))
                  mp
                  (set->list (hash-ref mp call set))))
         calls-to-lambdas
         (hash-keys calls-to-lambdas)))


(define (shallow e [n 2])
  (match e
   [`(,es ...) #:when (<= n 0) '...]
   [`(,es ...) (map (lambda (e) (shallow e (- n 1))) es)]
   [else e]))


; Computes a set of lambdas/callsites to be converted to use variable arguments
; (using the output of callsites-lambdas)
(define (compute-vararg-set clm)
  (define (iterate-set st)
    (foldl (lambda (call/lam st)
             (match call/lam
                    [(or `(,(? (and/c symbol? (not/c reserved?)) _) ,(? symbol? xs) ...)
                         `(lambda (,xs ...) ,_))
                     (define lst (set->list (hash-ref clm call/lam set)))
                     (if (< 1 (set-count (foldl set-union
                                                (set (length xs))
                                                (map (match-lambda
                                                       [(? ((curry set-member?) st)) (set 0 1)]
                                                       [`(lambda (,ys ...) ,_) (set (length ys))]
                                                       [`(lambda ,y ,_) (set 0 1)]
                                                       [`(apply ,f ,y) (set 0 1)]
                                                       [`(,f ,ys ...) (set (length ys))])
                                                     lst))))
                         (set-add st call/lam)
                         st)]
                    [(or `(apply ,_ ,_)
                         `(lambda ,_ ,_))
                     (set-union st (list->set (filter (match-lambda
                                                        [`(lambda ,(? symbol? x) ,body) #f]
                                                        [`(apply ,f ,x) #f]
                                                        [else #t])
                                                      (set->list (hash-ref clm call/lam set)))))]))
           st
           (hash-keys clm)))
  (define (fixpoint st)
    (define st+ (iterate-set st))
    (if (equal? st st+)
        st
        (fixpoint st+)))
  (fixpoint (set)))


(define (remove-varargs e vst) 
  (match e
         [`(let ([,x ',dat]) ,e0)
          `(let ([,x ',dat]) ,(remove-varargs e0 vst))]
         [`(let ([,x (prim ,op ,xs ...)]) ,e0)
          `(let ([,x (prim ,op ,@xs)]) ,(remove-varargs e0 vst))]
         [`(let ([,x (apply-prim ,op ,y)]) ,e0)
          `(let ([,x (apply-prim ,op ,y)]) ,(remove-varargs e0 vst))]
         [`(let ([,x (lambda (,xs ...) ,body)]) ,e0)
          #:when (or (not vst) (set-member? vst `(lambda ,xs ,body)))
          (define gx (gensym 'rvp))
          (define gx+e
            (foldr (lambda (x gx+e)
                     (define gx (gensym 'rvp))
                     (cons gx
                           `(let ([,x (prim car ,gx)])
                              (let ([,(car gx+e) (prim cdr ,gx)])
                                ,(cdr gx+e)))))
                   (cons (gensym 'na) (remove-varargs body vst))
                   xs))
          `(let ([,x (lambda (,(car gx+e)) ,(cdr gx+e))])
             ,(remove-varargs e0 vst))]
         [`(let ([,x (lambda (,xs ...) ,body)]) ,e0)
          `(let ([,x (lambda (,@xs) ,(remove-varargs body vst))])
             ,(remove-varargs e0 vst))]
         [`(let ([,x (lambda ,y ,body)]) ,e0)
          `(let ([,x (lambda (,y) ,(remove-varargs body vst))])
             ,(remove-varargs e0 vst))]
         [`(if ,x ,e0 ,e1)
          `(if ,x ,(remove-varargs e0 vst) ,(remove-varargs e1 vst))]
         [`(apply ,f ,args)
          `(,f ,args)] 
         [`(,f ,xs ...)
          #:when (or (not vst) (set-member? vst e))
          (define gx+e
            (let ()
              (define gx (gensym 'rva))
              (foldl (lambda (x gx+e)
                       (define gx (gensym 'rva))
                       (cons
                        gx
                        `(let ([,(car gx+e) (prim cons ,x ,gx)])
                           ,(cdr gx+e))))
                     (cons gx `(,f ,gx))
                     xs)))
          `(let ([,(car gx+e) '()])
             ,(cdr gx+e))]
         [`(,f ,xs ...)
          `(,f . ,xs)]))



(define (T-bottom-up e procs)
  (match e
         [`(let ([,x ',dat]) ,e0)
          (match-define `(,freevars ,e0+ ,procs+) (T-bottom-up e0 procs))
          (define dx (gensym 'd))
          (list (set-remove freevars x)
                `(let ([,x ',dat]) ,e0+)
                procs+)]
         [`(let ([,x (prim ,op ,xs ...)]) ,e0)
          (match-define `(,freevars ,e0+ ,procs+) (T-bottom-up e0 procs))
          (list (set-remove (set-union (list->set xs) freevars) x)
                `(let ([,x (prim ,op ,@xs)]) ,e0+)
                procs+)]
         [`(let ([,x (apply-prim ,op ,y)]) ,e0)
          (match-define `(,freevars ,e0+ ,procs+) (T-bottom-up e0 procs))
          (list (set-remove (set-add freevars y) x)
                `(let ([,x (apply-prim ,op ,y)]) ,e0+)
                procs+)]
         [`(let ([,x (lambda (,xs ...) ,body)]) ,e0)
          (match-define `(,freevars ,e0+ ,procs0+) (T-bottom-up e0 procs))
          (match-define `(,freelambda ,body+ ,procs1+) (T-bottom-up body procs0+))
          (define fx (gensym 'lam))
          (define envx (gensym 'env))
          (define envvars (foldl (lambda (x fr) (set-remove fr x))
                                            freelambda
                                            xs))
          (define envlist (set->list envvars))
          (define body++ (cdr (foldl (lambda (x count+bdy)
                                       (cons (+ 1 (car count+bdy))
                                             `(let ([,x (env-ref ,envx ,(car count+bdy))])
                                                ,(cdr count+bdy))))
                                     (cons 1 body+)
                                     envlist)))
          (list (set-remove (set-union envvars freevars) x)
                `(let ([,x (make-closure ,fx ,@envlist)]) ,e0+)
                `((proc (,fx ,envx ,@xs) ,body++) . ,procs1+))]
         [`(if ,x ,e0 ,e1)
          (match-define `(,freevars0 ,e0+ ,procs0+) (T-bottom-up e0 procs))
          (match-define `(,freevars1 ,e1+ ,procs1+) (T-bottom-up e1 procs0+))
          (list (set-add (set-union freevars0 freevars1) x)
                `(if ,x ,e0+ ,e1+)
                procs1+)]
         [`(,f ,xs ...)
          (list (list->set `(,f ,@xs)) 
                `(clo-app ,f ,@xs)
                procs)]))


; call simplify-ae on input to closure convert, then cfa-0, then walk ast
(define (closure-convert cps)
  (define scps (simplify-ae cps))
  (match-define (cons cfg store) (0-cfa scps))
  (if (and optimize-varargs cfg store)
      (let () 
        (define call-lam-map (callsites-lambdas cfg store))
        (define vararg-set (compute-vararg-set call-lam-map))
        ;(pretty-print `(vararg-set-size ,(set-count vararg-set)))
        (define no-varargs-cps (remove-varargs scps vararg-set))
        (match-define `(,freevars ,main-body ,procs) (T-bottom-up no-varargs-cps '()))
        `((proc (main) ,main-body) . ,procs))
      (let () 
        (define no-varargs-cps (remove-varargs scps #f))
        (match-define `(,freevars ,main-body ,procs) (T-bottom-up no-varargs-cps '()))
        `((proc (main) ,main-body) . ,procs))))


(define (proc->llvm procs)
  (define globals "")
  (define (s-> s) (c-name s))

  (define (comment-line . strs)
    (match-define `(,strs+ ... ,cmt) strs)
    (define line (apply string-append strs+))
    (define extline (string-append line (let loop ([s ""]
                                                   [i (string-length line)])
                                          (if (>= i 85) s (loop (string-append " " s) (+ i 1))))))
    (string-append extline "; " cmt "\n"))

  (define (e->llvm e)
    (match e
           [`(let ([,x '#t]) ,e0)
            (string-append
             (comment-line
              "  %" (s-> x) " = call i64 @const_init_true()"
              "quoted #t")
             (e->llvm e0))]
           [`(let ([,x '#f]) ,e0)
            (string-append
             (comment-line
              "  %" (s-> x) " = call i64 @const_init_false()"
              "quoted #f")
             (e->llvm e0))]
           [`(let ([,x '()]) ,e0)
            (string-append
             (comment-line
              "  %" (s-> x) " = add i64 0, 0"
              "quoted ()")
             (e->llvm e0))]
           [`(let ([,x ',(? integer? dat)]) ,e0)
            (string-append
             (comment-line
              "  %" (s-> x) " = call i64 @const_init_int(i64 " (number->string dat) ")"
              "quoted int")
             (e->llvm e0))]
           [`(let ([,x ',(? string? dat)]) ,e0)
            (define dx (gensym 'str))
            (define lenstr (string-append "[" (number->string (+ 1 (string-length dat))) " x i8]"))
            (set! globals
                  (string-append globals
                      "@" (s-> dx) " = private unnamed_addr constant "
                      lenstr " c\"" dat "\\00\", align 8\n"))
            (string-append
             (comment-line
              "  %" (s-> x) " = call i64 @const_init_string(i8* getelementptr inbounds (" lenstr ", " lenstr "* @" (s-> dx) ", i32 0, i32 0))"
              "quoted string")
             (e->llvm e0))]
           [`(let ([,x ',(? symbol? dat)]) ,e0)
            (define dx (gensym 'sym))
            (define lenstr (string-append "[" (number->string (+ 1 (string-length (symbol->string dat)))) " x i8]"))
            (set! globals
                  (string-append globals
                      "@" (s-> dx) " = private unnamed_addr constant "
                      lenstr " c\"" (symbol->string dat) "\\00\", align 8\n"))
            (string-append
             (comment-line
              "  %" (s-> x) " = call i64 @const_init_symbol(i8* getelementptr inbounds (" lenstr ", " lenstr "* @" (s-> dx) ", i32 0, i32 0))"
              "quoted string")
             (e->llvm e0))]
           [`(let ([,x (make-closure ,lamx ,xs ...)]) ,e0)
            (define cptr (gensym 'cloptr))
            (define fptrptr (gensym 'eptr))
            (define eptrs (map (lambda (x) (gensym 'eptr)) xs))
            (define n (match (car (filter (match-lambda [`(proc (,f . ,_) . ,_) (eq? f lamx)]) procs))
                             [`(proc (,lamx ,ys ...) . ,_) (length ys)]))
            (define f (gensym 'f))
            (apply string-append
                   `(,(comment-line
                       "  %" (s-> cptr) " = call i64* @alloc(i64 " (number->string (* (+ (length xs) 1) 8)) ")"
                       "malloc")
                     ,@(map (lambda (iptr n)
                              (comment-line
                               "  %" (s-> iptr) " = getelementptr inbounds i64, i64* %" (s-> cptr) ", i64 " (number->string n)
                               (string-append "&" (s-> iptr) "[" (number->string n) "]")))
                            eptrs
                            (cdr (range (+ 1 (length xs)))))
                     ,@(map (lambda (x iptr)
                              (comment-line
                               "  store i64 %" (s-> x) ", i64* %" (s-> iptr)
                               (string-append "*" (s-> iptr) " = %" (s-> x))))
                            xs
                            eptrs)
                     ,(comment-line
                       "  %" (s-> fptrptr) " = getelementptr inbounds i64, i64* %" (s-> cptr) ", i64 0"
                       (string-append "&" (s-> cptr) "[0]"))
                     ,(comment-line 
                        "  %" (s-> f) " = ptrtoint void(i64"
                        (foldr string-append "" (map (lambda (_) ",i64")
                                                     (range (- n 1))))
                        ")* @" (s-> lamx) " to i64"
                        "fptr cast; i64(...)* -> i64")
                     ,(comment-line
                       "  store i64 %" (s-> f) ", i64* %" (s-> fptrptr)
                       "store fptr")
                     ,(comment-line
                      "  %" (s-> x) " = ptrtoint i64* %" (s-> cptr) " to i64"
                      "closure cast; i64* -> i64")
                     ,(e->llvm e0)))]
           [`(let ([,x (env-ref ,y ,n)]) ,e0)
            (define yptr (gensym 'envptr))
            (define iptr (gensym 'envptr))
            (apply string-append
                   `(,(comment-line
                       "  %" (s-> yptr) " = inttoptr i64 %" (s-> y) " to i64*"
                       "closure/env cast; i64 -> i64*")
                     ,(comment-line
                       "  %" (s-> iptr) " = getelementptr inbounds i64, i64* %" (s-> yptr) ", i64 " (number->string n)
                       (string-append "&" (s-> yptr) "[" (number->string n) "]"))
                     ,(comment-line
                       "  %" (s-> x) " = load i64, i64* %" (s-> iptr) ", align 8"
                       (string-append "load; *" (s-> iptr)))
                     ,(e->llvm e0)))]
           [`(let ([,x (prim ,op ,ys ...)]) ,e0)
            (string-append
             (comment-line
              "  %" (s-> x) " = call i64 @" (prim-name op) "("
              (if (null? ys)
                  ""
                  (string-append
                   "i64 %" (s-> (car ys))
                   (foldl (lambda (y acc) (string-append acc ", i64 %" (s-> y))) "" (cdr ys))))
              ")"
              (string-append "call " (prim-name op)))
             (e->llvm e0))]
           [`(let ([,x (apply-prim ,op ,y)]) ,e0)
            (string-append
             (comment-line
              "  %" (s-> x) " = call i64 @" (prim-applyname op) "(i64 %" (s-> y) ")"
              (string-append "call " (prim-applyname op)))
             (e->llvm e0))]
           [`(if ,x ,e0 ,e1)
            (define cmp (gensym 'cmp))
            (define tlab (gensym 'then))
            (define flab (gensym 'else))
            (string-append
             (comment-line "  %" (s-> cmp) " = icmp eq i64 %" (s-> x) ", 15"
                           "false?")
             (comment-line "  br i1 %" (s-> cmp) ", label %" (s-> flab) ", label %" (s-> tlab)
                           "if")
             "\n" (s-> tlab) ":\n"
             (e->llvm e0)
             "\n" (s-> flab) ":\n"
             (e->llvm e1))]
           [`(clo-app ,fx ,xs ...)
            (define cloptr (gensym 'cloptr))
            (define i0ptr (gensym 'i0ptr))
            (define fptr (gensym 'fptr))
            (define f (gensym 'f))
            (apply string-append
             `(,(comment-line
                 "  %" (s-> cloptr) " = inttoptr i64 %" (s-> fx) " to i64*"
                 "closure/env cast; i64 -> i64*")
               ,(comment-line
                 "  %" (s-> i0ptr) " = getelementptr inbounds i64, i64* %" (s-> cloptr) ", i64 0"
                 (string-append "&" (s-> cloptr) "[0]"))
               ,(comment-line
                 "  %" (s-> f) " = load i64, i64* %" (s-> i0ptr) ", align 8"
                 (string-append "load; *" (s-> i0ptr)))
               ,(comment-line
                 "  %" (s-> fptr) " = inttoptr i64 %" (s-> f) " to void (i64"
                 (foldl string-append "" (map (lambda (_) ",i64") (range (length xs))))
                 ")*"
                 "cast fptr; i64 -> void(...)*")
               ,(comment-line
                 "  musttail call fastcc void %" (s-> fptr)
                 "(i64 %" (s-> fx)
                 (foldl (lambda (x acc) (string-append acc ", i64 %" (s-> x))) "" xs)
                 ")"
                 "tail call")
               "  ret void\n"))]))

(define (proc->llvm proc)
    (match proc
           [`(proc (main) ,e)
            (string-append
             "define void @proc_main() {\n"
             (e->llvm e)
             "}\n"
             "\n\n"
             "define i32 @main() {\n"
             "  call fastcc void @proc_main()\n"
             "  ret i32 0\n"
             "}\n\n")]
           [`(proc (,lamx ,x0 ,xs ...) ,e0)
            (string-append
             "define void @" (symbol->string lamx)
             "("
             (foldl (lambda (x args) (string-append args ", i64 %" (s-> x)))
                    (string-append "i64 %" (s-> x0))
                    xs)
             ") {\n"
             ;;"call void @print_u64(i64 " (number->string (random 100000 999999)) ")\n"  ; useful for debugging
             (e->llvm e0)
             "}\n")]))
  
  (define llvm-procs
    (apply string-append
           (map (lambda (s) (string-append s "\n\n"))
                (map proc->llvm procs))))
  (string-append llvm-procs
                 "\n\n\n"
                 globals))





