/*
 * INF560
 *
 * Image Filtering Project
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>

#include "gif_lib.h"

/* Set this macro to 1 to enable debugging information */
#define SOBELF_DEBUG 0

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

/*
 * Load a GIF image from a file and return a
 * structure of type animated_gif.
 */
animated_gif *
load_pixels( char * filename ) 
{
    GifFileType * g ;
    ColorMapObject * colmap ;
    int error ;
    int n_images ;
    int * width ;
    int * height ;
    pixel ** p ;
    int i ;
    animated_gif * image ;

    /* Open the GIF image (read mode) */
    g = DGifOpenFileName( filename, &error ) ;
    if ( g == NULL ) 
    {
        fprintf( stderr, "Error DGifOpenFileName %s\n", filename ) ;
        return NULL ;
    }

    /* Read the GIF image */
    error = DGifSlurp( g ) ;
    if ( error != GIF_OK )
    {
        fprintf( stderr, 
                "Error DGifSlurp: %d <%s>\n", error, GifErrorString(g->Error) ) ;
        return NULL ;
    }

    /* Grab the number of images and the size of each image */
    n_images = g->ImageCount ;

    width = (int *)malloc( n_images * sizeof( int ) ) ;
    if ( width == NULL )
    {
        fprintf( stderr, "Unable to allocate width of size %d\n",
                n_images ) ;
        return 0 ;
    }

    height = (int *)malloc( n_images * sizeof( int ) ) ;
    if ( height == NULL )
    {
        fprintf( stderr, "Unable to allocate height of size %d\n",
                n_images ) ;
        return 0 ;
    }

    /* Fill the width and height */
    for ( i = 0 ; i < n_images ; i++ ) 
    {
        width[i] = g->SavedImages[i].ImageDesc.Width ;
        height[i] = g->SavedImages[i].ImageDesc.Height ;

    #if SOBELF_DEBUG
        printf( "Image %d: l:%d t:%d w:%d h:%d interlace:%d localCM:%p\n",
                i, 
                g->SavedImages[i].ImageDesc.Left,
                g->SavedImages[i].ImageDesc.Top,
                g->SavedImages[i].ImageDesc.Width,
                g->SavedImages[i].ImageDesc.Height,
                g->SavedImages[i].ImageDesc.Interlace,
                g->SavedImages[i].ImageDesc.ColorMap
                ) ;
    #endif
    }


    /* Get the global colormap */
    colmap = g->SColorMap ;
    if ( colmap == NULL ) 
    {
        fprintf( stderr, "Error global colormap is NULL\n" ) ;
        return NULL ;
    }

    #if SOBELF_DEBUG
    printf( "Global color map: count:%d bpp:%d sort:%d\n",
            g->SColorMap->ColorCount,
            g->SColorMap->BitsPerPixel,
            g->SColorMap->SortFlag
            ) ;
    #endif

    /* Allocate the array of pixels to be returned */
    p = (pixel **)malloc( n_images * sizeof( pixel * ) ) ;
    if ( p == NULL )
    {
        fprintf( stderr, "Unable to allocate array of %d images\n",
                n_images ) ;
        return NULL ;
    }

    for ( i = 0 ; i < n_images ; i++ ) 
    {
        p[i] = (pixel *)malloc( width[i] * height[i] * sizeof( pixel ) ) ;
        if ( p[i] == NULL )
        {
        fprintf( stderr, "Unable to allocate %d-th array of %d pixels\n",
                i, width[i] * height[i] ) ;
        return NULL ;
        }
    }
    
    /* Fill pixels */

    /* For each image */
    for ( i = 0 ; i < n_images ; i++ )
    {
        int j ;

        /* Get the local colormap if needed */
        if ( g->SavedImages[i].ImageDesc.ColorMap )
        {

            /* TODO No support for local color map */
            fprintf( stderr, "Error: application does not support local colormap\n" ) ;
            return NULL ;

            colmap = g->SavedImages[i].ImageDesc.ColorMap ;
        }

        /* Traverse the image and fill pixels */
        for ( j = 0 ; j < width[i] * height[i] ; j++ ) 
        {
            int c ;

            c = g->SavedImages[i].RasterBits[j] ;

            p[i][j].r = colmap->Colors[c].Red ;
            p[i][j].g = colmap->Colors[c].Green ;
            p[i][j].b = colmap->Colors[c].Blue ;
        }
    }

    /* Allocate image info */
    image = (animated_gif *)malloc( sizeof(animated_gif) ) ;
    if ( image == NULL ) 
    {
        fprintf( stderr, "Unable to allocate memory for animated_gif\n" ) ;
        return NULL ;
    }

    /* Fill image fields */
    image->n_images = n_images ;
    image->width = width ;
    image->height = height ;
    image->p = p ;
    image->g = g ;

    #if SOBELF_DEBUG
    printf( "-> GIF w/ %d image(s) with first image of size %d x %d\n",
            image->n_images, image->width[0], image->height[0] ) ;
    #endif

    return image ;
}

int 
output_modified_read_gif( char * filename, GifFileType * g ) 
{
    GifFileType * g2 ;
    int error2 ;

    #if SOBELF_DEBUG
    printf( "Starting output to file %s\n", filename ) ;
    #endif

    g2 = EGifOpenFileName( filename, false, &error2 ) ;
    if ( g2 == NULL )
    {
        fprintf( stderr, "Error EGifOpenFileName %s\n",
                filename ) ;
        return 0 ;
    }

    g2->SWidth = g->SWidth ;
    g2->SHeight = g->SHeight ;
    g2->SColorResolution = g->SColorResolution ;
    g2->SBackGroundColor = g->SBackGroundColor ;
    g2->AspectByte = g->AspectByte ;
    g2->SColorMap = g->SColorMap ;
    g2->ImageCount = g->ImageCount ;
    g2->SavedImages = g->SavedImages ;
    g2->ExtensionBlockCount = g->ExtensionBlockCount ;
    g2->ExtensionBlocks = g->ExtensionBlocks ;

    error2 = EGifSpew( g2 ) ;
    if ( error2 != GIF_OK ) 
    {
        fprintf( stderr, "Error after writing g2: %d <%s>\n", 
                error2, GifErrorString(g2->Error) ) ;
        return 0 ;
    }

    return 1 ;
}


int
store_pixels( char * filename, animated_gif * image )
{
    int n_colors = 0 ;
    pixel ** p ;
    int i, j, k ;
    GifColorType * colormap ;

    /* Initialize the new set of colors */
    colormap = (GifColorType *)malloc( 256 * sizeof( GifColorType ) ) ;
    if ( colormap == NULL ) 
    {
        fprintf( stderr,
                "Unable to allocate 256 colors\n" ) ;
        return 0 ;
    }

    /* Everything is white by default */
    for ( i = 0 ; i < 256 ; i++ ) 
    {
        colormap[i].Red = 255 ;
        colormap[i].Green = 255 ;
        colormap[i].Blue = 255 ;
    }

    /* Change the background color and store it */
    int moy ;
    moy = (
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Red
            +
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Green
            +
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Blue
            )/3 ;
    if ( moy < 0 ) moy = 0 ;
    if ( moy > 255 ) moy = 255 ;

    #if SOBELF_DEBUG
    printf( "[DEBUG] Background color (%d,%d,%d) -> (%d,%d,%d)\n",
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Red,
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Green,
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Blue,
            moy, moy, moy ) ;
    #endif

    colormap[0].Red = moy ;
    colormap[0].Green = moy ;
    colormap[0].Blue = moy ;

    image->g->SBackGroundColor = 0 ;

    n_colors++ ;

    /* Process extension blocks in main structure */
    for ( j = 0 ; j < image->g->ExtensionBlockCount ; j++ )
    {
        int f ;

        f = image->g->ExtensionBlocks[j].Function ;
        if ( f == GRAPHICS_EXT_FUNC_CODE )
        {
            int tr_color = image->g->ExtensionBlocks[j].Bytes[3] ;

            if ( tr_color >= 0 &&
                    tr_color < 255 )
            {

                int found = -1 ;

                moy = 
                    (
                     image->g->SColorMap->Colors[ tr_color ].Red
                     +
                     image->g->SColorMap->Colors[ tr_color ].Green
                     +
                     image->g->SColorMap->Colors[ tr_color ].Blue
                    ) / 3 ;
                if ( moy < 0 ) moy = 0 ;
                if ( moy > 255 ) moy = 255 ;

    #if SOBELF_DEBUG
                printf( "[DEBUG] Transparency color image %d (%d,%d,%d) -> (%d,%d,%d)\n",
                        i,
                        image->g->SColorMap->Colors[ tr_color ].Red,
                        image->g->SColorMap->Colors[ tr_color ].Green,
                        image->g->SColorMap->Colors[ tr_color ].Blue,
                        moy, moy, moy ) ;
    #endif

                for ( k = 0 ; k < n_colors ; k++ )
                {
                    if ( 
                            moy == colormap[k].Red
                            &&
                            moy == colormap[k].Green
                            &&
                            moy == colormap[k].Blue
                       )
                    {
                        found = k ;
                    }
                }
                if ( found == -1  ) 
                {
                    if ( n_colors >= 256 ) 
                    {
                        fprintf( stderr, 
                                "Error: Found too many colors inside the image\n"
                               ) ;
                        return 0 ;
                    }

    #if SOBELF_DEBUG
                    printf( "[DEBUG]\tNew color %d\n",
                            n_colors ) ;
    #endif

                    colormap[n_colors].Red = moy ;
                    colormap[n_colors].Green = moy ;
                    colormap[n_colors].Blue = moy ;


                    image->g->ExtensionBlocks[j].Bytes[3] = n_colors ;

                    n_colors++ ;
                } else
                {
    #if SOBELF_DEBUG
                    printf( "[DEBUG]\tFound existing color %d\n",
                            found ) ;
    #endif
                    image->g->ExtensionBlocks[j].Bytes[3] = found ;
                }
            }
        }
    }

    for ( i = 0 ; i < image->n_images ; i++ )
    {
        for ( j = 0 ; j < image->g->SavedImages[i].ExtensionBlockCount ; j++ )
        {
            int f ;

            f = image->g->SavedImages[i].ExtensionBlocks[j].Function ;
            if ( f == GRAPHICS_EXT_FUNC_CODE )
            {
                int tr_color = image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] ;

                if ( tr_color >= 0 &&
                        tr_color < 255 )
                {

                    int found = -1 ;

                    moy = 
                        (
                         image->g->SColorMap->Colors[ tr_color ].Red
                         +
                         image->g->SColorMap->Colors[ tr_color ].Green
                         +
                         image->g->SColorMap->Colors[ tr_color ].Blue
                        ) / 3 ;
                    if ( moy < 0 ) moy = 0 ;
                    if ( moy > 255 ) moy = 255 ;

    #if SOBELF_DEBUG
                    printf( "[DEBUG] Transparency color image %d (%d,%d,%d) -> (%d,%d,%d)\n",
                            i,
                            image->g->SColorMap->Colors[ tr_color ].Red,
                            image->g->SColorMap->Colors[ tr_color ].Green,
                            image->g->SColorMap->Colors[ tr_color ].Blue,
                            moy, moy, moy ) ;
    #endif

                    for ( k = 0 ; k < n_colors ; k++ )
                    {
                        if ( 
                                moy == colormap[k].Red
                                &&
                                moy == colormap[k].Green
                                &&
                                moy == colormap[k].Blue
                           )
                        {
                            found = k ;
                        }
                    }
                    if ( found == -1  ) 
                    {
                        if ( n_colors >= 256 ) 
                        {
                            fprintf( stderr, 
                                    "Error: Found too many colors inside the image\n"
                                   ) ;
                            return 0 ;
                        }

    #if SOBELF_DEBUG
                        printf( "[DEBUG]\tNew color %d\n",
                                n_colors ) ;
    #endif

                        colormap[n_colors].Red = moy ;
                        colormap[n_colors].Green = moy ;
                        colormap[n_colors].Blue = moy ;


                        image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] = n_colors ;

                        n_colors++ ;
                    } else
                    {
    #if SOBELF_DEBUG
                        printf( "[DEBUG]\tFound existing color %d\n",
                                found ) ;
    #endif
                        image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] = found ;
                    }
                }
            }
        }
    }

    #if SOBELF_DEBUG
    printf( "[DEBUG] Number of colors after background and transparency: %d\n",
            n_colors ) ;
    #endif

    p = image->p ;

    /* Find the number of colors inside the image */
    for ( i = 0 ; i < image->n_images ; i++ )
    {

    #if SOBELF_DEBUG
        printf( "OUTPUT: Processing image %d (total of %d images) -> %d x %d\n",
                i, image->n_images, image->width[i], image->height[i] ) ;
    #endif

        for ( j = 0 ; j < image->width[i] * image->height[i] ; j++ ) 
        {
            int found = 0 ;
            for ( k = 0 ; k < n_colors ; k++ )
            {
                if ( p[i][j].r == colormap[k].Red &&
                        p[i][j].g == colormap[k].Green &&
                        p[i][j].b == colormap[k].Blue )
                {
                    found = 1 ;
                }
            }

            if ( found == 0 ) 
            {
                if ( n_colors >= 256 ) 
                {
                    fprintf( stderr, 
                            "Error: Found too many colors inside the image\n"
                           ) ;
                    return 0 ;
                }

    #if SOBELF_DEBUG
                printf( "[DEBUG] Found new %d color (%d,%d,%d)\n",
                        n_colors, p[i][j].r, p[i][j].g, p[i][j].b ) ;
    #endif

                colormap[n_colors].Red = p[i][j].r ;
                colormap[n_colors].Green = p[i][j].g ;
                colormap[n_colors].Blue = p[i][j].b ;
                n_colors++ ;
            }
        }
    }

    #if SOBELF_DEBUG
    printf( "OUTPUT: found %d color(s)\n", n_colors ) ;
    #endif


    /* Round up to a power of 2 */
    if ( n_colors != (1 << GifBitSize(n_colors) ) )
    {
        n_colors = (1 << GifBitSize(n_colors) ) ;
    }

    #if SOBELF_DEBUG
    printf( "OUTPUT: Rounding up to %d color(s)\n", n_colors ) ;
    #endif

    /* Change the color map inside the animated gif */
    ColorMapObject * cmo ;

    cmo = GifMakeMapObject( n_colors, colormap ) ;
    if ( cmo == NULL )
    {
        fprintf( stderr, "Error while creating a ColorMapObject w/ %d color(s)\n",
                n_colors ) ;
        return 0 ;
    }

    image->g->SColorMap = cmo ;

    /* Update the raster bits according to color map */
    for ( i = 0 ; i < image->n_images ; i++ )
    {
        for ( j = 0 ; j < image->width[i] * image->height[i] ; j++ ) 
        {
            int found_index = -1 ;
            for ( k = 0 ; k < n_colors ; k++ ) 
            {
                if ( p[i][j].r == image->g->SColorMap->Colors[k].Red &&
                        p[i][j].g == image->g->SColorMap->Colors[k].Green &&
                        p[i][j].b == image->g->SColorMap->Colors[k].Blue )
                {
                    found_index = k ;
                }
            }

            if ( found_index == -1 ) 
            {
                fprintf( stderr,
                        "Error: Unable to find a pixel in the color map\n" ) ;
                return 0 ;
            }

            image->g->SavedImages[i].RasterBits[j] = found_index ;
        }
    }


    /* Write the final image */
    if ( !output_modified_read_gif( filename, image->g ) ) { return 0 ; }

    return 1 ;
}

void
apply_gray_filter( animated_gif * image )
{
    int i, j ;
    pixel ** p ;

    p = image->p ;

    for ( i = 0 ; i < image->n_images ; i++ )
    {
        for ( j = 0 ; j < image->width[i] * image->height[i] ; j++ )
        {
            int moy ;

            moy = (p[i][j].r + p[i][j].g + p[i][j].b)/3 ;
            if ( moy < 0 ) moy = 0 ;
            if ( moy > 255 ) moy = 255 ;

            p[i][j].r = moy ;
            p[i][j].g = moy ;
            p[i][j].b = moy ;
        }
    }
}

void apply_gray_filter_gpu(animated_gif * image);

#define CONV(l,c,nb_c) \
    (l)*(nb_c)+(c)

void apply_gray_line( animated_gif * image ) 
{
    int i, j, k ;
    pixel ** p ;

    p = image->p ;

    for ( i = 0 ; i < image->n_images ; i++ )
    {
        for ( j = 0 ; j < 10 ; j++ )
        {
            for ( k = image->width[i]/2 ; k < image->width[i] ; k++ )
            {
            p[i][CONV(j,k,image->width[i])].r = 0 ;
            p[i][CONV(j,k,image->width[i])].g = 0 ;
            p[i][CONV(j,k,image->width[i])].b = 0 ;
            }
        }
    }
}

void apply_gray_line_gpu( animated_gif * image);

void
apply_blur_filter( animated_gif * image, int size, int threshold )
{
    int i, j, k ;
    int width, height ;
    int end = 0 ;
    int n_iter = 0 ;

    pixel ** p ;
    pixel * new ;

    /* Get the pixels of all images */
    p = image->p ;


    /* Process all images */
    #pragma omp parallel for
    for ( i = 0 ; i < image->n_images ; i++ )
    {
        n_iter = 0 ;
        width = image->width[i] ;
        height = image->height[i] ;

        /* Allocate array of new pixels */
        new = (pixel *)malloc(width * height * sizeof( pixel ) ) ;


        /* Perform at least one blur iteration */
        do
        {
            end = 1 ;
            n_iter++ ;


	for(j=0; j<height-1; j++)
	{
		for(k=0; k<width-1; k++)
		{
			new[CONV(j,k,width)].r = p[i][CONV(j,k,width)].r ;
			new[CONV(j,k,width)].g = p[i][CONV(j,k,width)].g ;
			new[CONV(j,k,width)].b = p[i][CONV(j,k,width)].b ;
		}
	}

            /* Apply blur on top part of image (10%) */
            for(j=size; j<height/10-size; j++)
            {
                for(k=size; k<width-size; k++)
                {
                    int stencil_j, stencil_k ;
                    int t_r = 0 ;
                    int t_g = 0 ;
                    int t_b = 0 ;

                    for ( stencil_j = -size ; stencil_j <= size ; stencil_j++ )
                    {
                        for ( stencil_k = -size ; stencil_k <= size ; stencil_k++ )
                        {
                            t_r += p[i][CONV(j+stencil_j,k+stencil_k,width)].r ;
                            t_g += p[i][CONV(j+stencil_j,k+stencil_k,width)].g ;
                            t_b += p[i][CONV(j+stencil_j,k+stencil_k,width)].b ;
                        }
                    }

                    new[CONV(j,k,width)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV(j,k,width)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV(j,k,width)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
                }
            }

            /* Copy the middle part of the image */
            for(j=height/10-size; j<height*0.9+size; j++)
            {
                for(k=size; k<width-size; k++)
                {
                    new[CONV(j,k,width)].r = p[i][CONV(j,k,width)].r ; 
                    new[CONV(j,k,width)].g = p[i][CONV(j,k,width)].g ; 
                    new[CONV(j,k,width)].b = p[i][CONV(j,k,width)].b ; 
                }
            }

            /* Apply blur on the bottom part of the image (10%) */
            for(j=height*0.9+size; j<height-size; j++)
            {
                for(k=size; k<width-size; k++)
                {
                    int stencil_j, stencil_k ;
                    int t_r = 0 ;
                    int t_g = 0 ;
                    int t_b = 0 ;

                    for ( stencil_j = -size ; stencil_j <= size ; stencil_j++ )
                    {
                        for ( stencil_k = -size ; stencil_k <= size ; stencil_k++ )
                        {
                            t_r += p[i][CONV(j+stencil_j,k+stencil_k,width)].r ;
                            t_g += p[i][CONV(j+stencil_j,k+stencil_k,width)].g ;
                            t_b += p[i][CONV(j+stencil_j,k+stencil_k,width)].b ;
                        }
                    }

                    new[CONV(j,k,width)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV(j,k,width)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV(j,k,width)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
                }
            }

            for(j=1; j<height-1; j++)
            {
                for(k=1; k<width-1; k++)
                {

                    float diff_r ;
                    float diff_g ;
                    float diff_b ;

                    diff_r = (new[CONV(j  ,k  ,width)].r - p[i][CONV(j  ,k  ,width)].r) ;
                    diff_g = (new[CONV(j  ,k  ,width)].g - p[i][CONV(j  ,k  ,width)].g) ;
                    diff_b = (new[CONV(j  ,k  ,width)].b - p[i][CONV(j  ,k  ,width)].b) ;

                    if ( diff_r > threshold || -diff_r > threshold 
                            ||
                             diff_g > threshold || -diff_g > threshold
                             ||
                              diff_b > threshold || -diff_b > threshold
                       ) {
                        end = 0 ;
                    }

                    p[i][CONV(j  ,k  ,width)].r = new[CONV(j  ,k  ,width)].r ;
                    p[i][CONV(j  ,k  ,width)].g = new[CONV(j  ,k  ,width)].g ;
                    p[i][CONV(j  ,k  ,width)].b = new[CONV(j  ,k  ,width)].b ;
                }
            }

        }
        while ( threshold > 0 && !end ) ;

    #if SOBELF_DEBUG
	printf( "BLUR: number of iterations for image %d\n", n_iter ) ;
    #endif

        free (new) ;
    }

}

void apply_blur_filter_gpu( animated_gif * image, int size, int threshold);

void
apply_sobel_filter( animated_gif * image )
{
    int i, j, k ;
    int width, height ;

    pixel ** p ;

    p = image->p ;

    for ( i = 0 ; i < image->n_images ; i++ )
    {
        width = image->width[i] ;
        height = image->height[i] ;

        pixel * sobel ;

        sobel = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

        for(j=1; j<height-1; j++)
        {
            for(k=1; k<width-1; k++)
            {
                int pixel_blue_no, pixel_blue_n, pixel_blue_ne;
                int pixel_blue_so, pixel_blue_s, pixel_blue_se;
                int pixel_blue_o , pixel_blue  , pixel_blue_e ;

                float deltaX_blue ;
                float deltaY_blue ;
                float val_blue;

                pixel_blue_no = p[i][CONV(j-1,k-1,width)].b ;
                pixel_blue_n  = p[i][CONV(j-1,k  ,width)].b ;
                pixel_blue_ne = p[i][CONV(j-1,k+1,width)].b ;
                pixel_blue_so = p[i][CONV(j+1,k-1,width)].b ;
                pixel_blue_s  = p[i][CONV(j+1,k  ,width)].b ;
                pixel_blue_se = p[i][CONV(j+1,k+1,width)].b ;
                pixel_blue_o  = p[i][CONV(j  ,k-1,width)].b ;
                pixel_blue    = p[i][CONV(j  ,k  ,width)].b ;
                pixel_blue_e  = p[i][CONV(j  ,k+1,width)].b ;

                deltaX_blue = -pixel_blue_no + pixel_blue_ne - 2*pixel_blue_o + 2*pixel_blue_e - pixel_blue_so + pixel_blue_se;             

                deltaY_blue = pixel_blue_se + 2*pixel_blue_s + pixel_blue_so - pixel_blue_ne - 2*pixel_blue_n - pixel_blue_no;

                val_blue = sqrt(deltaX_blue * deltaX_blue + deltaY_blue * deltaY_blue)/4;


                if ( val_blue > 50 ) 
                {
                    sobel[CONV(j  ,k  ,width)].r = 255 ;
                    sobel[CONV(j  ,k  ,width)].g = 255 ;
                    sobel[CONV(j  ,k  ,width)].b = 255 ;
                } else
                {
                    sobel[CONV(j  ,k  ,width)].r = 0 ;
                    sobel[CONV(j  ,k  ,width)].g = 0 ;
                    sobel[CONV(j  ,k  ,width)].b = 0 ;
                }
            }
        }

        for(j=1; j<height-1; j++)
        {
            for(k=1; k<width-1; k++)
            {
                p[i][CONV(j  ,k  ,width)].r = sobel[CONV(j  ,k  ,width)].r ;
                p[i][CONV(j  ,k  ,width)].g = sobel[CONV(j  ,k  ,width)].g ;
                p[i][CONV(j  ,k  ,width)].b = sobel[CONV(j  ,k  ,width)].b ;
            }
        }

        free (sobel) ;
    }

}

void apply_sobel_filter_gpu( animated_gif * image);

//transpose the (1d array) //i think its best to avoid parallelising on this level, or leave it for cuda
pixel *
transpose (pixel * pixel_array, int width, int height){
    pixel * temp;
    temp = (pixel *)malloc(width * height * sizeof(pixel));
    for (int i = 0; i < height; i++){
        for (int j = 0; j < width; j++){
            temp[(j * height) + i] = pixel_array[(i * width) + j];
        }
    }
    return temp;
}

//the paddings here are horizontal (cutting the height)
pixel *
pad_frame (pixel * pixel_array, int width, int height, int * splits, int n_splits, int pad_size){
    pixel * temp = (pixel *)malloc((height + (2 * pad_size * (n_splits - 1))) * width * sizeof(pixel)); 
    //need to be careful here esp when pad > split size, but if thats the case you should not really split this small anymore
    
    int split_idx = 1; //the first is the very point is 0, cause its more convenient
    int i_temp = 0;
    for (int i = 0; i < height; i++){
        if (split_idx < n_splits && i == splits[split_idx] + pad_size){ 
            for (int j = 0; j < 2 * pad_size * width; j++){
                temp[i_temp] = pixel_array[((i - (2 *pad_size)) * width) + j]; //+1 because you still want to copy this line too
                i_temp++;
            }
            split_idx ++;
        }
        for (int j = 0; j < width; j++){
            temp[i_temp] = pixel_array[(i *  width) + j];
            i_temp++;
        }
    }
    return temp;
}

pixel * 
pad_remove (pixel * pixel_array, int width, int height, int * splits, int n_splits, int pad_size){
    pixel * temp = (pixel *)malloc(width * height * sizeof(pixel));
    int split_idx = 1; //cause the first point is 0
    int i_pad = 0;
    for (int i = 0; i < height; i++){
        if (split_idx < n_splits && i == splits[split_idx]){ //1 because of indexing
            i_pad += 2 * (pad_size) * width;
            split_idx++;
        }
        for (int j = 0; j < width; j++){
            temp[(i * width) + j] = pixel_array[i_pad];
            i_pad++;
        }
    }
    return temp;
}

/*
pixel *
pad_remove_local (pixel * pixel_array, int width, int height, int pad_size, int rank, int cluster){
    //this is an option but its a bit troublesome to implement cause you need to update gather
    int n_pixels;
    if (rank != 0 && rank != cluster - 1){
        n_pixels = height - (2 * pad_size);
        n_pixels *= width;
    } else {
        n_pixels = height - (pad_size);
        n_pixels *= width;
    }

    pixel * temp = (pixel *)malloc(n_pixels * sizeof(pixel));
    for (int i = 0; i < n_pixels; i++){
        if (rank == cluster - 1){
            temp[i] = pixel_array[i];
        } else {
            temp[i] = pixel_array[i + (pad_size * width)];
        }
    }
    return temp;
} */

/*
 * Main entry point
 */
int 
main( int argc, char ** argv )
{
    char * input_filename ; 
    char * output_filename ;
    animated_gif * image ;
    struct timeval t1, t2, t3, t4;
    double duration ;

//Init MPI/OMP
    int process_Rank, size_of_Cluster;
    int threads;

//for sharing info over mpi;
    pixel ** send_pixels, * for_free;
    animated_gif * img_temp;
    int num_images, offset, split_offset;
    int img_width, img_height;
    int ** original_split, ** displacement, ** scounts;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size_of_Cluster);
    MPI_Comm_rank(MPI_COMM_WORLD, &process_Rank);

//OMP getting number of threads
    #pragma omp parallel  
    {
    #pragma omp master
    { 
       //this is just the first thread operating with respect to the local mpi process
        threads = omp_get_num_threads();
    }
    }

/*Creating an MPI Datatype*/
    MPI_Datatype Pixel_MPI;
    int lengths[3] = {1, 1, 1};
    
    MPI_Aint displacements[3];
    struct pixel dummy_pixel;
    MPI_Aint base_address;
    MPI_Get_address(&dummy_pixel, &base_address);
    MPI_Get_address(&dummy_pixel.r, &displacements[0]);
    MPI_Get_address(&dummy_pixel.g, &displacements[1]);
    MPI_Get_address(&dummy_pixel.b, &displacements[2]);
    displacements[0] = MPI_Aint_diff(displacements[0], base_address);
    displacements[1] = MPI_Aint_diff(displacements[1], base_address);
    displacements[2] = MPI_Aint_diff(displacements[2], base_address);

    MPI_Datatype types[3] = {MPI_INT, MPI_INT, MPI_INT};
    MPI_Type_create_struct(3, lengths, displacements, types, &Pixel_MPI);
    MPI_Type_commit(&Pixel_MPI);

/* Check command-line arguments */
    if ( argc < 3 ){
        MPI_Finalize();
        fprintf( stderr, "Usage: %s input.gif output.gif \n", argv[0] ) ;
        return 1 ;
    }

    int padding = 6; //for the splitting later on
    image = (animated_gif *)malloc(sizeof(animated_gif)); 
//only the first process reads the file
    if (process_Rank == 0){ 
        input_filename = argv[1] ;
        output_filename = argv[2] ;

        /* IMPORT Timer start */
        gettimeofday(&t1, NULL);

        /* Load file and store the pixels in array */
        image = load_pixels( input_filename ) ;
        if ( image == NULL ) { return 1 ; }

        /* IMPORT Timer stop */
        gettimeofday(&t2, NULL);

        duration = (t2.tv_sec -t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);

        printf( "GIF loaded from file %s with %d image(s) of dim w x h (%d, %d) in %lf s\n", 
                input_filename, image->n_images, image->width[0], image->height[0], duration ) ;

    }
        
//pre-processing image before sharing
    if (process_Rank == 0){ 
        gettimeofday(&t1, NULL);
        num_images = image -> n_images;
    }

//setting up the gif to store data for each node
    MPI_Bcast(&num_images, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    img_temp = (animated_gif *)malloc(sizeof(animated_gif));

    img_temp -> n_images = num_images; 
    img_temp -> p = (pixel **)malloc(num_images * sizeof(pixel *));
    img_temp -> width = (int *)malloc(num_images * sizeof(int));
    img_temp -> height = (int *)malloc(num_images * sizeof(int));

    send_pixels = (pixel **)malloc(num_images * sizeof(pixel *));

    displacement = (int **)malloc(num_images * sizeof(int *));
    scounts = (int **)malloc(num_images * sizeof(int *));
    original_split = (int **)malloc(num_images * sizeof(int *));
//
    if (process_Rank == 0){ 
        gettimeofday(&t3, NULL);
    }
    
    for (int i = 0; i < num_images; i++){
    //setting split points
        displacement[i] = (int *)malloc(size_of_Cluster * sizeof(int));
        scounts[i] = (int *)malloc(size_of_Cluster * sizeof(int));
        original_split[i] = (int *)malloc(size_of_Cluster * sizeof(int));

        if (process_Rank == 0){
            img_temp -> width[i] = image -> width[i];
            img_temp -> height[i] = image -> height[i];
        }
        MPI_Bcast(&img_temp -> width[i], 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&img_temp -> height[i], 1, MPI_INT, 0, MPI_COMM_WORLD);
    }

    //#pragma omp parallel for //omp doesnt give any advantage here, in fact its worse
    for (int i = 0; i < num_images; i++){
        img_width = img_temp -> width[i];
        img_height = img_temp -> height[i];

        int sub_width = img_width/size_of_Cluster;
        int extra = img_width % size_of_Cluster;

        img_temp -> width[i] = sub_width;
        img_temp -> height[i] = img_height;
        if (process_Rank < extra){
            img_temp -> width[i] += 1;
        }
        if (process_Rank > 0){
            img_temp -> width[i] += padding;
        }
        if (process_Rank < size_of_Cluster - 1){
            img_temp -> width[i] += padding;
        }

        //this should be omp-able
        for (int j = 0; j < size_of_Cluster; j++){
            if (j < extra){
                original_split[i][j] = j * (sub_width + 1);
                displacement[i][j] = j * (sub_width + 1) * img_height;
                scounts[i][j] = (sub_width + 1) * img_height;
            } else {
                original_split[i][j] = (j * sub_width) + extra;
                displacement[i][j] = ((j * sub_width) + extra) * img_height;
                scounts[i][j] = sub_width * img_height;
            }
            if (j != 0){
                displacement[i][j] += padding * (j * 2 - 1) * img_height;
                scounts[i][j] += padding * img_height;
            }
            if (j != size_of_Cluster - 1){
                scounts[i][j] += padding * img_height;
            }
        }
    }
    if (process_Rank == 0){
        gettimeofday(&t4, NULL);
        duration = (t4.tv_sec - t3.tv_sec)+((t4.tv_usec-t3.tv_usec)/1e6);
        printf("pre-processing setup in %lf s\n", duration);
    }
    
    
    if (process_Rank == 0){
        gettimeofday(&t3, NULL);
        //#pragma omp parallel for private(for_free) //not sure if this does anything
        for (int i = 0; i < num_images; i++){
            for_free = transpose(image -> p[i], image -> width[i], image -> height[i]);
            send_pixels[i] = pad_frame(for_free, image->height[i], image -> width[i], original_split[i], size_of_Cluster, padding); //the implementation here is a bit weird
            
            free(image -> p[i]);
            free(for_free);
        }
        gettimeofday(&t4, NULL);
        duration = (t4.tv_sec - t3.tv_sec)+((t4.tv_usec-t3.tv_usec)/1e6);
        printf("pre-processing (before sending) in %lf s\n", duration);
    
    }
    if (process_Rank == 0){
        gettimeofday(&t3, NULL);
    }
    for (int i = 0; i < num_images; i++){
        for_free = (pixel *)malloc(scounts[i][process_Rank] * sizeof(pixel));

        MPI_Scatterv(send_pixels[i], scounts[i], displacement[i], Pixel_MPI, for_free, scounts[i][process_Rank], Pixel_MPI, 0, MPI_COMM_WORLD);
        //MPI_Barrier(MPI_COMM_WORLD); //just seeing if everything reaches here

        #pragma omp parallel //im not sure if this has any benefits
        {
            #pragma omp single
            img_temp -> p[i] = transpose(for_free, img_temp -> height[i], img_temp -> width[i]);
        }
        free(for_free);
    }

    if (process_Rank == 0){
        gettimeofday(&t4, NULL);
        duration = (t4.tv_sec - t3.tv_sec)+((t4.tv_usec-t3.tv_usec)/1e6);
        printf("gif shared in %lf s\n", duration);
 
        gettimeofday(&t2, NULL);
    
        duration = (t2.tv_sec -t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
    
        printf("Image processed and shared in %lf s\n", duration);
    }
    
    /* FILTER Timer start */
    gettimeofday(&t1, NULL);

    gettimeofday(&t3, NULL);
    /* Convert the pixels into grayscale */
    apply_gray_filter_gpu( img_temp ) ;
 
    gettimeofday(&t4, NULL);
    duration = (t4.tv_sec -t3.tv_sec)+((t4.tv_usec-t3.tv_usec)/1e6);
    
    printf("gray filter pre sobel in %lf s on machine  %d\n", duration, process_Rank);

    /* Apply blur filter with convergence value */
    apply_blur_filter_gpu( img_temp, 5, 20 ) ;
    
    gettimeofday(&t4, NULL);
    duration = (t4.tv_sec -t3.tv_sec)+((t4.tv_usec-t3.tv_usec)/1e6);
    
    printf("blur filter pre sobel in %lf s on machine %d\n", duration, process_Rank);

   
    /* Apply sobel filter on pixels */
    apply_sobel_filter_gpu( img_temp ) ;
    
    /* FILTER Timer stop */
    gettimeofday(&t2, NULL);

    duration = (t2.tv_sec -t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);

    printf( "SOBEL done in %lf s on machine %d\n", duration, process_Rank ) ;

    //getting time for gather on machine 1
    if (process_Rank == 0){
        gettimeofday(&t1, NULL);
    }

    for (int i = 0; i < num_images; i++){
        for_free = transpose(img_temp -> p[i], img_temp -> width[i], img_temp -> height[i]);
        free(img_temp -> p[i]);
        MPI_Gatherv(for_free, scounts[i][process_Rank], Pixel_MPI,
                    send_pixels[i], scounts[i], displacement[i], Pixel_MPI, 
                    0, MPI_COMM_WORLD);
        free(for_free);
        //MPI_Barrier(MPI_COMM_WORLD);
    }    

    if (process_Rank == 0){
        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec -t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        printf( "Gathered in %lf s\n", duration) ;
    }
    
     
    if (process_Rank == 0){

        gettimeofday(&t3, NULL);
        //#pragma omp parallel for private(for_free) //don't know if this is useful
        for (int i = 0; i < image -> n_images; i++){
    
            for_free = pad_remove(send_pixels[i], image -> height[i], image -> width[i], original_split[i], size_of_Cluster, padding);
   
            image -> p[i] = transpose(for_free, image -> height[i], image -> width[i]);
    
            free(send_pixels[i]);
            free(for_free);
        }
        gettimeofday(&t4, NULL);
        duration = (t4.tv_sec - t3.tv_sec)+((t4.tv_usec-t3.tv_usec)/1e6);
        printf("post-processed in %lf s\n", duration);
}

    if (process_Rank == 0){
        /* EXPORT Timer start */
        gettimeofday(&t1, NULL);

        /* Store file from array of pixels to GIF file */
        if ( !store_pixels( output_filename, image ) ) { return 1 ; }

        /* EXPORT Timer stop */
        gettimeofday(&t2, NULL);

        duration = (t2.tv_sec -t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);

        printf( "Export done in %lf s in file %s\n", duration, output_filename ) ;
    }
    
    for (int i = 0; i < num_images; i++){
        //free(img_temp -> p[i]);
        //free(send_pixels[i]); 
        free(original_split[i]);
        free(scounts[i]);
        free(displacement[i]);
    }
    free(img_temp -> p);
    free(img_temp -> width);
    free(img_temp -> height);
    free(send_pixels);
    
    free(displacement);
    free(scounts);
    free(original_split);
    free(img_temp);
    
    MPI_Finalize();

    return 0 ;
}
