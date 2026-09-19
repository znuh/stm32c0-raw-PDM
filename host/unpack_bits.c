#include <unistd.h>
#include <stdint.h>
#include <assert.h>

int main(int argc, char **argv) {
	uint8_t out[4096];
	uint8_t in[sizeof(out)/8];
	int res;
	while((res=read(0,in,sizeof(in)))>0) {
		for(int i=0;i<(res*8);i++) {
			int bit = 7-(i&7);
			out[i] = 255 * ((in[i>>3]>>bit)&1);
		}
		int r2 = write(1,out,res*8);
		assert(r2 == res*8);
	}
	return 0;
}
