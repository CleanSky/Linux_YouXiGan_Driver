#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 10

int main(){
	int num[SIZE];
	int i = 0, sum = 0;

	srand(time(0));

	for(i = 0; i < SIZE; i++){
		num[i] = rand() % 100;
		sum += num[i];
	}

	printf("Sum: %d\n", sum);
}