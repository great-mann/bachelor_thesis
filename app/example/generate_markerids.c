#include <stdio.h>

int main (int argc, char *argv){
	int id = 202;
	if (0<=id && id<1024) {
	        //for each line, create
	        int ids[4]={0x10,0x17,0x09,0x0e};
	        for (int y=0;y<5;y++) {
	            int index=(id>>(2*(4-y))) & 0x0003;
		    printf("\nindex=%d   ", index);
	            int val=ids[index];
	            for (int x=0;x<5;x++) {
	                if ( ( val>>(4-x) ) & 0x0001 ) printf("1");
	                else printf("0");
	            }
	        }
	}
	printf("\n\n");
	
}
