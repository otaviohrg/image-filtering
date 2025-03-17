#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <omp.h>

#include "gif_lib.h"

/* Represent one pixel from the image */
typedef struct pixel
{
    int r ; /* Red */
    int g ; /* Green */
    int b ; /* Blue */
} pixel ;

/* Represent one GIF image (animated or not */
typedef struct animated_gif
{
    int n_images ; /* Number of images */
    int * width ; /* Width of each image */
    int * height ; /* Height of each image */
    pixel ** p ; /* Pixels of each image */
    GifFileType * g ; /* Internal representation.
                         DO NOT MODIFY */
} animated_gif ;


__global__ void apply_gray_filter_kernel(pixel *d_pixels, int width, int height) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int idy = blockIdx.y * blockDim.y + threadIdx.y;

    if (idx < width && idy < height) {
        int pixelIndex = (idy * width) + idx;

        int moy = (d_pixels[pixelIndex].r + d_pixels[pixelIndex].g + d_pixels[pixelIndex].b) / 3;

        if (moy < 0) moy = 0;
        if (moy > 255) moy = 255;

        d_pixels[pixelIndex].r = moy;
        d_pixels[pixelIndex].g = moy;
        d_pixels[pixelIndex].b = moy;
    }
}

extern "C" void
apply_gray_filter_gpu( animated_gif * image ){
    pixel *d_pixels;
    int *d_width, *d_height;

    #pragma omp parallel for
    for (int i = 0; i < image->n_images; i++) {
        cudaMalloc(&d_pixels, image->width[i] * image->height[i] * sizeof(pixel));
        int width = image->width[i];
        int height = image->height[i];

        cudaMemcpy(d_width, &image->width[i], sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(d_height, &image->height[i], sizeof(int), cudaMemcpyHostToDevice);

        cudaMemcpy(d_pixels, image->p[i], image->width[i] * image->height[i] * sizeof(pixel), cudaMemcpyHostToDevice);

        dim3 blockSize(32, 32, 1);
        dim3 gridSize((image->width[i] + blockSize.x - 1) / blockSize.x,
                      (image->height[i] + blockSize.y - 1) / blockSize.y,
                      1);

        apply_gray_filter_kernel<<<gridSize, blockSize>>>(d_pixels, width, height);

        cudaDeviceSynchronize();
        cudaMemcpy(image->p[i], d_pixels, image->width[i] * image->height[i] * sizeof(pixel), cudaMemcpyDeviceToHost);

        cudaFree(d_pixels);

    }
}

#define CONV(l,c,nb_c) \
    (l)*(nb_c)+(c)

__global__ void apply_gray_line_kernel(pixel *d_pixels, int *d_width, int *d_height, int n_images) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int idy = blockIdx.y * blockDim.y + threadIdx.y;
    int frame_idx = blockIdx.z;

    if (frame_idx < n_images) {
        int width = d_width[frame_idx];
        int height = d_height[frame_idx];

        // Apply the gray line transformation only if we're within the image dimensions
        if (idy < 10 && idx >= width / 2 && idx < width) {
            int pixelIndex = (frame_idx * width * height) + (idy * width) + idx;
            d_pixels[pixelIndex].r = 0;
            d_pixels[pixelIndex].g = 0;
            d_pixels[pixelIndex].b = 0;
        }
    }
}

extern "C" void apply_gray_line_gpu(animated_gif *image) {
    pixel *d_pixels;
    int *d_width, *d_height;
    size_t total_pixels = 0;

    for (int i = 0; i < image->n_images; i++) {
        total_pixels += image->width[i] * image->height[i];
    }

    cudaMalloc(&d_pixels, total_pixels * sizeof(pixel));
    cudaMalloc(&d_width, image->n_images * sizeof(int));
    cudaMalloc(&d_height, image->n_images * sizeof(int));

    cudaMemcpy(d_width, image->width, image->n_images * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_height, image->height, image->n_images * sizeof(int), cudaMemcpyHostToDevice);

    size_t offset = 0;
    for (int i = 0; i < image->n_images; i++) {
        cudaMemcpy(d_pixels + offset, image->p[i], image->width[i] * image->height[i] * sizeof(pixel), cudaMemcpyHostToDevice);
        offset += image->width[i] * image->height[i];
    }

    dim3 blockSize(32, 32, 1);
    dim3 gridSize((image->width[0] + blockSize.x - 1) / blockSize.x,
                  (10 + blockSize.y - 1) / blockSize.y,
                  image->n_images);

    apply_gray_line_kernel<<<gridSize, blockSize>>>(d_pixels, d_width, d_height, image->n_images);

    cudaDeviceSynchronize();

    offset = 0;
    for (int i = 0; i < image->n_images; i++) {
        cudaMemcpy(image->p[i], d_pixels + offset, image->width[i] * image->height[i] * sizeof(pixel), cudaMemcpyDeviceToHost);
        offset += image->width[i] * image->height[i];
    }

    cudaFree(d_pixels);
    cudaFree(d_width);
    cudaFree(d_height);
}

__global__ void apply_blur_filter_kernel(pixel *d_pixels, pixel *d_new_pixels, int width, int height, int size, int threshold, bool *d_end_flag) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int idy = blockIdx.y * blockDim.y + threadIdx.y;

    if (idx < width && idy < height) {
        int pixelIndex = idy * width + idx;
        
        if (idy>=size && idy < height / 10-size && idx>=size && idx < width-size) {
            int stencil_j, stencil_k ;
            int t_r = 0 ;
            int t_g = 0 ;
            int t_b = 0 ;
            for (stencil_j = -size; stencil_j <= size; stencil_j++) {
                for (stencil_k = -size; stencil_k <= size; stencil_k++) {
                    int neighbor_x = idx + stencil_k;
                    int neighbor_y = idy + stencil_j;

                    int neighborIndex = neighbor_y * width + neighbor_x;
                    t_r += d_pixels[neighborIndex].r;
                    t_g += d_pixels[neighborIndex].g;
                    t_b += d_pixels[neighborIndex].b;
                }
            }
            d_new_pixels[pixelIndex].r = t_r / ( (2*size+1)*(2*size+1) ) ;
            d_new_pixels[pixelIndex].g = t_g / ( (2*size+1)*(2*size+1) ) ;
            d_new_pixels[pixelIndex].b = t_b / ( (2*size+1)*(2*size+1) ) ;
        }

        if (idy>=height/10-size && idy < height*0.9+size && idx>=size && idx < width-size) {
            d_new_pixels[pixelIndex].r = d_pixels[pixelIndex].r ;
            d_new_pixels[pixelIndex].g = d_pixels[pixelIndex].g ;
            d_new_pixels[pixelIndex].b = d_pixels[pixelIndex].b ;
        }

        if (idy>=height*0.9+size && idy < height-size && idx>=size && idx < width-size) {
            int stencil_j, stencil_k ;
            int t_r = 0 ;
            int t_g = 0 ;
            int t_b = 0 ;
            for (stencil_j = -size; stencil_j <= size; stencil_j++) {
                for (stencil_k = -size; stencil_k <= size; stencil_k++) {
                    int neighbor_x = idx + stencil_k;
                    int neighbor_y = idy + stencil_j;

                    int neighborIndex = neighbor_y * width + neighbor_x;
                    t_r += d_pixels[neighborIndex].r;
                    t_g += d_pixels[neighborIndex].g;
                    t_b += d_pixels[neighborIndex].b;
                }
            }
            d_new_pixels[pixelIndex].r = t_r / ( (2*size+1)*(2*size+1) ) ;
            d_new_pixels[pixelIndex].g = t_g / ( (2*size+1)*(2*size+1) ) ;
            d_new_pixels[pixelIndex].b = t_b / ( (2*size+1)*(2*size+1) ) ;
        }


        float diff_r = d_new_pixels[pixelIndex].r - d_pixels[pixelIndex].r;
        float diff_g = d_new_pixels[pixelIndex].g - d_pixels[pixelIndex].g;
        float diff_b = d_new_pixels[pixelIndex].b - d_pixels[pixelIndex].b;
        
        

        if ( diff_r > threshold || -diff_r > threshold
            ||
            diff_g > threshold || -diff_g > threshold
            ||
            diff_b > threshold || -diff_b > threshold
                ) {
            *d_end_flag = false;
        }     
    }
}

extern "C" void apply_blur_filter_gpu(animated_gif *image, int size, int threshold) {
    pixel *d_pixels, *d_new_pixels;
    bool *d_end_flag;
    bool end;

    #pragma omp parallel for
    for (int i = 0; i < image->n_images; i++) {
        int width = image->width[i];
        int height = image->height[i];

        cudaMalloc(&d_pixels, width * height * sizeof(pixel));
        cudaMalloc(&d_new_pixels, width * height * sizeof(pixel));
        cudaMalloc(&d_end_flag, sizeof(bool));

        cudaMemcpy(d_pixels, image->p[i], width * height * sizeof(pixel), cudaMemcpyHostToDevice);
        cudaMemcpy(d_new_pixels, image->p[i], width * height * sizeof(pixel), cudaMemcpyHostToDevice);
        cudaMemset(d_end_flag, true, sizeof(bool));  // Set the end flag to true initially

        dim3 blockSize(32, 32, 1);
        dim3 gridSize((width + blockSize.x - 1) / blockSize.x,
                        (height + blockSize.y - 1) / blockSize.y,
                        1);

        do {
            end = true;
            cudaMemset(d_end_flag, true, sizeof(bool));
            apply_blur_filter_kernel<<<gridSize, blockSize>>>(d_pixels, d_new_pixels, width, height, size, threshold, d_end_flag);
            cudaDeviceSynchronize();
            cudaMemcpy(d_pixels, d_new_pixels, width * height * sizeof(pixel), cudaMemcpyDeviceToDevice);
            cudaMemcpy(&end, d_end_flag, sizeof(bool), cudaMemcpyDeviceToHost);
        } while (threshold > 0 && !end );

        cudaMemcpy(image->p[i], d_new_pixels, width * height * sizeof(pixel), cudaMemcpyDeviceToHost);

        cudaFree(d_pixels);
        cudaFree(d_new_pixels);
        cudaFree(d_end_flag);
    }
}


__global__ void apply_sobel_filter_kernel(pixel *d_pixels, pixel *d_sobel, int width, int height) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int idy = blockIdx.y * blockDim.y + threadIdx.y;

    if (idx > 0 && idx < width - 1 && idy > 0 && idy < height - 1) {
        int pixel_blue_no, pixel_blue_n, pixel_blue_ne;
        int pixel_blue_so, pixel_blue_s, pixel_blue_se;
        int pixel_blue_o, pixel_blue_e;
        //int pixel_blue

        float deltaX_blue, deltaY_blue, val_blue;

        pixel_blue_no = d_pixels[(idy - 1) * width + idx - 1].b;
        pixel_blue_n  = d_pixels[(idy - 1) * width + idx].b;
        pixel_blue_ne = d_pixels[(idy - 1) * width + idx +1].b;
        pixel_blue_so  = d_pixels[(idy + 1) * width + idx-1].b;
        pixel_blue_s    = d_pixels[(idy + 1) * width + idx].b;
        pixel_blue_se  = d_pixels[(idy + 1) * width + idx+1].b;
        pixel_blue_o = d_pixels[idy * width + idx -1].b;
        //pixel_blue  = d_pixels[idy * width + idx].b;
        pixel_blue_e = d_pixels[idy * width + idx +1].b;

        deltaX_blue = -pixel_blue_no + pixel_blue_ne - 2*pixel_blue_o + 2*pixel_blue_e - pixel_blue_so + pixel_blue_se;             
        deltaY_blue = pixel_blue_se + 2*pixel_blue_s + pixel_blue_so - pixel_blue_ne - 2*pixel_blue_n - pixel_blue_no;
        val_blue = sqrt(deltaX_blue * deltaX_blue + deltaY_blue * deltaY_blue)/4;

        int index = idy * width + idx;

        if (val_blue > 50) {
            d_sobel[index].r = 255;
            d_sobel[index].g = 255;
            d_sobel[index].b = 255;
        } else {
            d_sobel[index].r = 0;
            d_sobel[index].g = 0;
            d_sobel[index].b = 0;
        }
    }
}

extern "C" void apply_sobel_filter_gpu(animated_gif *image){
        pixel *d_pixels;
        pixel *sobel;
        pixel *d_sobel;
        int *d_width, *d_height;

    #pragma omp parallel for
        for (int i = 0; i < image->n_images; i++) {
            int width = image->width[i];
            int height = image->height[i];

            sobel = (pixel *)malloc(width * height * sizeof( pixel ) ) ;
            cudaMalloc(&d_pixels, image->width[i] * image->height[i] * sizeof(pixel));
            cudaMalloc(&d_sobel, image->width[i] * image->height[i] * sizeof(pixel));

            cudaMemcpy(d_width, &image->width[i], sizeof(int), cudaMemcpyHostToDevice);
            cudaMemcpy(d_height, &image->height[i], sizeof(int), cudaMemcpyHostToDevice);

            cudaMemcpy(d_pixels, image->p[i], image->width[i] * image->height[i] * sizeof(pixel), cudaMemcpyHostToDevice);

            dim3 blockSize(32, 32, 1);
            dim3 gridSize((image->width[i] + blockSize.x - 1) / blockSize.x,
                          (image->height[i] + blockSize.y - 1) / blockSize.y,
                          1);

            apply_sobel_filter_kernel<<<gridSize, blockSize>>>(d_pixels, d_sobel, width, height);

            cudaDeviceSynchronize();
            cudaMemcpy(sobel, d_sobel, image->width[i] * image->height[i] * sizeof(pixel), cudaMemcpyDeviceToHost);

            for(int j=1; j<height-1; j++)
            {
                for(int k=1; k<width-1; k++)
                {
                    image->p[i][CONV(j  ,k  ,width)].r = sobel[CONV(j  ,k  ,width)].r ;
                    image->p[i][CONV(j  ,k  ,width)].g = sobel[CONV(j  ,k  ,width)].g ;
                    image->p[i][CONV(j  ,k  ,width)].b = sobel[CONV(j  ,k  ,width)].b ;
                }
            }

            cudaFree(d_pixels);
            cudaFree(d_sobel);
            free(sobel);
        }
        
    }
