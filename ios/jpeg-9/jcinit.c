/*
 * jcinit.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains initialization logic for the JPEG compressor.
 * This routine is in charge of selecting the modules to be executed and
 * making an initialization call to each one.
 *
 * Logically, this code belongs in jcmaster.c.  It's split out because
 * linking this routine implies linking the entire compression library.
 * For a transcoding-only application, we want to be able to use jcmaster.c
 * without linking in the whole library.
 */

#define JPEG12_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"


/*
 * Master selection of compression modules.
 * This is done once at the start of processing an image.  We determine
 * which modules will be used and give them appropriate initialization calls.
 */

GLOBAL(void)
j12_init_compress_master (j12_compress_ptr cinfo)
{
  /* Initialize master control (includes parameter checking/processing) */
  j12_init_c_master_control(cinfo, FALSE /* full compression */);

  /* Preprocessing */
  if (! cinfo->raw_data_in) {
    jinit_j12_color_converter(cinfo);
    jinit_j12_downsampler(cinfo);
    j12_init_c_prep_controller(cinfo, FALSE /* never need full buffer here */);
  }
  /* Forward DCT */
  j12_init_forward_dct(cinfo);
  /* Entropy encoding: either Huffman or arithmetic coding. */
  if (cinfo->arith_code)
    j12_init_arith_encoder(cinfo);
  else {
    j12_init_huff_encoder(cinfo);
  }

  /* Need a full-image coefficient buffer in any multi-pass mode. */
  j12_init_c_coef_controller(cinfo,
		(boolean) (cinfo->num_scans > 1 || cinfo->optimize_coding));
  j12_init_c_main_controller(cinfo, FALSE /* never need full buffer here */);

  j12_init_marker_writer(cinfo);

  /* We can now tell the memory manager to allocate virtual arrays. */
  (*cinfo->mem->j12_realize_virt_arrays) ((j12_common_ptr) cinfo);

  /* Write the datastream header (SOI) immediately.
   * Frame and scan headers are postponed till later.
   * This lets application insert special markers after the SOI.
   */
  (*cinfo->marker->j12_write_file_header) (cinfo);
}
