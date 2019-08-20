#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "pgm_io.h"


int main (int argc, char **argv) {
	unsigned char *original, *image1, *image2, *diff;
	int rows, rows1, rows2,
		cols, cols1, cols2,
		x, y;
	float mse1 = 0, mse2 = 0;
	float temp1, temp2;

	if (argc < 5) {
		printf("Usage: %s <original> <image1> <image2> <output>\n", argv[0]);
		printf("\t<original> is the filename of the original image\n");
		printf("\t<image1> is the filename of the \"correct\" image\n");
		printf("\t<image2> is the filename of the second image\n");
		printf("\t<output> is the filename where a difference map will be written to\n");
		return 1;
	}

	if (read_pgm_image(argv[1], &original, &rows, &cols) == 0) {
		printf("Unable to open image %s\n", argv[1]);
		return 2;
	}
	if (read_pgm_image(argv[2], &image1, &rows1, &cols1) == 0) {
		printf("Unable to open image %s\n", argv[2]);
		return 2;
	}
	if (read_pgm_image(argv[3], &image2, &rows2, &cols2) == 0) {
		printf("Unable to open image %s\n", argv[3]);
		return 3;
	}

	if (rows != rows1 || cols != cols1 || rows1 != rows2 || cols1 != cols2) {
		printf("Images are of different sizes!\n");
		return 4;
	}


	diff = malloc(rows * cols * sizeof(unsigned char));

	for (y = 0; y < rows; ++y) {
		for (x = 0; x < cols; ++x) {
			diff[y * rows + x] = 255 - (unsigned char)abs(image1[y * rows + x] - image2[y * rows + x]);

			temp1 = (original[y * rows + x] - image1[y * rows + x]);
			temp2 = (original[y * rows + x] - image2[y * rows + x]);
			mse1 += (temp1 * temp1);
			mse2 += (temp2 * temp2);
		}
	}
	mse1 /= (rows * cols);
	mse2 /= (rows * cols);

	if (write_pgm_image(argv[4], diff, rows, cols, "", 255) == 0) {
		printf("Unable to write image %s\n", argv[4]);
		return 5;
	}

	printf("mean squared error 1: %f\n", mse1);
	printf("mean squared error 2: %f\n", mse2);
	printf("mean squared error difference: %f\n", fabs(mse2 - mse1));
	printf("error rate: %f\n", fabs(mse2 - mse1) / mse1);

	free(original);
	free(image1);
	free(image2);
	free(diff);
	return 0;
}
