#include "vk_main.h"
#include "vk_echo.h"

int main(int argc, char* argv[])
{
	return vk_main_init(vk_echo, NULL, 0, 25, 0, 1);
}