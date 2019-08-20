#ifndef PGM_IO_H
#define PGM_IO_H

int read_pgm_image(char *infilename, unsigned char **image, int *rows, int *cols);
int write_pgm_image(char *outfilename, unsigned char *image, int rows, int cols, char *comment, int maxval);

#endif /* PGM_IO_H */

