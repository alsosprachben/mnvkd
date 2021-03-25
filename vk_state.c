#include "vk_state.h"
#include "vk_stack.h"

#include "vk_space.h"


int vk_continue(struct that *that) {
	while (that->status == VK_PROC_RUN) {
		that->func(that);
	}
	return 0;
}

// vk_proc(proc_a) {
void proc_a(struct that *that) {
	int rc;
	char *s1;
	char *s2;

	vk_begin();
	
	rc = vk_stack_begin(that->stack, that);
	if (rc == -1) {
		vk_error();
	}

	rc = vk_calloc(s1, 1, 256);
	if (rc == -1) {
		vk_error();
	}

	vk_procdump(that, "in1");

	rc = snprintf(s1, 256, "test 1\n");
	if (rc == -1) {
		vk_error();
	}

	vk_msg((intptr_t) s1);

	rc = vk_calloc(s2, 1, 256);
	if (rc == -1) {
		vk_error();
	}


	vk_procdump(that, "in2");

	rc = snprintf(s2, 256, "test 2\n");
	if (rc == -1) {
		vk_error();
	}
	vk_msg((intptr_t) s2);


	rc = vk_stack_end(that->stack);
	if (rc == -1) {
		vk_error();
	}

	vk_end();
}

int main() {
	struct vk_space_1k proc;

	VK_SPACE_INIT(&proc, proc_a);

	vk_procdump(&proc.that, "out0");
	printf("proc.that.msg = %s\n", (char *) proc.that.msg);
	proc.that.status = VK_PROC_RUN;

	vk_continue(&proc.that);
	vk_procdump(&proc.that, "out1");
	printf("proc.that.msg = %s\n", (char *) proc.that.msg);
	proc.that.status = VK_PROC_RUN;

	vk_continue(&proc.that);
	vk_procdump(&proc.that, "out2");
	printf("proc.that.msg = %s\n", (char *) proc.that.msg);
	proc.that.status = VK_PROC_RUN;

	vk_continue(&proc.that);
	vk_procdump(&proc.that, "out3");
	printf("proc.that.msg = %s\n", (char *) proc.that.msg);
	proc.that.status = VK_PROC_RUN;
}
