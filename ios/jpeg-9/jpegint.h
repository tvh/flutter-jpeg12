/*
 * jpegint.h
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * Modified 1997-2011 by Guido Vollbeding.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file provides common declarations for the various JPEG modules.
 * These declarations are considered internal to the JPEG library; most
 * applications using the library shouldn't need to include this file.
 */


/* Declarations for both compression & decompression */

typedef enum {			/* Operating modes for buffer controllers */
	JBUF_PASS_THRU,		/* Plain stripwise operation */
	/* Remaining modes require a full-image buffer to have been created */
	JBUF_SAVE_SOURCE,	/* Run source subobject only, save output */
	JBUF_CRANK_DEST,	/* Run dest subobject only, using saved data */
	JBUF_SAVE_AND_PASS	/* Run both subobjects, save output */
} J_BUF_MODE;

/* Values of global_state field (jdapi.c has some dependencies on ordering!) */
#define CSTATE_START	100	/* after create_compress */
#define CSTATE_SCANNING	101	/* start_compress done, write_scanlines OK */
#define CSTATE_RAW_OK	102	/* start_compress done, write_raw_data OK */
#define CSTATE_WRCOEFS	103	/* jpeg12_write_coefficients done */
#define DSTATE_START	200	/* after create_decompress */
#define DSTATE_INHEADER	201	/* reading header markers, no SOS yet */
#define DSTATE_READY	202	/* found SOS, ready for start_decompress */
#define DSTATE_PRELOAD	203	/* reading multiscan file in start_decompress*/
#define DSTATE_PRESCAN	204	/* performing dummy pass for 2-pass quant */
#define DSTATE_SCANNING	205	/* start_decompress done, read_scanlines OK */
#define DSTATE_RAW_OK	206	/* start_decompress done, read_raw_data OK */
#define DSTATE_BUFIMAGE	207	/* expecting jpeg12_j12_start_output */
#define DSTATE_BUFPOST	208	/* looking for SOS/EOI in jpeg12_j12_finish_output */
#define DSTATE_RDCOEFS	209	/* reading file in jpeg12_read_coefficients */
#define DSTATE_STOPPING	210	/* looking for EOI in jpeg12_finish_decompress */


/* Declarations for compression modules */

/* Master control module */
struct jpeg12_comp_master {
  JMETHOD(void, j12_prepare_for_pass, (j12_compress_ptr cinfo));
  JMETHOD(void, j12_pass_startup, (j12_compress_ptr cinfo));
  JMETHOD(void, j12_finish_pass, (j12_compress_ptr cinfo));

  /* State variables made visible to other modules */
  boolean call_j12_pass_startup;	/* True if j12_pass_startup must be called */
  boolean is_last_pass;		/* True during last pass */
};

/* Main buffer control (j12_downsampled-data buffer) */
struct jpeg12_c_main_controller {
  JMETHOD(void, j12_start_pass, (j12_compress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(void, j12_process_data, (j12_compress_ptr cinfo,
			       JSAMPARRAY input_buf, JDIMENSION *in_row_ctr,
			       JDIMENSION in_rows_avail));
};

/* Compression preprocessing (downsampling input buffer control) */
struct jpeg12_c_prep_controller {
  JMETHOD(void, j12_start_pass, (j12_compress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(void, pre_j12_process_data, (j12_compress_ptr cinfo,
				   JSAMPARRAY input_buf,
				   JDIMENSION *in_row_ctr,
				   JDIMENSION in_rows_avail,
				   JSAMPIMAGE output_buf,
				   JDIMENSION *out_row_group_ctr,
				   JDIMENSION out_row_groups_avail));
};

/* Coefficient buffer control */
struct jpeg12_c_coef_controller {
  JMETHOD(void, j12_start_pass, (j12_compress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(boolean, j12_compress_data, (j12_compress_ptr cinfo,
				   JSAMPIMAGE input_buf));
};

/* Colorspace conversion */
struct jpeg12_j12_color_converter {
  JMETHOD(void, j12_start_pass, (j12_compress_ptr cinfo));
  JMETHOD(void, j12_color_convert, (j12_compress_ptr cinfo,
				JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
				JDIMENSION output_row, int num_rows));
};

/* Downsampling */
struct jpeg12_j12_downsampler {
  JMETHOD(void, j12_start_pass, (j12_compress_ptr cinfo));
  JMETHOD(void, j12_downsample, (j12_compress_ptr cinfo,
			     JSAMPIMAGE input_buf, JDIMENSION in_row_index,
			     JSAMPIMAGE output_buf,
			     JDIMENSION out_row_group_index));

  boolean need_context_rows;	/* TRUE if need rows above & below */
};

/* Forward DCT (also controls coefficient quantization) */
typedef JMETHOD(void, forward_DCT_ptr,
		(j12_compress_ptr cinfo, jpeg12_component_info * compptr,
		 JSAMPARRAY sample_data, JBLOCKROW coef_blocks,
		 JDIMENSION start_row, JDIMENSION start_col,
		 JDIMENSION num_blocks));

struct jpeg12_forward_dct {
  JMETHOD(void, j12_start_pass, (j12_compress_ptr cinfo));
  /* It is useful to allow each component to have a separate FDCT method. */
  forward_DCT_ptr forward_DCT[MAX_COMPONENTS];
};

/* Entropy encoding */
struct jpeg12_entropy_encoder {
  JMETHOD(void, j12_start_pass, (j12_compress_ptr cinfo, boolean gather_statistics));
  JMETHOD(boolean, j12_encode_mcu, (j12_compress_ptr cinfo, JBLOCKROW *MCU_data));
  JMETHOD(void, j12_finish_pass, (j12_compress_ptr cinfo));
};

/* Marker writing */
struct jpeg12_marker_writer {
  JMETHOD(void, j12_write_file_header, (j12_compress_ptr cinfo));
  JMETHOD(void, j12_write_frame_header, (j12_compress_ptr cinfo));
  JMETHOD(void, j12_write_scan_header, (j12_compress_ptr cinfo));
  JMETHOD(void, j12_write_file_trailer, (j12_compress_ptr cinfo));
  JMETHOD(void, j12_write_tables_only, (j12_compress_ptr cinfo));
  /* These routines are exported to allow insertion of extra markers */
  /* Probably only COM and APPn markers should be written this way */
  JMETHOD(void, j12_write_marker_header, (j12_compress_ptr cinfo, int marker,
				      unsigned int datalen));
  JMETHOD(void, j12_write_marker_byte, (j12_compress_ptr cinfo, int val));
};


/* Declarations for decompression modules */

/* Master control module */
struct jpeg12_decomp_master {
  JMETHOD(void, j12_prepare_for_output_pass, (j12_decompress_ptr cinfo));
  JMETHOD(void, j12_finish_output_pass, (j12_decompress_ptr cinfo));

  /* State variables made visible to other modules */
  boolean is_dummy_pass;	/* True during 1st pass for 2-pass quant */
};

/* Input control module */
struct jpeg12_input_controller {
  JMETHOD(int, j12_consume_input, (j12_decompress_ptr cinfo));
  JMETHOD(void, j12_reset_input_controller, (j12_decompress_ptr cinfo));
  JMETHOD(void, j12_start_input_pass, (j12_decompress_ptr cinfo));
  JMETHOD(void, j12_finish_input_pass, (j12_decompress_ptr cinfo));

  /* State variables made visible to other modules */
  boolean has_multiple_scans;	/* True if file has multiple scans */
  boolean eoi_reached;		/* True when EOI has been consumed */
};

/* Main buffer control (j12_downsampled-data buffer) */
struct jpeg12_d_main_controller {
  JMETHOD(void, j12_start_pass, (j12_decompress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(void, j12_process_data, (j12_decompress_ptr cinfo,
			       JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
			       JDIMENSION out_rows_avail));
};

/* Coefficient buffer control */
struct jpeg12_d_coef_controller {
  JMETHOD(void, j12_start_input_pass, (j12_decompress_ptr cinfo));
  JMETHOD(int, j12_consume_data, (j12_decompress_ptr cinfo));
  JMETHOD(void, j12_start_output_pass, (j12_decompress_ptr cinfo));
  JMETHOD(int, dej12_compress_data, (j12_decompress_ptr cinfo,
				 JSAMPIMAGE output_buf));
  /* Pointer to array of coefficient virtual arrays, or NULL if none */
  jvirt_barray_ptr *coef_arrays;
};

/* Decompression postprocessing (color quantization buffer control) */
struct jpeg12_d_post_controller {
  JMETHOD(void, j12_start_pass, (j12_decompress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(void, post_j12_process_data, (j12_decompress_ptr cinfo,
				    JSAMPIMAGE input_buf,
				    JDIMENSION *in_row_group_ctr,
				    JDIMENSION in_row_groups_avail,
				    JSAMPARRAY output_buf,
				    JDIMENSION *out_row_ctr,
				    JDIMENSION out_rows_avail));
};

/* Marker reading & parsing */
struct jpeg12_marker_reader {
  JMETHOD(void, j12_reset_marker_reader, (j12_decompress_ptr cinfo));
  /* Read markers until SOS or EOI.
   * Returns same codes as are defined for jpeg12_j12_consume_input:
   * JPEG12_SUSPENDED, JPEG12_REACHED_SOS, or JPEG12_REACHED_EOI.
   */
  JMETHOD(int, j12_read_markers, (j12_decompress_ptr cinfo));
  /* Read a restart marker --- exported for use by entropy decoder only */
  jpeg12_marker_parser_method read_restart_marker;

  /* State of marker reader --- nominally internal, but applications
   * supplying COM or APPn handlers might like to know the state.
   */
  boolean saw_SOI;		/* found SOI? */
  boolean saw_SOF;		/* found SOF? */
  int next_restart_num;		/* next restart number expected (0-7) */
  unsigned int discarded_bytes;	/* # of bytes skipped looking for a marker */
};

/* Entropy decoding */
struct jpeg12_entropy_decoder {
  JMETHOD(void, j12_start_pass, (j12_decompress_ptr cinfo));
  JMETHOD(boolean, j12_decode_mcu, (j12_decompress_ptr cinfo,
				JBLOCKROW *MCU_data));
};

/* Inverse DCT (also performs dequantization) */
typedef JMETHOD(void, inverse_DCT_method_ptr,
		(j12_decompress_ptr cinfo, jpeg12_component_info * compptr,
		 JCOEFPTR coef_block,
		 JSAMPARRAY output_buf, JDIMENSION output_col));

struct jpeg12_inverse_dct {
  JMETHOD(void, j12_start_pass, (j12_decompress_ptr cinfo));
  /* It is useful to allow each component to have a separate IDCT method. */
  inverse_DCT_method_ptr inverse_DCT[MAX_COMPONENTS];
};

/* Upsampling (note that j12_upsampler must also call color converter) */
struct jpeg12_j12_upsampler {
  JMETHOD(void, j12_start_pass, (j12_decompress_ptr cinfo));
  JMETHOD(void, j12_upsample, (j12_decompress_ptr cinfo,
			   JSAMPIMAGE input_buf,
			   JDIMENSION *in_row_group_ctr,
			   JDIMENSION in_row_groups_avail,
			   JSAMPARRAY output_buf,
			   JDIMENSION *out_row_ctr,
			   JDIMENSION out_rows_avail));

  boolean need_context_rows;	/* TRUE if need rows above & below */
};

/* Colorspace conversion */
struct jpeg12_color_deconverter {
  JMETHOD(void, j12_start_pass, (j12_decompress_ptr cinfo));
  JMETHOD(void, j12_color_convert, (j12_decompress_ptr cinfo,
				JSAMPIMAGE input_buf, JDIMENSION input_row,
				JSAMPARRAY output_buf, int num_rows));
};

/* Color quantization or color precision reduction */
struct jpeg12_j12_color_quantizer {
  JMETHOD(void, j12_start_pass, (j12_decompress_ptr cinfo, boolean is_pre_scan));
  JMETHOD(void, j12_color_quantize, (j12_decompress_ptr cinfo,
				 JSAMPARRAY input_buf, JSAMPARRAY output_buf,
				 int num_rows));
  JMETHOD(void, j12_finish_pass, (j12_decompress_ptr cinfo));
  JMETHOD(void, j12_new_color_map, (j12_decompress_ptr cinfo));
};


/* Miscellaneous useful macros */

#undef MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#undef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))


/* We assume that right shift corresponds to signed division by 2 with
 * rounding towards minus infinity.  This is correct for typical "arithmetic
 * shift" instructions that shift in copies of the sign bit.  But some
 * C compilers implement >> with an unsigned shift.  For these machines you
 * must define RIGHT_SHIFT_IS_UNSIGNED.
 * RIGHT_SHIFT provides a proper signed right shift of an INT32 quantity.
 * It is only applied with constant shift counts.  SHIFT_TEMPS must be
 * included in the variables of any routine using RIGHT_SHIFT.
 */

#ifdef RIGHT_SHIFT_IS_UNSIGNED
#define SHIFT_TEMPS	INT32 shift_temp;
#define RIGHT_SHIFT(x,shft)  \
	((shift_temp = (x)) < 0 ? \
	 (shift_temp >> (shft)) | ((~((INT32) 0)) << (32-(shft))) : \
	 (shift_temp >> (shft)))
#else
#define SHIFT_TEMPS
#define RIGHT_SHIFT(x,shft)	((x) >> (shft))
#endif


/* Short forms of external names for systems with brain-damaged linkers. */

#ifdef NEED_SHORT_EXTERNAL_NAMES
#define j12_init_compress_master	jICompress
#define j12_init_c_master_control	jICMaster
#define j12_init_c_main_controller	jICMainC
#define j12_init_c_prep_controller	jICPrepC
#define j12_init_c_coef_controller	jICCoefC
#define jinit_j12_color_converter	jICColor
#define jinit_j12_downsampler	jIDownsampler
#define j12_init_forward_dct	jIFDCT
#define j12_init_huff_encoder	jIHEncoder
#define j12_init_arith_encoder	jIAEncoder
#define j12_init_marker_writer	jIMWriter
#define j12_init_master_decompress	jIDMaster
#define j12_init_d_main_controller	jIDMainC
#define j12_init_d_coef_controller	jIDCoefC
#define j12_init_d_post_controller	jIDPostC
#define j12_init_input_controller	jIInCtlr
#define j12_init_marker_reader	jIMReader
#define j12_init_huff_decoder	jIHDecoder
#define j12_init_arith_decoder	jIADecoder
#define j12_init_inverse_dct	jIIDCT
#define jinit_j12_upsampler		jIUpsampler
#define j12_init_color_deconverter	jIDColor
#define j12_init_1pass_quantizer	jI1Quant
#define j12_init_2pass_quantizer	jI2Quant
#define jinit_merged_j12_upsampler	jIMUpsampler
#define j12_init_memory_mgr	jIMemMgr
#define j12_div_round_up		jDivRound
#define j12_round_up		jRound
#define jzero_far		jZeroFar
#define j12_copy_sample_rows	jCopySamples
#define j12_copy_block_row		jCopyBlocks
#define jpeg12_zigzag_order	jZIGTable
#define jpeg12_natural_order	jZAGTable
#define jpeg12_natural_order7	jZAG7Table
#define jpeg12_natural_order6	jZAG6Table
#define jpeg12_natural_order5	jZAG5Table
#define jpeg12_natural_order4	jZAG4Table
#define jpeg12_natural_order3	jZAG3Table
#define jpeg12_natural_order2	jZAG2Table
#define jpeg12_aritab		jAriTab
#endif /* NEED_SHORT_EXTERNAL_NAMES */


/* On normal machines we can apply MEMCOPY() and MEMZERO() to sample arrays
 * and coefficient-block arrays.  This won't work on 80x86 because the arrays
 * are FAR and we're assuming a small-pointer memory model.  However, some
 * DOS compilers provide far-pointer versions of memcpy() and memset() even
 * in the small-model libraries.  These will be used if USE_FMEM is defined.
 * Otherwise, the routines in jutils.c do it the hard way.
 */

#ifndef NEED_FAR_POINTERS	/* normal case, same as regular macro */
#define FMEMZERO(target,size)	MEMZERO(target,size)
#else				/* 80x86 case */
#ifdef USE_FMEM
#define FMEMZERO(target,size)	_fmemset((void FAR *)(target), 0, (size_t)(size))
#else
EXTERN(void) jzero_far JPP((void FAR * target, size_t bytestozero));
#define FMEMZERO(target,size)	jzero_far(target, size)
#endif
#endif


/* Compression module initialization routines */
EXTERN(void) j12_init_compress_master JPP((j12_compress_ptr cinfo));
EXTERN(void) j12_init_c_master_control JPP((j12_compress_ptr cinfo,
					 boolean transcode_only));
EXTERN(void) j12_init_c_main_controller JPP((j12_compress_ptr cinfo,
					  boolean need_full_buffer));
EXTERN(void) j12_init_c_prep_controller JPP((j12_compress_ptr cinfo,
					  boolean need_full_buffer));
EXTERN(void) j12_init_c_coef_controller JPP((j12_compress_ptr cinfo,
					  boolean need_full_buffer));
EXTERN(void) jinit_j12_color_converter JPP((j12_compress_ptr cinfo));
EXTERN(void) jinit_j12_downsampler JPP((j12_compress_ptr cinfo));
EXTERN(void) j12_init_forward_dct JPP((j12_compress_ptr cinfo));
EXTERN(void) j12_init_huff_encoder JPP((j12_compress_ptr cinfo));
EXTERN(void) j12_init_arith_encoder JPP((j12_compress_ptr cinfo));
EXTERN(void) j12_init_marker_writer JPP((j12_compress_ptr cinfo));
/* Decompression module initialization routines */
EXTERN(void) j12_init_master_decompress JPP((j12_decompress_ptr cinfo));
EXTERN(void) j12_init_d_main_controller JPP((j12_decompress_ptr cinfo,
					  boolean need_full_buffer));
EXTERN(void) j12_init_d_coef_controller JPP((j12_decompress_ptr cinfo,
					  boolean need_full_buffer));
EXTERN(void) j12_init_d_post_controller JPP((j12_decompress_ptr cinfo,
					  boolean need_full_buffer));
EXTERN(void) j12_init_input_controller JPP((j12_decompress_ptr cinfo));
EXTERN(void) j12_init_marker_reader JPP((j12_decompress_ptr cinfo));
EXTERN(void) j12_init_huff_decoder JPP((j12_decompress_ptr cinfo));
EXTERN(void) j12_init_arith_decoder JPP((j12_decompress_ptr cinfo));
EXTERN(void) j12_init_inverse_dct JPP((j12_decompress_ptr cinfo));
EXTERN(void) jinit_j12_upsampler JPP((j12_decompress_ptr cinfo));
EXTERN(void) j12_init_color_deconverter JPP((j12_decompress_ptr cinfo));
EXTERN(void) j12_init_1pass_quantizer JPP((j12_decompress_ptr cinfo));
EXTERN(void) j12_init_2pass_quantizer JPP((j12_decompress_ptr cinfo));
EXTERN(void) jinit_merged_j12_upsampler JPP((j12_decompress_ptr cinfo));
/* Memory manager initialization */
EXTERN(void) j12_init_memory_mgr JPP((j12_common_ptr cinfo));

/* Utility routines in jutils.c */
EXTERN(long) j12_div_round_up JPP((long a, long b));
EXTERN(long) j12_round_up JPP((long a, long b));
EXTERN(void) j12_copy_sample_rows JPP((JSAMPARRAY input_array, int source_row,
				    JSAMPARRAY output_array, int dest_row,
				    int num_rows, JDIMENSION num_cols));
EXTERN(void) j12_copy_block_row JPP((JBLOCKROW input_row, JBLOCKROW output_row,
				  JDIMENSION num_blocks));
/* Constant tables in jutils.c */
#if 0				/* This table is not actually needed in v6a */
extern const int jpeg12_zigzag_order[]; /* natural coef order to zigzag order */
#endif
extern const int jpeg12_natural_order[]; /* zigzag coef order to natural order */
extern const int jpeg12_natural_order7[]; /* zz to natural order for 7x7 block */
extern const int jpeg12_natural_order6[]; /* zz to natural order for 6x6 block */
extern const int jpeg12_natural_order5[]; /* zz to natural order for 5x5 block */
extern const int jpeg12_natural_order4[]; /* zz to natural order for 4x4 block */
extern const int jpeg12_natural_order3[]; /* zz to natural order for 3x3 block */
extern const int jpeg12_natural_order2[]; /* zz to natural order for 2x2 block */

/* Arithmetic coding probability estimation tables in jaricom.c */
extern const INT32 jpeg12_aritab[];

/* Suppress undefined-structure complaints if necessary. */

#ifdef INCOMPLETE_TYPES_BROKEN
#ifndef AM_MEMORY_MANAGER	/* only jmemmgr.c defines these */
struct jvirt_sarray_control { long dummy; };
struct jvirt_barray_control { long dummy; };
#endif
#endif /* INCOMPLETE_TYPES_BROKEN */
