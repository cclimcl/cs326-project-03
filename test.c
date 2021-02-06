/**
 * @file test.c
 *
 * A test driver.
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logger.h"
#include "allocator.h"


/**
 * A test driver.
 *
 * @param void
 */
int main(void){

	// //test 3
	// char* str0 = malloc(100);
	// char* str1 = malloc(100);
	// char* str2 = malloc(16);
	// char* str3 = malloc(100);
	// char* str4 = malloc(100);
	// free(str1);
	// free(str3);
	// char* str5 = malloc(16);
	// char* str6 = malloc(4096);

	// print_memory();

	//char* str1 =
	// char* str2 = malloc(10);
	// char* str3 = malloc(5000);
	// print_memory();

	// char* str0 = malloc(8);
	// free(str0);
	// char* str1 = malloc(120);
	// char* str2 = malloc(16);
	// char* str3 = malloc(776);
	// char* str4 = malloc(112);
	// char* str5 = malloc(1336);
	// char* str6 = malloc(216);
	// char* str7 = malloc(432);
	// char* mid = malloc(8);
	// char* str8 = malloc(1);

	// char *str1 = malloc(100);
	// char *str2 = calloc(17, 16);


	//print memory is allocating memory
	print_memory();

}