#include <stdio.h>

int add_together(int x, int y) {
    int result = x + y;
    return result;
}

typedef struct {
    float x;
    float y;
} point;

int main(int argc, char** argv) {
	int added = add_together(10, 20);
	point p;
	p.x = 0.1;
	p.y = 10.0;
	
	float legnth = p.x * p.x + p.y * p.y; 
	
	
	return 0;
}
