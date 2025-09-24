#include "vk_main.h"
#include "vk_thread.h"
#include <signal.h>

void erring2(struct vk_thread *that)
{
	volatile ssize_t rc = 0; /* volatile to avoid optimization */
	struct {
		int erred;
	}* self;
	vk_begin();

	vk_log("LOG Before signal is raised.");

	if ( ! self->erred) {
		vk_log("LOG Generating signal.");
		rc = raise(SIGFPE); /* raise SIGFPE in a way optimizers cannot fold out */
	} else {
		vk_log("LOG Signal is not generated.");
	}

	vk_log("LOG After signal is raised.");

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG Caught signal");
		vk_sigerror();
		self->erred = 1;
		vk_lower();
	} else {
		vk_log("LOG Signal is cleaned up.");
	}

	vk_end();
}

int main() {
	return vk_main_init(erring2, NULL, 0, 26, 1, 0);
}
