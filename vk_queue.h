#ifndef VK_QUEUE_H
#define VK_QUEUE_H

#include <sys/queue.h>

#if !defined(SLIST_FOREACH_SAFE) && defined(SLIST_FOREACH_MUTABLE)
#define SLIST_FOREACH_SAFE SLIST_FOREACH_MUTABLE
#else
/* from Darwin */
#define SLIST_FOREACH_SAFE(var, head, field, tvar)                   \
	for ((var) = SLIST_FIRST((head));                               \
	    (var) && ((tvar) = SLIST_NEXT((var), field), 1);            \
	    (var) = (tvar))
#endif

#endif