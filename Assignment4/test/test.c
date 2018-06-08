#include <stdio.h>
#include <unistd.h>

#define usedMem syscall(354)
#define freeMem syscall(353)

int main() {
	float fragmentation;

	printf("Running tests... \n");

	int i;
	for(i=0; i<4; i++) {
		fragmentation = (float)freeMem / (float)usedMem;
		printf("Used Memory: %lu \n", usedMem);
		printf("Free Memory: %lu \n", freeMem);
		printf("Fragmentation: %f \n", fragmentation);
		printf("\n");
		printf("\n");
		sleep(1);
	}
		
}
