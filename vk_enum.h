#ifndef VK_ENUM_H
#define VK_ENUM_H

#include <string.h>

#define ENUM_BIT(bit) (1U << (bit))

#define DEFINE_ENUM_INTERFACE(type_name, map_name, map_elem, map_repr, mapping_name) \
        static const int map_name##_SIZE; \
	const char* type_name##_to_string(enum type_name map_elem); \
	enum type_name string_to_##type_name(const char* map_repr); \
	int validate_##type_name##_map();\
                                                                                    \
                                                                                    \
// Unified macro to define enum, map, and all associated functions including validation
#define DEFINE_ENUM_FUNCTIONS(type_name, map_name, map_elem, map_repr, mapping_name) \
	static const int map_name##_SIZE = sizeof(map_name) / sizeof(struct mapping_name); \
                                                                   \
	const char* type_name##_to_string(enum type_name map_elem) {                            \
		if (map_elem >= 0 && map_elem < map_name##_SIZE) {                       \
			return map_name[map_elem].map_repr;                                      \
		} \
                return "UNKNOWN"; \
	}                                                                        \
                                                                          \
	enum type_name string_to_##type_name(const char* map_repr) { \
		int low = 0, high = map_name##_SIZE - 1, mid; \
		while (low <= high) { \
			mid = low + (high - low) / 2; \
			int res = strcasecmp(map_repr, map_name[mid].map_repr); \
			if (res == 0) { \
				return map_name[mid].map_elem; \
			} else if (res < 0) { \
				high = mid - 1; \
			} else { \
				low = mid + 1; \
			} \
		} \
		return (enum type_name)(map_name##_SIZE); \
	}                                                                        \
                                                                          \
	int validate_##type_name##_map() { \
		for (int i = 1; i < map_name##_SIZE; ++i) { \
			if (strcmp(map_name[i - 1].map_repr, map_name[i].map_repr) > 0 || map_name[i - 1].map_elem >= map_name[i].map_elem) { \
				errno = EINVAL; \
				return -1; \
			} \
		}                                                                       \
		return 0;\
	}

#endif