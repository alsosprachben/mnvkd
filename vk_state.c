#include "vk_state.h"

#include "vk_heap.h"

int vk_init(struct that *that, void (*func)(struct that *that), char *file, size_t line, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset) {
	that->func = func;
	that->file = file;
	that->line = line;
	that->counter = -1;
	that->status = 0;
	that->error = 0;
	that->msg = 0;
	return vk_heap_map(&that->hd, map_addr, map_len, map_prot, map_flags, map_fd, map_offset);
}
int vk_deinit(struct that *that) {
	return vk_heap_unmap(&that->hd);
}
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

	struct {
		int rc;
		size_t i;
		char s1[256];
		char s2[256];
		struct {
			char s3[256];
		} *other;
	} *self;

	self = that->hd.addr_start;

	vk_begin();

	self = vk_heap_push(&that->hd, sizeof (*self), 1);
	if (self == NULL) {
		vk_error();
	}

	
	vk_procdump(that, "in1");

	rc = snprintf(self->s1, 256, "test 1\n");
	if (rc == -1) {
		vk_error();
	}

	vk_msg((intptr_t) self->s1);

	vk_procdump(that, "in2");

	rc = snprintf(self->s2, 256, "test 2\n");
	if (rc == -1) {
		vk_error();
	}
	vk_msg((intptr_t) self->s2);

	for (self->i = 0; self->i < 5; self->i++) {
		self->other = vk_heap_push(&that->hd, sizeof (*self->other), 5);
		if (self->other == NULL) {
			vk_error();
		}

		rc = snprintf(self->other[3].s3, 256, "test 3: %zu\n", self->i);

		if (rc == -1) {
			vk_error();
		}
		vk_msg((intptr_t) self->other[3].s3);

		rc = vk_heap_pop(&that->hd);
		if (rc == -1) {
			vk_error();
		}
	}

	rc = vk_heap_pop(&that->hd);
	if (rc == -1) {
		vk_error();
	}

	vk_end();
}

int main() {
	int rc;
	size_t i;
	struct that that;

	rc = VK_INIT(&that, proc_a, 4096 * 32);
	if (rc == -1) {
		return 1;
	}

	do {
		vk_procdump(&that, "out");
		printf("that.msg = %s\n", (char *) that.msg);
		that.status = VK_PROC_RUN;
		vk_continue(&that);
	} while (that.status != VK_PROC_END);

	return 0;
}
