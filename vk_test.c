#include "vk_state.h"
#include "debug.h"

void proc_a(struct that *that) {
	int rc;

	struct {
		int rc;
		size_t i;
		char s1[256];
		char s2[256];
		struct {
			char s3[256];
		} *other;
	} *self;

	vk_begin();

	for (self->i = 0; self->i < 1000000; self->i++) {
		vk_calloc(self->other, 5);

		vk_readline(rc, self->other[2].s3, sizeof (self->other[2].s3) - 1);
		if (rc == 0 || vk_eof()) {
			vk_free();
			break;
		}

		self->other[2].s3[rc] = '\0';

		rc = snprintf(self->other[3].s3, sizeof (self->other[3].s3) - 1, "Line %zu: %s", self->i, self->other[2].s3);
		if (rc == -1) {
			vk_error();
		}

		vk_write(self->other[3].s3, strlen(self->other[3].s3));
		vk_flush();

		vk_free();
	}

	vk_end();
}

#include "vk_proc.h"
#include "vk_proc_s.h"
#include "vk_state_s.h"

int main2() {
	int rc;
	struct vk_proc proc;
	struct that vk;

	memset(&proc, 0, sizeof (proc));
	rc = VK_PROC_INIT_PRIVATE(&proc, 4096 * 2);
	if (rc == -1) {
		return 1;
	}

	memset(&vk, 0, sizeof (vk));
	VK_INIT(&vk, &proc, proc_a, 0, 1);

	do {
		vk_proc_execute(&proc, &vk);
	} while ( ! vk_is_completed(&vk));;

	rc = vk_deinit(&vk);
	if (rc == -1) {
		return 2;
	}

	rc = vk_proc_deinit(&proc);
	if (rc == -1) {
		return 3;
	}

	return 0;
}


