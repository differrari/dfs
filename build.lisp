(load "~/redbuild/v3/redbuild.lisp")

(redbuild:quick-build (redbuild:make-instance `redbuild:redmod
        :name "dfs"
        :type :bin
        :target :linux
        :libs (list (make-instance `redbuild:lib-class
            :header "/usr/include/fuse3"
            :source ""
        ) (redbuild:system-lib "fuse3"))
        :srcs (list "main.c" "module_loader.c")
        :flags (list "-DCROSS")
) :add-dependencies t :run nil :success (lambda () (print (redbuild:emit-compile-commands))))