#include <stdio.h>
int odr_a(void);
int odr_b(void);
int main(void)
{
	printf("odr-link ok: %d %d\n", odr_a(), odr_b());
	return 0;
}
