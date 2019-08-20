#include "neon.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <arm_neon.h>
#include <string.h>

#define VERBOSE 0 

/*******************************************************************************
* PROCEDURE: make_gaussian_kernel
* PURPOSE: Create a one dimensional gaussian kernel.
* NAME: Mike Heath
* DATE: 2/15/96
*******************************************************************************/
void make_gaussian_kernel(float sigma, float **kernel, int *windowsize)
{
    int i, center;
    float x, fx, sum=0.0;

    *windowsize = 1 + 2 * ceil(2.5 * sigma);
    center = (*windowsize) / 2;

    if((*kernel = (float *) malloc((*windowsize)* sizeof(float))) == NULL)
    {
        fprintf(stderr, "Error callocing the gaussian kernel array.\n");
        exit(1);
    }

    for(i=0; i<(*windowsize); i++)
    {
        x = (float)(i - center);
        fx = pow(2.71828, -0.5*x*x/(sigma*sigma)) / (sigma * sqrt(6.2831853));
        (*kernel)[i] = fx;
        sum += fx;
    }

    for(i=0; i<(*windowsize); i++) (*kernel)[i] /= sum;

    if(VERBOSE)
    {
        printf("The filter coefficients are:\n");
        for(i=0; i<(*windowsize); i++)
            printf("kernel[%d] = %f\n", i, (*kernel)[i]);
    }
}

unsigned short int* gaussian_smooth_neon(unsigned char *image, int rows, int cols, float sigma, int complete_rows)
{
    int r, c, rr, cc,     /* Counter variables. */
        windowsize,        /* Dimension of the gaussian kernel. */
        center;            /* Half of the windowsize. */
    float *tempim,*tempim1,        /* Buffer for separable filter gaussian smoothing. */
          *kernel,        /* A one dimensional gaussian kernel. */
          dot,            /* Dot product summing variable. */
          sum;            /* Sum of the kernel weights variable. */

    /****************************************************************************
    * Create a 1-dimensional gaussian smoothing kernel.
    ****************************************************************************/
    if(VERBOSE) printf("   Computing the gaussian smoothing kernel.\n");
    make_gaussian_kernel(sigma, &kernel, &windowsize);
    center = windowsize / 2;


    /****************************************************************************
    * Allocate a temporary buffer image and the smoothed image.
    ****************************************************************************/
    if((tempim = (float *) malloc(rows*cols* sizeof(float))) == NULL)
    {
        fprintf(stderr, "Error allocating the buffer image.\n");
        exit(1);
    }
    unsigned short int* smoothedim;
    unsigned short int* smoothedim1;// just for the pupose of testing 
    if(((smoothedim) = (unsigned short int *) malloc(complete_rows*cols*sizeof(short int))) == NULL)
    {
        fprintf(stderr, "Error allocating the smoothed image.\n");
        exit(1);
    }
 
    /****************************************************************************
    * Blur in the x - direction.
    ****************************************************************************/
	int loop; 
	
	int floop;
	float * new_image;
	float *new_image_col;
	float new_kernel[17];

	for (floop = 0 ; floop < 17 ; floop++)
	{
		if(floop == 0 || floop == 16 )
			new_kernel[floop] = 0 ;
		else
			new_kernel [floop] = kernel[floop -1];	
	}
	unsigned int new_cols;
	new_cols=cols+16;
	unsigned int i, k; 
	unsigned int a; 
	unsigned int m; 
	unsigned int n, j;
	new_image = (float*)malloc(new_cols*rows*sizeof(float));
	for( i =0; i<rows; i++)
		{
		 memset(&new_image[i*new_cols],0,8*sizeof(float));
  		 for( k=0; k<cols;k++)
			{
			 new_image[i*new_cols+8+k] = (float)image[i*cols+k];
			}
		 memset(&new_image[i*new_cols+8+cols],0,8*sizeof(float));
	    }

  	float32x4_t neon_input;
	float32x4_t neon_filter;
	float32x4_t temp_sum;
	float32x2_t tempUpper;
	float32x2_t tempLower; 
	float32_t zero = 0;
	float32_t temp_output;
	float Basekernel = 0.0f;
	float kernelSum = 0.0f;


	//printf("\n the ouput image\n");
	for( a=8; a<=16; a++){
		Basekernel += new_kernel[a];
	}


	for(m=0; m<rows; m++){
		for( n=0; n<cols; n++){
			temp_sum = vdupq_n_f32(0);
			if(n==0){
				kernelSum = Basekernel;
			}
			else if(n <=8){
				kernelSum += new_kernel[8-n];
			}
			else if(n>=cols-8){
				kernelSum -=new_kernel[cols-n+8];
			}


			for( j=0; j<4; j++)
			{
				int kk=0;
				if(j>=2)
				{
					kk=1;
				}
				neon_input = vld1q_f32((float32_t const *)&new_image[m*new_cols+n+j*4+kk]);
				neon_filter = vld1q_f32((float32_t const *)&new_kernel[j*4+kk]);
				temp_sum = vmlaq_f32(temp_sum,neon_input,neon_filter);
			}
			
			unsigned int t;
	
			for( t=0; t<=3; t++){	
						
				temp_output += vgetq_lane_f32(temp_sum,t ); 
			}
			temp_output += new_image[m*new_cols+n+8] * new_kernel[8];
			temp_output /= kernelSum;
			tempim[m*cols+n] = temp_output;
			temp_output=0; 
		}
	}

	
    // /****************************************************************************
    // * Blur in the x - direction.
    // ****************************************************************************/

    /****************************************************************************
    * Blur in the y - direction.
    ****************************************************************************/

    unsigned int new_rows;
	new_rows=rows+16;
	new_image_col = (float*)malloc(new_rows*cols*sizeof(float));
	if(VERBOSE) printf("   Bluring the image in the Y-direction.\n");

	for( i =0; i<cols; i++){//actually nember of new rows are the number of columns here 
		memset(&new_image_col[i*new_rows],0,8*sizeof(float));

		for( k=0; k<rows;k++){
			new_image_col[i*new_rows+8+k] = tempim[k*cols+i];
			//new_image_col[i*new_rows+8+k] = imagetest1[k*cols+i];
		}
		memset(&new_image_col[i*new_rows+8+rows],0,8*sizeof(float));
	}

	Basekernel = 0.0; 
	for( a=8; a<=16; a++){
		Basekernel += new_kernel[a];
	}

	for(m=0; m<cols; m++){
		for( n=0; n<rows; n++){
			temp_sum = vdupq_n_f32(0);
			if(n==0){
				kernelSum = Basekernel;
			}
			else if(n <=8){
				kernelSum += new_kernel[8-n];
			}
			else if(n>=rows-8){
				kernelSum -=new_kernel[rows-n+8];
			}

			for( j=0; j<4; j++)
			{
				int kk=0;
				if(j>=2)
				{
					kk=1;
				}
				neon_input = vld1q_f32((float32_t const *)&new_image_col[m*new_rows+n+j*4+kk]);
			 	neon_filter = vld1q_f32((float32_t const *)&new_kernel[j*4+kk]);
				temp_sum = vmlaq_f32(temp_sum,neon_input,neon_filter);
			}
			
			unsigned int t;
		
			for( t=0; t<=3; t++){	
						
				temp_output += vgetq_lane_f32(temp_sum,t ); 

			}
			temp_output += new_image_col[m*new_rows+n+8] * new_kernel[8];
			temp_output = (temp_output * 90) / kernelSum + 0.5;
			
			smoothedim[n*cols+m] = (unsigned short int )temp_output;
			temp_output=0; 
		}
	}

	free(new_image);
	free(new_image_col);
    free(tempim);
    free(kernel);
    return smoothedim;
}
