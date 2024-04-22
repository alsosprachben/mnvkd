#include "vk_main.h"
#include "vk_http11.h"

int main(int argc, char* argv[])
{
	return vk_main_init(http11_request, NULL, 0, 26, 0, 1);
}