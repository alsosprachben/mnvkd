#ifndef VK_SIGNAL_H
#define VK_SIGNAL_H

#include <signal.h>

struct vk_signal;

int vk_signal_init();
int vk_signal_setjmp();
void vk_signal_set_handler(void (*handler)(void *handler_udata, int jump, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr), void *handler_udata);
void vk_signal_set_jumper(void (*jumper)(void *jumper_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr), void *jumper_udata);
void vk_signal_set_mainline(void (*mainline)(void *mainline_udata), void *mainline_udata);

int vk_signal_restore(struct vk_signal *signal_ptr);
int vk_signal_save(struct vk_signal *signal_ptr);
void vk_signal_block(struct vk_signal *signal_ptr, int signal);
void vk_signal_unblock(struct vk_signal *signal_ptr, int signal);

int vk_signal_get_siginfo_str(siginfo_t *siginfo_ptr, char *str, size_t size);

#endif