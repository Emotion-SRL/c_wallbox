(defun onion-build (target)
  (interactive "sMake target (empty = default): ")
  (copy-directory
   "/home/carlo_m/programming/emotion/wallbox/c_ws/src"
   "/docker:builder@onion-builder:/home/builder/src"
   t  ;; keep-time
   t  ;; parents
   t) ;; copy-contents
  (copy-file
   "/home/carlo_m/programming/emotion/wallbox/c_ws/Makefile"
   "/docker:builder@onion-builder:/home/builder/Makefile" t)
  (let ((cmd (if (string-empty-p target)
                 "make"
               (format "make %s" target))))
    (if (get-buffer "container")
        (with-current-buffer "container"
          (compile cmd))
      (message "no buffer named container"))))

(defun build_x86 ()
  (interactive)
  (if (set-buffer "c_ws")
      (compile "make build_x86")
    (message "error on setting buffer")))

(defun build_x86 ()
  (interactive)
  (if (set-buffer "c_ws")
      (compile "make build_x86")
    (message "error on setting buffer")))

