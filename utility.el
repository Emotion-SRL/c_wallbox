; look for Omega-3D16 on nmap

(defun onion-build (target)
  (interactive "sMake target (empty = default): ")
  (copy-directory
   "/home/carlo_m/programming/emotion/wallbox/c_ws/src"
   "/docker:builder@onion-builder-wss:/home/builder/src"
   t  ;; keep-time
   t  ;; parents
   t) ;; copy-contents
  (copy-file
   "/home/carlo_m/programming/emotion/wallbox/c_ws/Makefile"
   "/docker:builder@onion-builder-wss:/home/builder/Makefile" t)
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

(defun build_x86_and_exec ()
  (interactive)
  (if (set-buffer "c_ws")
      (compile "make build_x86 && ./build_x86.out")
    (message "error on setting buffer")))

(defun dev-onion-init (ip)
  "Start the build container and connect to the Onion device at IP."
  (interactive "sOnion IP address: ")
  (start-process "enter-container" "*enter-container*" "bash"
                 "/home/carlo_m/programming/emotion/wallbox/c_ws/enter_container.sh")
  (onion--wait-and-connect ip))

(defun onion--wait-and-connect (ip)
  "Wait for onion-builder-wss container, then open TRAMP dired buffers."
  (if (= 0 (call-process "docker" nil nil nil
                          "inspect" "-f" "{{.State.Running}}" "onion-builder-wss"))
      (progn
        (with-current-buffer
            (dired "/docker:builder@onion-builder-wss:/home/builder/")
          (rename-buffer "container" t))
        (with-current-buffer
            (dired (format "/ssh:root@%s:/root/" ip))
          (rename-buffer "onion" t))
        (message "Connected to container and onion at %s" ip))
    (run-with-timer 1 nil #'onion--wait-and-connect ip)))

