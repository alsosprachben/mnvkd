#include "vk_main.h"
#include "vk_thread_io.h"
#include "vk_thread.h"

void aio_copy(struct vk_thread* that)
{
    ssize_t rc = 0;
    struct {
        char buf[4096];
    } *self;

    vk_begin();

    for (;;) {
        vk_read(rc, self->buf, sizeof(self->buf));
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            vk_raise(EPIPE);
        }

        vk_write(self->buf, (size_t)rc);
    }

    vk_end();
}

int main(void)
{
    return vk_main_init(aio_copy, NULL, 0, 32, 1, 0);
}
