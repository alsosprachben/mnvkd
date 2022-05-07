#ifndef VK_POLL_H
#define VK_POLL_H

struct that;
struct io_future;

void io_future_init(struct io_future *ioft, struct that *blocked_vk);

#endif