#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <omp.h>
#include <sys/time.h>

#define PNG_DEBUG 3
#include "png.h"


void abort_(const char * s, ...)
{
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
}

struct Image {
    int width, height;
    png_byte color_type;
    png_byte bit_depth;
    png_structp png_ptr;
    png_infop info_ptr;
    int number_of_passes;
    png_bytep * row_pointers;
};


struct Image read_png_file(char* file_name)
{
    char header[8];    // 8 is the maximum size that can be checked
    struct Image image;
    
    /* open file and test for it being a png */
    FILE *fp = fopen(file_name, "rb");
    if (!fp)
        abort_("[read_png_file] File %s could not be opened for reading", file_name);
        fread(header, 1, 8, fp);
        if (png_sig_cmp(header, 0, 8))
            abort_("[read_png_file] File %s is not recognized as a PNG file", file_name);
            
            
        /* initialize stuff */
            image.png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
            
            if (!image.png_ptr)
                abort_("[read_png_file] png_create_read_struct failed");
                
                image.info_ptr = png_create_info_struct(image.png_ptr);
                if (!image.info_ptr)
                    abort_("[read_png_file] png_create_info_struct failed");
                    
                    if (setjmp(png_jmpbuf(image.png_ptr)))
                        abort_("[read_png_file] Error during init_io");
                        
                        png_init_io(image.png_ptr, fp);
                        png_set_sig_bytes(image.png_ptr, 8);
                        
                        png_read_info(image.png_ptr, image.info_ptr);
                        
                        image.width = png_get_image_width(image.png_ptr, image.info_ptr);
                        image.height = png_get_image_height(image.png_ptr, image.info_ptr);
                        image.color_type = png_get_color_type(image.png_ptr, image.info_ptr);
                        image.bit_depth = png_get_bit_depth(image.png_ptr, image.info_ptr);
                        
                        image.number_of_passes = png_set_interlace_handling(image.png_ptr);
                        png_read_update_info(image.png_ptr, image.info_ptr);
                        
                        
                    /* read file */
                        if (setjmp(png_jmpbuf(image.png_ptr)))
                            abort_("[read_png_file] Error during read_image");
                            
                            image.row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * image.height);
                            
                            for (int y=0; y<image.height; y++)
                                image.row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(image.png_ptr,image.info_ptr));
                                
                                png_read_image(image.png_ptr, image.row_pointers);
                                
                                fclose(fp);
                                
                                return image;
}

/* float *createEmpty(int width, int height) */
/* { */
/* 	float *buffer = (float *) malloc(width * height * sizeof(float)); */
/* 	if (buffer == NULL) { */
/* 		fprintf(stderr, "Could not create image buffer\n"); */
/* 		return NULL; */
/* 	} */
/* 	// Create Mandelbrot set image */
/* 	int xPos, yPos; */
/* 	for (yPos=0 ; yPos<height ; yPos++) */
/* 	{ */
/* 		for (xPos=0 ; xPos<width ; xPos++) */
/* 		{ */
/*       buffer[yPos * width + xPos] = 0; */
/* 		} */
/* 	} */
/* 	return buffer; */
/* } */

void write_png_file(char* file_name, struct Image image)
{
    /* create file */
    FILE *fp = fopen(file_name, "wb");
    if (!fp)
        abort_("[write_png_file] File %s could not be opened for writing", file_name);
    
    
    /* initialize stuff */
    image.png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    if (!image.png_ptr)
        abort_("[write_png_file] png_create_write_struct failed");
    
    image.info_ptr = png_create_info_struct(image.png_ptr);
    if (!image.info_ptr)
        abort_("[write_png_file] png_create_info_struct failed");
    
    if (setjmp(png_jmpbuf(image.png_ptr)))
        abort_("[write_png_file] Error during init_io");
    
    png_init_io(image.png_ptr, fp);
    
    
    /* write header */
    if (setjmp(png_jmpbuf(image.png_ptr)))
        abort_("[write_png_file] Error during writing header");
    
    png_set_IHDR(image.png_ptr, image.info_ptr, image.width, image.height,
                 image.bit_depth, image.color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    
    png_write_info(image.png_ptr, image.info_ptr);
    
    
    /* write bytes */
    if (setjmp(png_jmpbuf(image.png_ptr)))
        abort_("[write_png_file] Error during writing bytes");
    
    png_write_image(image.png_ptr, image.row_pointers);
    
    
    /* end write */
    if (setjmp(png_jmpbuf(image.png_ptr)))
        abort_("[write_png_file] Error during end of write");
    
    png_write_end(image.png_ptr, NULL);
    
    /* cleanup heap allocation */
    for (int y=0; y<image.height; y++)
        free(image.row_pointers[y]);
    free(image.row_pointers);
    
    fclose(fp);
}


int process_file(struct Image image, struct Image image2, int startRowIndex, int endRowIndex, int startColumnIndex, int endColumnIndex)
{
    double diff = 0;
    double temp = 0;
    int n = 100;
    struct timeval start, end;
    
    
    
    //  if (png_get_color_type(image.png_ptr, image.info_ptr) == PNG_COLOR_TYPE_RGB)
    //   abort_("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA "
    //          "(lacks the alpha channel)");
    
    // if (png_get_color_type(image.png_ptr, image.info_ptr) != PNG_COLOR_TYPE_RGBA)
    //        abort_("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
    //               PNG_COLOR_TYPE_RGBA, png_get_color_type(image.png_ptr, image.info_ptr));
    
    gettimeofday(&start, 0);
    
    
    /* parallelization with OMP */
#pragma omp parallel for collapse(3)
    
    //  printf("Hello from thread %d, nthreads %d\n", omp_get_thread_num(), omp_get_num_threads());
    
    for (int i = 0; i < n; i++) {
        
        for (int y=startRowIndex; y<endRowIndex; y++) {
            // png_byte* row  = image.row_pointers[y];
            // png_byte* row2 = image2.row_pointers[y];
            
            for (int x=startColumnIndex; x<endColumnIndex; x++) {
                png_byte* row  = image.row_pointers[y];
                png_byte* row2 = image2.row_pointers[y];
                
                png_byte* ptr  = &(row[x*4]);
                png_byte* ptr2 = &(row2[x*4]);
                if(ptr[0] != ptr2[0] || ptr[1] != ptr2[1] ||ptr[2] != ptr2[2] ||ptr[3] != ptr2[3]) {
                    /* printf("Pixel at position [ %d - %d ] has RGBA values: %d - %d - %d - %d\n", */
                    /*        x, y, ptr[0], ptr[1], ptr[2], ptr[3]); */
                    /* printf("Pixel2 at position [ %d - %d ] has RGBA values: %d - %d - %d - %d\n", */
                    /*        x, y, ptr2[0], ptr2[1], ptr2[2], ptr2[3]); */
                    /* diff the first image */
                    ptr[0] = 255;
                    ptr[1] = 30;
                    ptr[2] = 232;
                    ptr[3] = 255;
                    diff++;
                }
            }
        }
        
    }
    gettimeofday(&end, 0);
    
    //  printf("begin:                         %d sec %d usec\n", start.tv_sec, start.tv_usec);
    //   printf("end:                           %d sec %d usec\n", end.tv_sec, end.tv_usec);
    long seconds = end.tv_sec - start.tv_sec;
    long useconds = end.tv_usec - start.tv_usec;
    
    if(useconds < 0) {
        useconds += 1000000;
        seconds--;
    }
    
    printf("\nPeriod of time:      %d sec %.2f millisec\n", seconds, (useconds * 0.001));
    return diff/n;
}


int main(int argc, char **argv)
{
    
    /* TODO implement two different dimension images */
    if (argc != 4)
        abort_("Usage: program_name <file_in_to_compare> <file_in_to_compare> <diff_file_out>");
    
    struct Image image  = read_png_file(argv[1]);
    struct Image image2 = read_png_file(argv[2]);
    
    int diff = process_file(image, image2, 0, image.height, 0, image.width);
    write_png_file(argv[3], image);
    
    double count = image.height * image.width;
    float percentage = diff/count*100;
    printf("Difference:          %.2f %%\n\n", percentage);
    
    return 0;
}
