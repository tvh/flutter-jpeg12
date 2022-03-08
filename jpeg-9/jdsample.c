/*
 * jdsample.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * Modified 2002-2008 by Guido Vollbeding.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains upsampling routines.
 *
 * Upsampling input data is counted in "row groups".  A row group
 * is defined to be (v_samp_factor * DCT_v_scaled_size / min_DCT_v_scaled_size)
 * sample rows of each component.  Upsampling will normally produce
 * max_v_samp_factor pixel rows from each row group (but this could vary
 * if the j12_upsampler is applying a scale factor of its own).
 *
 * An excellent reference for image resampling is
 *   Digital Image Warping, George Wolberg, 1990.
 *   Pub. by IEEE Computer Society Press, Los Alamitos, CA. ISBN 0-8186-8944-7.
 */

#define JPEG12_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"


/* Pointer to routine to j12_upsample a single component */
typedef JMETHOD(void, j12_upsample1_ptr,
		(j12_decompress_ptr cinfo, jpeg12_component_info * compptr,
		 JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));

/* Private subobject */

typedef struct {
  struct jpeg12_j12_upsampler pub;	/* public fields */

  /* Color conversion buffer.  When using separate upsampling and color
   * conversion steps, this buffer holds one j12_upsampled row group until it
   * has been color converted and output.
   * Note: we do not allocate any storage for component(s) which are full-size,
   * ie do not need rescaling.  The corresponding entry of color_buf[] is
   * simply set to point to the input data array, thereby avoiding copying.
   */
  JSAMPARRAY color_buf[MAX_COMPONENTS];

  /* Per-component upsampling method pointers */
  j12_upsample1_ptr methods[MAX_COMPONENTS];

  int next_row_out;		/* counts rows emitted from color_buf */
  JDIMENSION rows_to_go;	/* counts rows remaining in image */

  /* Height of an input row group for each component. */
  int rowgroup_height[MAX_COMPONENTS];

  /* These arrays save pixel expansion factors so that int_expand need not
   * recompute them each time.  They are unused for other upsampling methods.
   */
  UINT8 h_expand[MAX_COMPONENTS];
  UINT8 v_expand[MAX_COMPONENTS];
} my_j12_upsampler;

typedef my_j12_upsampler * my_j12_upsample_ptr;


/*
 * Initialize for an upsampling pass.
 */

METHODDEF(void)
j12_start_pass_j12_upsample (j12_decompress_ptr cinfo)
{
  my_j12_upsample_ptr j12_upsample = (my_j12_upsample_ptr) cinfo->j12_upsample;

  /* Mark the conversion buffer empty */
  j12_upsample->next_row_out = cinfo->max_v_samp_factor;
  /* Initialize total-height counter for detecting bottom of image */
  j12_upsample->rows_to_go = cinfo->output_height;
}


/*
 * Control routine to do upsampling (and color conversion).
 *
 * In this version we j12_upsample each component independently.
 * We j12_upsample one row group into the conversion buffer, then apply
 * color conversion a row at a time.
 */

METHODDEF(void)
sep_j12_upsample (j12_decompress_ptr cinfo,
	      JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	      JDIMENSION in_row_groups_avail,
	      JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	      JDIMENSION out_rows_avail)
{
  my_j12_upsample_ptr j12_upsample = (my_j12_upsample_ptr) cinfo->j12_upsample;
  int ci;
  jpeg12_component_info * compptr;
  JDIMENSION num_rows;

  /* Fill the conversion buffer, if it's empty */
  if (j12_upsample->next_row_out >= cinfo->max_v_samp_factor) {
    for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
	 ci++, compptr++) {
      /* Invoke per-component j12_upsample method.  Notice we pass a POINTER
       * to color_buf[ci], so that fullsize_j12_upsample can change it.
       */
      (*j12_upsample->methods[ci]) (cinfo, compptr,
	input_buf[ci] + (*in_row_group_ctr * j12_upsample->rowgroup_height[ci]),
	j12_upsample->color_buf + ci);
    }
    j12_upsample->next_row_out = 0;
  }

  /* Color-convert and emit rows */

  /* How many we have in the buffer: */
  num_rows = (JDIMENSION) (cinfo->max_v_samp_factor - j12_upsample->next_row_out);
  /* Not more than the distance to the end of the image.  Need this test
   * in case the image height is not a multiple of max_v_samp_factor:
   */
  if (num_rows > j12_upsample->rows_to_go) 
    num_rows = j12_upsample->rows_to_go;
  /* And not more than what the client can accept: */
  out_rows_avail -= *out_row_ctr;
  if (num_rows > out_rows_avail)
    num_rows = out_rows_avail;

  (*cinfo->cconvert->j12_color_convert) (cinfo, j12_upsample->color_buf,
				     (JDIMENSION) j12_upsample->next_row_out,
				     output_buf + *out_row_ctr,
				     (int) num_rows);

  /* Adjust counts */
  *out_row_ctr += num_rows;
  j12_upsample->rows_to_go -= num_rows;
  j12_upsample->next_row_out += num_rows;
  /* When the buffer is emptied, declare this input row group consumed */
  if (j12_upsample->next_row_out >= cinfo->max_v_samp_factor)
    (*in_row_group_ctr)++;
}


/*
 * These are the routines invoked by sep_j12_upsample to j12_upsample pixel values
 * of a single component.  One row group is processed per call.
 */


/*
 * For full-size components, we just make color_buf[ci] point at the
 * input buffer, and thus avoid copying any data.  Note that this is
 * safe only because sep_j12_upsample doesn't declare the input row group
 * "consumed" until we are done color converting and emitting it.
 */

METHODDEF(void)
fullsize_j12_upsample (j12_decompress_ptr cinfo, jpeg12_component_info * compptr,
		   JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr)
{
  *output_data_ptr = input_data;
}


/*
 * This is a no-op version used for "uninteresting" components.
 * These components will not be referenced by color conversion.
 */

METHODDEF(void)
noop_j12_upsample (j12_decompress_ptr cinfo, jpeg12_component_info * compptr,
	       JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr)
{
  *output_data_ptr = NULL;	/* safety check */
}


/*
 * This version handles any integral sampling ratios.
 * This is not used for typical JPEG files, so it need not be fast.
 * Nor, for that matter, is it particularly accurate: the algorithm is
 * simple replication of the input pixel onto the corresponding output
 * pixels.  The hi-falutin sampling literature refers to this as a
 * "box filter".  A box filter tends to introduce visible artifacts,
 * so if you are actually going to use 3:1 or 4:1 sampling ratios
 * you would be well advised to improve this code.
 */

METHODDEF(void)
int_j12_upsample (j12_decompress_ptr cinfo, jpeg12_component_info * compptr,
	      JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr)
{
  my_j12_upsample_ptr j12_upsample = (my_j12_upsample_ptr) cinfo->j12_upsample;
  JSAMPARRAY output_data = *output_data_ptr;
  register JSAMPROW inptr, outptr;
  register JSAMPLE invalue;
  register int h;
  JSAMPROW outend;
  int h_expand, v_expand;
  int inrow, outrow;

  h_expand = j12_upsample->h_expand[compptr->component_index];
  v_expand = j12_upsample->v_expand[compptr->component_index];

  inrow = outrow = 0;
  while (outrow < cinfo->max_v_samp_factor) {
    /* Generate one output row with proper horizontal expansion */
    inptr = input_data[inrow];
    outptr = output_data[outrow];
    outend = outptr + cinfo->output_width;
    while (outptr < outend) {
      invalue = *inptr++;	/* don't need GETJSAMPLE() here */
      for (h = h_expand; h > 0; h--) {
	*outptr++ = invalue;
      }
    }
    /* Generate any additional output rows by duplicating the first one */
    if (v_expand > 1) {
      j12_copy_sample_rows(output_data, outrow, output_data, outrow+1,
			v_expand-1, cinfo->output_width);
    }
    inrow++;
    outrow += v_expand;
  }
}


/*
 * Fast processing for the common case of 2:1 horizontal and 1:1 vertical.
 * It's still a box filter.
 */

METHODDEF(void)
h2v1_j12_upsample (j12_decompress_ptr cinfo, jpeg12_component_info * compptr,
	       JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  register JSAMPROW inptr, outptr;
  register JSAMPLE invalue;
  JSAMPROW outend;
  int outrow;

  for (outrow = 0; outrow < cinfo->max_v_samp_factor; outrow++) {
    inptr = input_data[outrow];
    outptr = output_data[outrow];
    outend = outptr + cinfo->output_width;
    while (outptr < outend) {
      invalue = *inptr++;	/* don't need GETJSAMPLE() here */
      *outptr++ = invalue;
      *outptr++ = invalue;
    }
  }
}


/*
 * Fast processing for the common case of 2:1 horizontal and 2:1 vertical.
 * It's still a box filter.
 */

METHODDEF(void)
h2v2_j12_upsample (j12_decompress_ptr cinfo, jpeg12_component_info * compptr,
	       JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr)
{
  JSAMPARRAY output_data = *output_data_ptr;
  register JSAMPROW inptr, outptr;
  register JSAMPLE invalue;
  JSAMPROW outend;
  int inrow, outrow;

  inrow = outrow = 0;
  while (outrow < cinfo->max_v_samp_factor) {
    inptr = input_data[inrow];
    outptr = output_data[outrow];
    outend = outptr + cinfo->output_width;
    while (outptr < outend) {
      invalue = *inptr++;	/* don't need GETJSAMPLE() here */
      *outptr++ = invalue;
      *outptr++ = invalue;
    }
    j12_copy_sample_rows(output_data, outrow, output_data, outrow+1,
		      1, cinfo->output_width);
    inrow++;
    outrow += 2;
  }
}


/*
 * Module initialization routine for upsampling.
 */

GLOBAL(void)
jinit_j12_upsampler (j12_decompress_ptr cinfo)
{
  my_j12_upsample_ptr j12_upsample;
  int ci;
  jpeg12_component_info * compptr;
  boolean need_buffer;
  int h_in_group, v_in_group, h_out_group, v_out_group;

  j12_upsample = (my_j12_upsample_ptr)
    (*cinfo->mem->j12_alloc_small) ((j12_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF(my_j12_upsampler));
  cinfo->j12_upsample = (struct jpeg12_j12_upsampler *) j12_upsample;
  j12_upsample->pub.j12_start_pass = j12_start_pass_j12_upsample;
  j12_upsample->pub.j12_upsample = sep_j12_upsample;
  j12_upsample->pub.need_context_rows = FALSE; /* until we find out differently */

  if (cinfo->CCIR601_sampling)	/* this isn't supported */
    ERREXIT(cinfo, JERR_CCIR601_NOTIMPL);

  /* Verify we can handle the sampling factors, select per-component methods,
   * and create storage as needed.
   */
  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    /* Compute size of an "input group" after IDCT scaling.  This many samples
     * are to be converted to max_h_samp_factor * max_v_samp_factor pixels.
     */
    h_in_group = (compptr->h_samp_factor * compptr->DCT_h_scaled_size) /
		 cinfo->min_DCT_h_scaled_size;
    v_in_group = (compptr->v_samp_factor * compptr->DCT_v_scaled_size) /
		 cinfo->min_DCT_v_scaled_size;
    h_out_group = cinfo->max_h_samp_factor;
    v_out_group = cinfo->max_v_samp_factor;
    j12_upsample->rowgroup_height[ci] = v_in_group; /* save for use later */
    need_buffer = TRUE;
    if (! compptr->component_needed) {
      /* Don't bother to j12_upsample an uninteresting component. */
      j12_upsample->methods[ci] = noop_j12_upsample;
      need_buffer = FALSE;
    } else if (h_in_group == h_out_group && v_in_group == v_out_group) {
      /* Fullsize components can be processed without any work. */
      j12_upsample->methods[ci] = fullsize_j12_upsample;
      need_buffer = FALSE;
    } else if (h_in_group * 2 == h_out_group &&
	       v_in_group == v_out_group) {
      /* Special case for 2h1v upsampling */
      j12_upsample->methods[ci] = h2v1_j12_upsample;
    } else if (h_in_group * 2 == h_out_group &&
	       v_in_group * 2 == v_out_group) {
      /* Special case for 2h2v upsampling */
      j12_upsample->methods[ci] = h2v2_j12_upsample;
    } else if ((h_out_group % h_in_group) == 0 &&
	       (v_out_group % v_in_group) == 0) {
      /* Generic integral-factors upsampling method */
      j12_upsample->methods[ci] = int_j12_upsample;
      j12_upsample->h_expand[ci] = (UINT8) (h_out_group / h_in_group);
      j12_upsample->v_expand[ci] = (UINT8) (v_out_group / v_in_group);
    } else
      ERREXIT(cinfo, JERR_FRACT_SAMPLE_NOTIMPL);
    if (need_buffer) {
      j12_upsample->color_buf[ci] = (*cinfo->mem->j12_alloc_sarray)
	((j12_common_ptr) cinfo, JPOOL_IMAGE,
	 (JDIMENSION) j12_round_up((long) cinfo->output_width,
				(long) cinfo->max_h_samp_factor),
	 (JDIMENSION) cinfo->max_v_samp_factor);
    }
  }
}
