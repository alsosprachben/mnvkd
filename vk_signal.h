#ifndef VK_SIGNAL_H
#define VK_SIGNAL_H

void vk_signal_init();
void vk_signal_setjmp();
void vk_signal_set_handler(void (*handler)(void *handler_udata, int jump, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr), void *handler_udata);
void vk_signal_set_jumper(void (*jumper)(void *jumper_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr), void *jumper_udata);

#endif