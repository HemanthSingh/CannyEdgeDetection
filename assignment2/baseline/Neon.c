float * new_image;
unsigned int new_cols;
new_cols=cols+16;
// if(new_cols%8!=0){
// 	new_cols = (new_cols/8+1)*8;
// }
new_image = (float*)malloc(new_cols*rows*sizeof(float));
for(int i =0; i<rows; i++){
	memset(new_image[i*new_cols],0,8);
	//memcpy(new_image[i*new_cols+8],image[i*cols],cols*sizeof(float));
	for(int k=0; k<cols;k++){
		new_image[i*new_cols+8+k] = (float)image[i*cols+k];
	}
	//memset(new_image[i*new_cols+8+cols],0,(new_cols-cols-8));
	memset(new_image[i*new_cols+8+cols],0,8);
}

float32x4_t neon_input;
float32x4_t neon_filter;
float32x4_t temp_sum;
float32_t zero = 0;
float32_t temp_output;
float Basekernel = 0.0f;
float kernelSum;
for(int a=8; a<=16; a++){
	Basekernel += kernel[a];
}

for（int m=0; m<rows; m++）{
	for(int n=0; n<cols; n++){
		temp_sum = vdupq_n_f32(0);
		if(n==0){
			kernelSum = Basekernel;
		}
		else if(n <=8){
			kernelSum +=kernel[8-n];
		}
		else if(n>=cols-8){
			kernelSum -=kernel[cols-n+8];
		}

		for(int j=0; j<3; j++)
		{
			int k=0;
			if(j>=2)
			{
				k=1;
			}
			neon_input = vld1q_f32(&new_image[m*new_cols+n+j*4+k]);
			neon_filter = vld1q_f32(&kernel[j*4+k]);
			temp_sum = vmlaq_f32(temp_sum,neon_input,neon_filter);
		}	
		for(int i=0; i<3; i++){			
			temp_output += vgetq_lane_f32(temp_sum, i);
		}
		temp_output += new_image[m*new_cols+n+8] * kernel[8];
		temp_output /= kernelSum;
	}
}





