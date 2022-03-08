/*
 * jdtrans.c
 *
 * Copyright (C) 1995-1997, Thomas G. Lane.
 * Modified 2000-2009 by Guido Vollbeding.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains library routines for transcoding decompression,
 * that is, reading raw DCT coefficient arrays from an input JPEG file.
 * The routines in jdapimin.c will also be needed by a transcoder.
 */

#define JPEG12_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"


/* Forward declarations */
LOCAL(void) transdecode_master_selection JPP((j12_decompress_ptr cinfo));


/*
 * Read the coefficient arrays from a JPEG file.
 * jpeg12_read_header must be completed before calling this.
 *
 * The entire image is read into a set of virtual coefficient-block arrays,
 * one per component.  The return value is a pointer to the array of
 * virtual-array descriptors.  These can be manipulated directly via the
 * JPEG memory manager, or handed off to jpeg12_write_coefficients().
 * To release the memory occupied by the virtual arrays, call
 * jpeg12_finish_decompress() when done with the data.
 *
 * An alternative usage is to simply obtain access to the coefficient arrays
 * during a buffered-image-mode decompression operation.  This is allowed
 * after any jpeg12_j12_finish_output() call.  The arrays can be accessed until
 * jpeg12_finish_decompress() is called.  (Note that any call to the library
 * may reposition the arrays, so don't rely on j12_access_virt_barray() results
 * to stay valid across library calls.)
 *
 * Returns NULL if suspended.  This case need be checked only if
 * a suspending data source is used.
 */

GLOBAL(jvirt_barray_ptr *)
jpeg12_read_coefficients (j12_decompress_ptr cinfo)
{
  if (cinfo->global_state == DSTATE_READY) {
    /* First call: initialize active modules */
    transdecode_master_selection(cinfo);
    cinfo->global_state = DSTATE_RDCOEFS;
  }
  if (cinfo->global_state == DSTATE_RDCOEFS) {
    /* Absorb whole file into the coef buffer */
    for (;;) {
      int retcode;
      /* Call progress monitor hook if present */
      if (cinfo->progress != NULL)
	(*cinfo->progress->j12_progress_monitor) ((j12_common_ptr) cinfo);
      /* Absorb some more input */
      retcode = (*cinfo->inputctl->j12_consume_input) (cinfo);
      if (retcode == JPEG12_SUSPENDED)
	return NULL;
      if (retcode == JPEG12_REACHED_EOI)
	break;
      /* Advance progress counter if appropriate */
      if (cinfo->progress != NULL &&
	  (retcode == JPEG12_ROW_COMPLETED || retcode == JPEG12_REACHED_SOS)) {
	if (++cinfo->progress->pass_counter >= cinfo->progress->pass_limit) {
	  /* startup underestimated number of scans; ratchet up one scan */
	  cinfo->progress->pass_limit += (long) cinfo->total_iMCU_rows;
	}
      }
    }
    /* Set state so that jpeg12_finish_decompress does the right thing */
    cinfo->global_state = DSTATE_STOPPING;
  }
  /* At this point we should be in state DSTATE_STOPPING if being used
   * standalone, or in state DSTATE_BUFIMAGE if being invoked to get access
   * to the coefficients during a full buffered-image-mode decompression.
   */
  if ((cinfo->global_state == DSTATE_STOPPING ||
       cinfo->global_state == DSTATE_BUFIMAGE) && cinfo->buffered_image) {
    return cinfo->coef->coef_arrays;
  }
  /* Oops, improper usage */
  ERREXIT1(cinfo, JERR_BAD_STATE, cinfo->global_state);
  return NULL;			/* keep compiler happy */
}


/*
 * Master selection of decompression modules for transcoding.
 * This substitutes for jdmaster.c's initialization of the full decompressor.
 */

LOCAL(void)
transdecode_master_selection (j12_decompress_ptr cinfo)
{
  /* This is effectively a buffered-image operation. */
  cinfo->buffered_image = TRUE;

  /* Compute output image dimensions and related values. */
  jpeg12_core_output_dimensions(cinfo);

  /* Entropy decoding: either Huffman or arithmetic coding. */
  if (cinfo->arith_code)
    j12_init_arith_decoder(cinfo);
  else {
    j12_init_huff_decoder(cinfo);
  }

  /* Always get a full-image coefficient buffer. */
  j12_init_d_coef_controller(cinfo, TRUE);

  /* We can now tell the memory manager to allocate virtual arrays. */
  (*cinfo->mem->j12_realize_virt_arrays) ((j12_common_ptr) cinfo);

  /* Initialize input side of decompressor to consume first scan. */
  (*cinfo->inputctl->j12_start_input_pass) (cinfo);

  /* Initialize progress monitoring. */
  if (cinfo->progress != NULL) {
    int nscans;
    /* Estimate number of scans to set pass_limit. */
    if (cinfo->progressive_mode) {
      /* Arbitrarily estimate 2 interleaved DC scans + 3 AC scans/component. */
      nscans = 2 + 3 * cinfo->num_components;
    } else if (cinfo->inputctl->has_multiple_scans) {
      /* For a nonprogressive multiscan file, estimate 1 scan per component. */
      nscans = cinfo->num_components;
    } else {
      nscans = 1;
    }
    cinfo->progress->pass_counter = 0L;
    cinfo->progress->pass_limit = (long) cinfo->total_iMCU_rows * nscans;
    cinfo->progress->completed_passes = 0;
    cinfo->progress->total_passes = 1;
  }
}
