#include "redbuild.h"
int main(){
	int __return_val;
	rebuild_self();
	new_module("mail");
	set_name("mail");
	set_target(target_native);
	set_package_type(package_bin);
	debug();
	add_local_dependency("/usr/include/fuse3", "`pkg-config fuse3 --cflags --libs`", 0, 0);
	source("mail.c");
	source("main.c");
	if (compile()){
		gen_compile_commands(0);
		run();
	}
	
	__return_val = 1;
	goto defer;
	defer:
	emit_compile_commands();
	return __return_val;
}
