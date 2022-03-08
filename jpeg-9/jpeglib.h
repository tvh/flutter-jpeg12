/*
 * jpeglib.h
 *
 * Copyright (C) 1991-1998, Thomas G. Lane.
 * Modified 2002-2012 by Guido Vollbeding.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file defines the application interface for the JPEG library.
 * Most applications using the library need only include this file,
 * and perhaps jerror.h if they want to know the exact error codes.
 */

#ifndef JPEGLIB_H
#define JPEGLIB_H

/*
 * First we include the configuration files that record how this
 * installation of the JPEG library is set up.  jconfig.h can be
 * generated automatically for many systems.  jmorecfg.h contains
 * manual configuration options that most people need not worry about.
 */

#ifndef JCONFIG_INCLUDED	/* in case jinclude.h already did */
#include "jconfig.h"		/* widely used configuration options */
#endif
#include "jmorecfg.h"		/* seldom changed options */


#ifdef __cplusplus
#ifndef DONT_USE_EXTERN_C
extern "C" {
#endif
#endif

/* Version IDs for the JPEG library.
 * Might be useful for tests like "#if JPEG12_LIB_VERSION >= 90".
 */

#define JPEG12_LIB_VERSION        90	/* Compatibility version 9.0 */
#define JPEG12_LIB_VERSION_MAJOR  9
#define JPEG12_LIB_VERSION_MINOR  0


/* Various constants determining the sizes of things.
 * All of these are specified by the JPEG standard, so don't change them
 * if you want to be compatible.
 */

#define DCTSIZE		    8	/* The basic DCT block is 8x8 coefficients */
#define DCTSIZE2	    64	/* DCTSIZE squared; # of elements in a block */
#define NUM_QUANT_TBLS      4	/* Quantization tables are numbered 0..3 */
#define NUM_HUFF_TBLS       4	/* Huffman tables are numbered 0..3 */
#define NUM_ARITH_TBLS      16	/* Arith-coding tables are numbered 0..15 */
#define MAX_COMPS_IN_SCAN   4	/* JPEG limit on # of components in one scan */
#define MAX_SAMP_FACTOR     4	/* JPEG limit on sampling factors */
/* Unfortunately, some bozo at Adobe saw no reason to be bound by the standard;
 * the PostScript DCT filter can emit files with many more than 10 blocks/MCU.
 * If you happen to run across such a file, you can up D_MAX_BLOCKS_IN_MCU
 * to handle it.  We even let you do this from the jconfig.h file.  However,
 * we strongly discourage changing C_MAX_BLOCKS_IN_MCU; just because Adobe
 * sometimes emits noncompliant files doesn't mean you should too.
 */
#define C_MAX_BLOCKS_IN_MCU   10 /* compressor's limit on blocks per MCU */
#ifndef D_MAX_BLOCKS_IN_MCU
#define D_MAX_BLOCKS_IN_MCU   10 /* decompressor's limit on blocks per MCU */
#endif


/* Data structures for images (arrays of samples and of DCT coefficients).
 * On 80x86 machines, the image arrays are too big for near pointers,
 * but the pointer arrays can fit in near memory.
 */

typedef JSAMPLE FAR *JSAMPROW;	/* ptr to one image row of pixel samples. */
typedef JSAMPROW *JSAMPARRAY;	/* ptr to some rows (a 2-D sample array) */
typedef JSAMPARRAY *JSAMPIMAGE;	/* a 3-D sample array: top index is color */

typedef JCOEF JBLOCK[DCTSIZE2];	/* one block of coefficients */
typedef JBLOCK FAR *JBLOCKROW;	/* pointer to one row of coefficient blocks */
typedef JBLOCKROW *JBLOCKARRAY;		/* a 2-D array of coefficient blocks */
typedef JBLOCKARRAY *JBLOCKIMAGE;	/* a 3-D array of coefficient blocks */

typedef JCOEF FAR *JCOEFPTR;	/* useful in a couple of places */


/* Types for JPEG compression parameters and working tables. */


/* DCT coefficient quantization tables. */

typedef struct {
  /* This array gives the coefficient quantizers in natural array order
   * (not the zigzag order in which they are stored in a JPEG DQT marker).
   * CAUTION: IJG versions prior to v6a kept this array in zigzag order.
   */
  UINT16 quantval[DCTSIZE2];	/* quantization step for each coefficient */
  /* This field is used only during compression.  It's initialized FALSE when
   * the table is created, and set TRUE when it's been output to the file.
   * You could suppress output of a table by setting this to TRUE.
   * (See jpeg12_suppress_tables for an example.)
   */
  boolean sent_table;		/* TRUE when table has been output */
} JQUANT_TBL;


/* Huffman coding tables. */

typedef struct {
  /* These two fields directly represent the contents of a JPEG DHT marker */
  UINT8 bits[17];		/* bits[k] = # of symbols with codes of */
				/* length k bits; bits[0] is unused */
  UINT8 huffval[256];		/* The symbols, in order of incr code length */
  /* This field is used only during compression.  It's initialized FALSE when
   * the table is created, and set TRUE when it's been output to the file.
   * You could suppress output of a table by setting this to TRUE.
   * (See jpeg12_suppress_tables for an example.)
   */
  boolean sent_table;		/* TRUE when table has been output */
} JHUFF_TBL;


/* Basic info about one component (color channel). */

typedef struct {
  /* These values are fixed over the whole image. */
  /* For compression, they must be supplied by parameter setup; */
  /* for decompression, they are read from the SOF marker. */
  int component_id;		/* identifier for this component (0..255) */
  int component_index;		/* its index in SOF or cinfo->comp_info[] */
  int h_samp_factor;		/* horizontal sampling factor (1..4) */
  int v_samp_factor;		/* vertical sampling factor (1..4) */
  int quant_tbl_no;		/* quantization table selector (0..3) */
  /* These values may vary between scans. */
  /* For compression, they must be supplied by parameter setup; */
  /* for decompression, they are read from the SOS marker. */
  /* The decompressor output side may not use these variables. */
  int dc_tbl_no;		/* DC entropy table selector (0..3) */
  int ac_tbl_no;		/* AC entropy table selector (0..3) */
  
  /* Remaining fields should be treated as private by applications. */
  
  /* These values are computed during compression or decompression startup: */
  /* Component's size in DCT blocks.
   * Any dummy blocks added to complete an MCU are not counted; therefore
   * these values do not depend on whether a scan is interleaved or not.
   */
  JDIMENSION width_in_blocks;
  JDIMENSION height_in_blocks;
  /* Size of a DCT block in samples,
   * reflecting any scaling we choose to apply during the DCT step.
   * Values from 1 to 16 are supported.
   * Note that different components may receive different DCT scalings.
   */
  int DCT_h_scaled_size;
  int DCT_v_scaled_size;
  /* The j12_downsampled dimensions are the component's actual, unpadded number
   * of samples at the main buffer (preprocessing/compression interface);
   * DCT scaling is included, so
   * j12_downsampled_width = ceil(image_width * Hi/Hmax * DCT_h_scaled_size/DCTSIZE)
   * and similarly for height.
   */
  JDIMENSION j12_downsampled_width;	 /* actual width in samples */
  JDIMENSION j12_downsampled_height; /* actual height in samples */
  /* This flag is used only for decompression.  In cases where some of the
   * components will be ignored (eg grayscale output from YCbCr image),
   * we can skip most computations for the unused components.
   */
  boolean component_needed;	/* do we need the value of this component? */

  /* These values are computed before starting a scan of the component. */
  /* The decompressor output side may not use these variables. */
  int MCU_width;		/* number of blocks per MCU, horizontally */
  int MCU_height;		/* number of blocks per MCU, vertically */
  int MCU_blocks;		/* MCU_width * MCU_height */
  int MCU_sample_width;	/* MCU width in samples: MCU_width * DCT_h_scaled_size */
  int last_col_width;		/* # of non-dummy blocks across in last MCU */
  int last_row_height;		/* # of non-dummy blocks down in last MCU */

  /* Saved quantization table for component; NULL if none yet saved.
   * See jdinput.c comments about the need for this information.
   * This field is currently used only for decompression.
   */
  JQUANT_TBL * quant_table;

  /* Private per-component storage for DCT or IDCT subsystem. */
  void * dct_table;
} jpeg12_component_info;


/* The script for encoding a multiple-scan file is an array of these: */

typedef struct {
  int comps_in_scan;		/* number of components encoded in this scan */
  int component_index[MAX_COMPS_IN_SCAN]; /* their SOF/comp_info[] indexes */
  int Ss, Se;			/* progressive JPEG spectral selection parms */
  int Ah, Al;			/* progressive JPEG successive approx. parms */
} jpeg12_scan_info;

/* The decompressor can save APPn and COM markers in a list of these: */

typedef struct jpeg12_marker_struct FAR * jpeg12_saved_marker_ptr;

struct jpeg12_marker_struct {
  jpeg12_saved_marker_ptr next;	/* next in list, or NULL */
  UINT8 marker;			/* marker code: JPEG12_COM, or JPEG12_APP0+n */
  unsigned int original_length;	/* # bytes of data in the file */
  unsigned int data_length;	/* # bytes of data saved at data[] */
  JOCTET FAR * data;		/* the data contained in the marker */
  /* the marker length word is not counted in data_length or original_length */
};

/* Known color spaces. */

typedef enum {
	JCS_UNKNOWN,		/* error/unspecified */
	JCS_GRAYSCALE,		/* monochrome */
	JCS_RGB,		/* red/green/blue */
	JCS_YCbCr,		/* Y/Cb/Cr (also known as YUV) */
	JCS_CMYK,		/* C/M/Y/K */
	JCS_YCCK		/* Y/Cb/Cr/K */
} J_COLOR_SPACE;

/* Supported color transforms. */

typedef enum {
	JCT_NONE           = 0,
	JCT_SUBTRACT_GREEN = 1
} J_COLOR_TRANSFORM;

/* DCT/IDCT algorithm options. */

typedef enum {
	JDCT_ISLOW,		/* slow but accurate integer algorithm */
	JDCT_IFAST,		/* faster, less accurate integer method */
	JDCT_FLOAT		/* floating-point: accurate, fast on fast HW */
} J_DCT_METHOD;

#ifndef JDCT_DEFAULT		/* may be overridden in jconfig.h */
#define JDCT_DEFAULT  JDCT_ISLOW
#endif
#ifndef JDCT_FASTEST		/* may be overridden in jconfig.h */
#define JDCT_FASTEST  JDCT_IFAST
#endif

/* Dithering options for decompression. */

typedef enum {
	JDITHER_NONE,		/* no dithering */
	JDITHER_ORDERED,	/* simple ordered dither */
	JDITHER_FS		/* Floyd-Steinberg error diffusion dither */
} J_DITHER_MODE;


/* Common fields between JPEG compression and decompression master structs. */

#define jpeg12_common_fields \
  struct jpeg12_error_mgr * err;	/* Error handler module */\
  struct jpeg12_memory_mgr * mem;	/* Memory manager module */\
  struct jpeg12_progress_mgr * progress; /* Progress monitor, or NULL if none */\
  void * client_data;		/* Available for use by application */\
  boolean is_decompressor;	/* So common code can tell which is which */\
  int global_state		/* For checking call sequence validity */

/* Routines that are to be used by both halves of the library are declared
 * to receive a pointer to this structure.  There are no actual instances of
 * jpeg12_common_struct, only of jpeg12_compress_struct and jpeg12_decompress_struct.
 */
struct jpeg12_common_struct {
  jpeg12_common_fields;		/* Fields common to both master struct types */
  /* Additional fields follow in an actual jpeg12_compress_struct or
   * jpeg12_decompress_struct.  All three structs must agree on these
   * initial fields!  (This would be a lot cleaner in C++.)
   */
};

typedef struct jpeg12_common_struct * j12_common_ptr;
typedef struct jpeg12_compress_struct * j12_compress_ptr;
typedef struct jpeg12_decompress_struct * j12_decompress_ptr;


/* Master record for a compression instance */

struct jpeg12_compress_struct {
  jpeg12_common_fields;		/* Fields shared with jpeg12_decompress_struct */

  /* Destination for compressed data */
  struct jpeg12_destination_mgr * dest;

  /* Description of source image --- these fields must be filled in by
   * outer application before starting compression.  in_color_space must
   * be correct before you can even call jpeg12_set_defaults().
   */

  JDIMENSION image_width;	/* input image width */
  JDIMENSION image_height;	/* input image height */
  int input_components;		/* # of color components in input image */
  J_COLOR_SPACE in_color_space;	/* colorspace of input image */

  double input_gamma;		/* image gamma of input image */

  /* Compression parameters --- these fields must be set before calling
   * jpeg12_start_compress().  We recommend calling jpeg12_set_defaults() to
   * initialize everything to reasonable defaults, then changing anything
   * the application specifically wants to change.  That way you won't get
   * burnt when new parameters are added.  Also note that there are several
   * helper routines to simplify changing parameters.
   */

  unsigned int scale_num, scale_denom; /* fraction by which to scale image */

  JDIMENSION jpeg12_width;	/* scaled JPEG image width */
  JDIMENSION jpeg12_height;	/* scaled JPEG image height */
  /* Dimensions of actual JPEG image that will be written to file,
   * derived from input dimensions by scaling factors above.
   * These fields are computed by jpeg12_start_compress().
   * You can also use jpeg12_calc_jpeg12_dimensions() to determine these values
   * in advance of calling jpeg12_start_compress().
   */

  int data_precision;		/* bits of precision in image data */

  int num_components;		/* # of color components in JPEG image */
  J_COLOR_SPACE jpeg12_color_space; /* colorspace of JPEG image */

  jpeg12_component_info * comp_info;
  /* comp_info[i] describes component that appears i'th in SOF */

  JQUANT_TBL * quant_tbl_ptrs[NUM_QUANT_TBLS];
  int q_scale_factor[NUM_QUANT_TBLS];
  /* ptrs to coefficient quantization tables, or NULL if not defined,
   * and corresponding scale factors (percentage, initialized 100).
   */

  JHUFF_TBL * dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
  JHUFF_TBL * ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
  /* ptrs to Huffman coding tables, or NULL if not defined */

  UINT8 arith_dc_L[NUM_ARITH_TBLS]; /* L values for DC arith-coding tables */
  UINT8 arith_dc_U[NUM_ARITH_TBLS]; /* U values for DC arith-coding tables */
  UINT8 arith_ac_K[NUM_ARITH_TBLS]; /* Kx values for AC arith-coding tables */

  int num_scans;		/* # of entries in scan_info array */
  const jpeg12_scan_info * scan_info; /* script for multi-scan file, or NULL */
  /* The default value of scan_info is NULL, which causes a single-scan
   * sequential JPEG file to be emitted.  To create a multi-scan file,
   * set num_scans and scan_info to point to an array of scan definitions.
   */

  boolean raw_data_in;		/* TRUE=caller supplies j12_downsampled data */
  boolean arith_code;		/* TRUE=arithmetic coding, FALSE=Huffman */
  boolean optimize_coding;	/* TRUE=optimize entropy encoding parms */
  boolean CCIR601_sampling;	/* TRUE=first samples are cosited */
  boolean do_fancy_downsampling; /* TRUE=apply fancy downsampling */
  int smoothing_factor;		/* 1..100, or 0 for no input smoothing */
  J_DCT_METHOD dct_method;	/* DCT algorithm selector */

  /* The restart interval can be specified in absolute MCUs by setting
   * restart_interval, or in MCU rows by setting restart_in_rows
   * (in which case the correct restart_interval will be figured
   * for each scan).
   */
  unsigned int restart_interval; /* MCUs per restart, or 0 for no restart */
  int restart_in_rows;		/* if > 0, MCU rows per restart interval */

  /* Parameters controlling emission of special markers. */

  boolean write_JFIF_header;	/* should a JFIF marker be written? */
  UINT8 JFIF_major_version;	/* What to write for the JFIF version number */
  UINT8 JFIF_minor_version;
  /* These three values are not used by the JPEG code, merely copied */
  /* into the JFIF APP0 marker.  density_unit can be 0 for unknown, */
  /* 1 for dots/inch, or 2 for dots/cm.  Note that the pixel aspect */
  /* ratio is defined by X_density/Y_density even when density_unit=0. */
  UINT8 density_unit;		/* JFIF code for pixel size units */
  UINT16 X_density;		/* Horizontal pixel density */
  UINT16 Y_density;		/* Vertical pixel density */
  boolean write_Adobe_marker;	/* should an Adobe marker be written? */

  J_COLOR_TRANSFORM color_transform;
  /* Color transform identifier, writes LSE marker if nonzero */

  /* State variable: index of next scanline to be written to
   * jpeg12_write_scanlines().  Application may use this to control its
   * processing loop, e.g., "while (next_scanline < image_height)".
   */

  JDIMENSION next_scanline;	/* 0 .. image_height-1  */

  /* Remaining fields are known throughout compressor, but generally
   * should not be touched by a surrounding application.
   */

  /*
   * These fields are computed during compression startup
   */
  boolean progressive_mode;	/* TRUE if scan script uses progressive mode */
  int max_h_samp_factor;	/* largest h_samp_factor */
  int max_v_samp_factor;	/* largest v_samp_factor */

  int min_DCT_h_scaled_size;	/* smallest DCT_h_scaled_size of any component */
  int min_DCT_v_scaled_size;	/* smallest DCT_v_scaled_size of any component */

  JDIMENSION total_iMCU_rows;	/* # of iMCU rows to be input to coef ctlr */
  /* The coefficient controller receives data in units of MCU rows as defined
   * for fully interleaved scans (whether the JPEG file is interleaved or not).
   * There are v_samp_factor * DCTSIZE sample rows of each component in an
   * "iMCU" (interleaved MCU) row.
   */
  
  /*
   * These fields are valid during any one scan.
   * They describe the components and MCUs actually appearing in the scan.
   */
  int comps_in_scan;		/* # of JPEG components in this scan */
  jpeg12_component_info * cur_comp_info[MAX_COMPS_IN_SCAN];
  /* *cur_comp_info[i] describes component that appears i'th in SOS */
  
  JDIMENSION MCUs_per_row;	/* # of MCUs across the image */
  JDIMENSION MCU_rows_in_scan;	/* # of MCU rows in the image */
  
  int blocks_in_MCU;		/* # of DCT blocks per MCU */
  int MCU_membership[C_MAX_BLOCKS_IN_MCU];
  /* MCU_membership[i] is index in cur_comp_info of component owning */
  /* i'th block in an MCU */

  int Ss, Se, Ah, Al;		/* progressive JPEG parameters for scan */

  int block_size;		/* the basic DCT block size: 1..16 */
  const int * natural_order;	/* natural-order position array */
  int lim_Se;			/* min( Se, DCTSIZE2-1 ) */

  /*
   * Links to compression subobjects (methods and private variables of modules)
   */
  struct jpeg12_comp_master * master;
  struct jpeg12_c_main_controller * main;
  struct jpeg12_c_prep_controller * prep;
  struct jpeg12_c_coef_controller * coef;
  struct jpeg12_marker_writer * marker;
  struct jpeg12_j12_color_converter * cconvert;
  struct jpeg12_j12_downsampler * j12_downsample;
  struct jpeg12_forward_dct * fdct;
  struct jpeg12_entropy_encoder * entropy;
  jpeg12_scan_info * script_space; /* workspace for jpeg12_simple_progression */
  int script_space_size;
};


/* Master record for a decompression instance */

struct jpeg12_decompress_struct {
  jpeg12_common_fields;		/* Fields shared with jpeg12_compress_struct */

  /* Source of compressed data */
  struct jpeg12_source_mgr * src;

  /* Basic description of image --- filled in by jpeg12_read_header(). */
  /* Application may inspect these values to decide how to process image. */

  JDIMENSION image_width;	/* nominal image width (from SOF marker) */
  JDIMENSION image_height;	/* nominal image height */
  int num_components;		/* # of color components in JPEG image */
  J_COLOR_SPACE jpeg12_color_space; /* colorspace of JPEG image */

  /* Decompression processing parameters --- these fields must be set before
   * calling jpeg12_start_decompress().  Note that jpeg12_read_header() initializes
   * them to default values.
   */

  J_COLOR_SPACE out_color_space; /* colorspace for output */

  unsigned int scale_num, scale_denom; /* fraction by which to scale image */

  double output_gamma;		/* image gamma wanted in output */

  boolean buffered_image;	/* TRUE=multiple output passes */
  boolean raw_data_out;		/* TRUE=j12_downsampled data wanted */

  J_DCT_METHOD dct_method;	/* IDCT algorithm selector */
  boolean do_fancy_upsampling;	/* TRUE=apply fancy upsampling */
  boolean do_block_smoothing;	/* TRUE=apply interblock smoothing */

  boolean quantize_colors;	/* TRUE=colormapped output wanted */
  /* the following are ignored if not quantize_colors: */
  J_DITHER_MODE dither_mode;	/* type of color dithering to use */
  boolean two_pass_quantize;	/* TRUE=use two-pass color quantization */
  int desired_number_of_colors;	/* max # colors to use in created colormap */
  /* these are significant only in buffered-image mode: */
  boolean enable_1pass_quant;	/* enable future use of 1-pass quantizer */
  boolean enable_external_quant;/* enable future use of external colormap */
  boolean enable_2pass_quant;	/* enable future use of 2-pass quantizer */

  /* Description of actual output image that will be returned to application.
   * These fields are computed by jpeg12_start_decompress().
   * You can also use jpeg12_calc_output_dimensions() to determine these values
   * in advance of calling jpeg12_start_decompress().
   */

  JDIMENSION output_width;	/* scaled image width */
  JDIMENSION output_height;	/* scaled image height */
  int out_color_components;	/* # of color components in out_color_space */
  int output_components;	/* # of color components returned */
  /* output_components is 1 (a colormap index) when quantizing colors;
   * otherwise it equals out_color_components.
   */
  int rec_outbuf_height;	/* min recommended height of scanline buffer */
  /* If the buffer passed to jpeg12_read_scanlines() is less than this many rows
   * high, space and time will be wasted due to unnecessary data copying.
   * Usually rec_outbuf_height will be 1 or 2, at most 4.
   */

  /* When quantizing colors, the output colormap is described by these fields.
   * The application can supply a colormap by setting colormap non-NULL before
   * calling jpeg12_start_decompress; otherwise a colormap is created during
   * jpeg12_start_decompress or jpeg12_j12_start_output.
   * The map has out_color_components rows and actual_number_of_colors columns.
   */
  int actual_number_of_colors;	/* number of entries in use */
  JSAMPARRAY colormap;		/* The color map as a 2-D pixel array */

  /* State variables: these variables indicate the progress of decompression.
   * The application may examine these but must not modify them.
   */

  /* Row index of next scanline to be read from jpeg12_read_scanlines().
   * Application may use this to control its processing loop, e.g.,
   * "while (output_scanline < output_height)".
   */
  JDIMENSION output_scanline;	/* 0 .. output_height-1  */

  /* Current input scan number and number of iMCU rows completed in scan.
   * These indicate the progress of the decompressor input side.
   */
  int input_scan_number;	/* Number of SOS markers seen so far */
  JDIMENSION input_iMCU_row;	/* Number of iMCU rows completed */

  /* The "output scan number" is the notional scan being displayed by the
   * output side.  The decompressor will not allow output scan/row number
   * to get ahead of input scan/row, but it can fall arbitrarily far behind.
   */
  int output_scan_number;	/* Nominal scan number being displayed */
  JDIMENSION output_iMCU_row;	/* Number of iMCU rows read */

  /* Current progression status.  coef_bits[c][i] indicates the precision
   * with which component c's DCT coefficient i (in zigzag order) is known.
   * It is -1 when no data has yet been received, otherwise it is the point
   * transform (shift) value for the most recent scan of the coefficient
   * (thus, 0 at completion of the progression).
   * This pointer is NULL when reading a non-progressive file.
   */
  int (*coef_bits)[DCTSIZE2];	/* -1 or current Al value for each coef */

  /* Internal JPEG parameters --- the application usually need not look at
   * these fields.  Note that the decompressor output side may not use
   * any parameters that can change between scans.
   */

  /* Quantization and Huffman tables are carried forward across input
   * datastreams when processing abbreviated JPEG datastreams.
   */

  JQUANT_TBL * quant_tbl_ptrs[NUM_QUANT_TBLS];
  /* ptrs to coefficient quantization tables, or NULL if not defined */

  JHUFF_TBL * dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
  JHUFF_TBL * ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
  /* ptrs to Huffman coding tables, or NULL if not defined */

  /* These parameters are never carried across datastreams, since they
   * are given in SOF/SOS markers or defined to be reset by SOI.
   */

  int data_precision;		/* bits of precision in image data */

  jpeg12_component_info * comp_info;
  /* comp_info[i] describes component that appears i'th in SOF */

  boolean is_baseline;		/* TRUE if Baseline SOF0 encountered */
  boolean progressive_mode;	/* TRUE if SOFn specifies progressive mode */
  boolean arith_code;		/* TRUE=arithmetic coding, FALSE=Huffman */

  UINT8 arith_dc_L[NUM_ARITH_TBLS]; /* L values for DC arith-coding tables */
  UINT8 arith_dc_U[NUM_ARITH_TBLS]; /* U values for DC arith-coding tables */
  UINT8 arith_ac_K[NUM_ARITH_TBLS]; /* Kx values for AC arith-coding tables */

  unsigned int restart_interval; /* MCUs per restart interval, or 0 for no restart */

  /* These fields record data obtained from optional markers recognized by
   * the JPEG library.
   */
  boolean saw_JFIF_marker;	/* TRUE iff a JFIF APP0 marker was found */
  /* Data copied from JFIF marker; only valid if saw_JFIF_marker is TRUE: */
  UINT8 JFIF_major_version;	/* JFIF version number */
  UINT8 JFIF_minor_version;
  UINT8 density_unit;		/* JFIF code for pixel size units */
  UINT16 X_density;		/* Horizontal pixel density */
  UINT16 Y_density;		/* Vertical pixel density */
  boolean saw_Adobe_marker;	/* TRUE iff an Adobe APP14 marker was found */
  UINT8 Adobe_transform;	/* Color transform code from Adobe marker */

  J_COLOR_TRANSFORM color_transform;
  /* Color transform identifier derived from LSE marker, otherwise zero */

  boolean CCIR601_sampling;	/* TRUE=first samples are cosited */

  /* Aside from the specific data retained from APPn markers known to the
   * library, the uninterpreted contents of any or all APPn and COM markers
   * can be saved in a list for examination by the application.
   */
  jpeg12_saved_marker_ptr marker_list; /* Head of list of saved markers */

  /* Remaining fields are known throughout decompressor, but generally
   * should not be touched by a surrounding application.
   */

  /*
   * These fields are computed during decompression startup
   */
  int max_h_samp_factor;	/* largest h_samp_factor */
  int max_v_samp_factor;	/* largest v_samp_factor */

  int min_DCT_h_scaled_size;	/* smallest DCT_h_scaled_size of any component */
  int min_DCT_v_scaled_size;	/* smallest DCT_v_scaled_size of any component */

  JDIMENSION total_iMCU_rows;	/* # of iMCU rows in image */
  /* The coefficient controller's input and output progress is measured in
   * units of "iMCU" (interleaved MCU) rows.  These are the same as MCU rows
   * in fully interleaved JPEG scans, but are used whether the scan is
   * interleaved or not.  We define an iMCU row as v_samp_factor DCT block
   * rows of each component.  Therefore, the IDCT output contains
   * v_samp_factor*DCT_v_scaled_size sample rows of a component per iMCU row.
   */

  JSAMPLE * sample_range_limit; /* table for fast range-limiting */

  /*
   * These fields are valid during any one scan.
   * They describe the components and MCUs actually appearing in the scan.
   * Note that the decompressor output side must not use these fields.
   */
  int comps_in_scan;		/* # of JPEG components in this scan */
  jpeg12_component_info * cur_comp_info[MAX_COMPS_IN_SCAN];
  /* *cur_comp_info[i] describes component that appears i'th in SOS */

  JDIMENSION MCUs_per_row;	/* # of MCUs across the image */
  JDIMENSION MCU_rows_in_scan;	/* # of MCU rows in the image */

  int blocks_in_MCU;		/* # of DCT blocks per MCU */
  int MCU_membership[D_MAX_BLOCKS_IN_MCU];
  /* MCU_membership[i] is index in cur_comp_info of component owning */
  /* i'th block in an MCU */

  int Ss, Se, Ah, Al;		/* progressive JPEG parameters for scan */

  /* These fields are derived from Se of first SOS marker.
   */
  int block_size;		/* the basic DCT block size: 1..16 */
  const int * natural_order; /* natural-order position array for entropy decode */
  int lim_Se;			/* min( Se, DCTSIZE2-1 ) for entropy decode */

  /* This field is shared between entropy decoder and marker parser.
   * It is either zero or the code of a JPEG marker that has been
   * read from the data source, but has not yet been processed.
   */
  int unread_marker;

  /*
   * Links to decompression subobjects (methods, private variables of modules)
   */
  struct jpeg12_decomp_master * master;
  struct jpeg12_d_main_controller * main;
  struct jpeg12_d_coef_controller * coef;
  struct jpeg12_d_post_controller * post;
  struct jpeg12_input_controller * inputctl;
  struct jpeg12_marker_reader * marker;
  struct jpeg12_entropy_decoder * entropy;
  struct jpeg12_inverse_dct * idct;
  struct jpeg12_j12_upsampler * j12_upsample;
  struct jpeg12_color_deconverter * cconvert;
  struct jpeg12_j12_color_quantizer * cquantize;
};


/* "Object" declarations for JPEG modules that may be supplied or called
 * directly by the surrounding application.
 * As with all objects in the JPEG library, these structs only define the
 * publicly visible methods and state variables of a module.  Additional
 * private fields may exist after the public ones.
 */


/* Error handler object */

struct jpeg12_error_mgr {
  /* Error exit handler: does not return to caller */
  JMETHOD(noreturn_t, j12_error_exit, (j12_common_ptr cinfo));
  /* Conditionally emit a trace or warning message */
  JMETHOD(void, j12_emit_message, (j12_common_ptr cinfo, int msg_level));
  /* Routine that actually outputs a trace or error message */
  JMETHOD(void, j12_output_message, (j12_common_ptr cinfo));
  /* Format a message string for the most recent JPEG error or message */
  JMETHOD(void, j12_format_message, (j12_common_ptr cinfo, char * buffer));
#define JMSG_LENGTH_MAX  200	/* recommended size of j12_format_message buffer */
  /* Reset error state variables at start of a new image */
  JMETHOD(void, j12_reset_error_mgr, (j12_common_ptr cinfo));
  
  /* The message ID code and any parameters are saved here.
   * A message can have one string parameter or up to 8 int parameters.
   */
  int msg_code;
#define JMSG_STR_PARM_MAX  80
  union {
    int i[8];
    char s[JMSG_STR_PARM_MAX];
  } msg_parm;
  
  /* Standard state variables for error facility */
  
  int trace_level;		/* max msg_level that will be displayed */
  
  /* For recoverable corrupt-data errors, we emit a warning message,
   * but keep going unless j12_emit_message chooses to abort.  j12_emit_message
   * should count warnings in num_warnings.  The surrounding application
   * can check for bad data by seeing if num_warnings is nonzero at the
   * end of processing.
   */
  long num_warnings;		/* number of corrupt-data warnings */

  /* These fields point to the table(s) of error message strings.
   * An application can change the table pointer to switch to a different
   * message list (typically, to change the language in which errors are
   * reported).  Some applications may wish to add additional error codes
   * that will be handled by the JPEG library error mechanism; the second
   * table pointer is used for this purpose.
   *
   * First table includes all errors generated by JPEG library itself.
   * Error code 0 is reserved for a "no such error string" message.
   */
  const char * const * jpeg12_message_table; /* Library errors */
  int last_jpeg12_message;    /* Table contains strings 0..last_jpeg12_message */
  /* Second table can be added by application (see cjpeg/djpeg for example).
   * It contains strings numbered first_addon_message..last_addon_message.
   */
  const char * const * addon_message_table; /* Non-library errors */
  int first_addon_message;	/* code for first string in addon table */
  int last_addon_message;	/* code for last string in addon table */
};


/* Progress monitor object */

struct jpeg12_progress_mgr {
  JMETHOD(void, j12_progress_monitor, (j12_common_ptr cinfo));

  long pass_counter;		/* work units completed in this pass */
  long pass_limit;		/* total number of work units in this pass */
  int completed_passes;		/* passes completed so far */
  int total_passes;		/* total number of passes expected */
};


/* Data destination object for compression */

struct jpeg12_destination_mgr {
  JOCTET * next_output_byte;	/* => next byte to write in buffer */
  size_t free_in_buffer;	/* # of byte spaces remaining in buffer */

  JMETHOD(void, j12_init_destination, (j12_compress_ptr cinfo));
  JMETHOD(boolean, j12_empty_output_buffer, (j12_compress_ptr cinfo));
  JMETHOD(void, j12_term_destination, (j12_compress_ptr cinfo));
};


/* Data source object for decompression */

struct jpeg12_source_mgr {
  const JOCTET * next_input_byte; /* => next byte to read from buffer */
  size_t bytes_in_buffer;	/* # of bytes remaining in buffer */

  JMETHOD(void, j12_init_source, (j12_decompress_ptr cinfo));
  JMETHOD(boolean, j12_fill_input_buffer, (j12_decompress_ptr cinfo));
  JMETHOD(void, j12_skip_input_data, (j12_decompress_ptr cinfo, long num_bytes));
  JMETHOD(boolean, j12_resync_to_restart, (j12_decompress_ptr cinfo, int desired));
  JMETHOD(void, j12_term_source, (j12_decompress_ptr cinfo));
};


/* Memory manager object.
 * Allocates "small" objects (a few K total), "large" objects (tens of K),
 * and "really big" objects (virtual arrays with backing store if needed).
 * The memory manager does not allow individual objects to be freed; rather,
 * each created object is assigned to a pool, and whole pools can be freed
 * at once.  This is faster and more convenient than remembering exactly what
 * to free, especially where malloc()/free() are not too speedy.
 * NB: alloc routines never return NULL.  They exit to j12_error_exit if not
 * successful.
 */

#define JPOOL_PERMANENT	0	/* lasts until master record is destroyed */
#define JPOOL_IMAGE	1	/* lasts until done with image/datastream */
#define JPOOL_NUMPOOLS	2

typedef struct jvirt_sarray_control * jvirt_sarray_ptr;
typedef struct jvirt_barray_control * jvirt_barray_ptr;


struct jpeg12_memory_mgr {
  /* Method pointers */
  JMETHOD(void *, j12_alloc_small, (j12_common_ptr cinfo, int pool_id,
				size_t sizeofobject));
  JMETHOD(void FAR *, j12_alloc_large, (j12_common_ptr cinfo, int pool_id,
				     size_t sizeofobject));
  JMETHOD(JSAMPARRAY, j12_alloc_sarray, (j12_common_ptr cinfo, int pool_id,
				     JDIMENSION samplesperrow,
				     JDIMENSION numrows));
  JMETHOD(JBLOCKARRAY, j12_alloc_barray, (j12_common_ptr cinfo, int pool_id,
				      JDIMENSION blocksperrow,
				      JDIMENSION numrows));
  JMETHOD(jvirt_sarray_ptr, j12_request_virt_sarray, (j12_common_ptr cinfo,
						  int pool_id,
						  boolean pre_zero,
						  JDIMENSION samplesperrow,
						  JDIMENSION numrows,
						  JDIMENSION maxaccess));
  JMETHOD(jvirt_barray_ptr, j12_request_virt_barray, (j12_common_ptr cinfo,
						  int pool_id,
						  boolean pre_zero,
						  JDIMENSION blocksperrow,
						  JDIMENSION numrows,
						  JDIMENSION maxaccess));
  JMETHOD(void, j12_realize_virt_arrays, (j12_common_ptr cinfo));
  JMETHOD(JSAMPARRAY, j12_access_virt_sarray, (j12_common_ptr cinfo,
					   jvirt_sarray_ptr ptr,
					   JDIMENSION start_row,
					   JDIMENSION num_rows,
					   boolean writable));
  JMETHOD(JBLOCKARRAY, j12_access_virt_barray, (j12_common_ptr cinfo,
					    jvirt_barray_ptr ptr,
					    JDIMENSION start_row,
					    JDIMENSION num_rows,
					    boolean writable));
  JMETHOD(void, j12_free_pool, (j12_common_ptr cinfo, int pool_id));
  JMETHOD(void, j12_self_destruct, (j12_common_ptr cinfo));

  /* Limit on memory allocation for this JPEG object.  (Note that this is
   * merely advisory, not a guaranteed maximum; it only affects the space
   * used for virtual-array buffers.)  May be changed by outer application
   * after creating the JPEG object.
   */
  long max_memory_to_use;

  /* Maximum allocation request accepted by j12_alloc_large. */
  long max_alloc_chunk;
};


/* Routine signature for application-supplied marker processing methods.
 * Need not pass marker code since it is stored in cinfo->unread_marker.
 */
typedef JMETHOD(boolean, jpeg12_marker_parser_method, (j12_decompress_ptr cinfo));


/* Declarations for routines called by application.
 * The JPP macro hides prototype parameters from compilers that can't cope.
 * Note JPP requires double parentheses.
 */

#ifdef HAVE_PROTOTYPES
#define JPP(arglist)	arglist
#else
#define JPP(arglist)	()
#endif


/* Short forms of external names for systems with brain-damaged linkers.
 * We shorten external names to be unique in the first six letters, which
 * is good enough for all known systems.
 * (If your compiler itself needs names to be unique in less than 15 
 * characters, you are out of luck.  Get a better compiler.)
 */

#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jpeg12_std_error		jStdError
#define jpeg12_CreateCompress	jCreaCompress
#define jpeg12_CreateDecompress	jCreaDecompress
#define jpeg12_destroy_compress	jDestCompress
#define jpeg12_destroy_decompress	jDestDecompress
#define jpeg12_stdio_dest		jStdDest
#define jpeg12_stdio_src		jStdSrc
#define jpeg12_mem_dest		jMemDest
#define jpeg12_mem_src		jMemSrc
#define jpeg12_set_defaults	jSetDefaults
#define jpeg12_set_colorspace	jSetColorspace
#define jpeg12_default_colorspace	jDefColorspace
#define jpeg12_set_quality	jSetQuality
#define jpeg12_set_linear_quality	jSetLQuality
#define jpeg12_default_qtables	jDefQTables
#define jpeg12_add_quant_table	jAddQuantTable
#define jpeg12_quality_scaling	jQualityScaling
#define jpeg12_simple_progression	jSimProgress
#define jpeg12_suppress_tables	jSuppressTables
#define jpeg12_alloc_quant_table	jAlcQTable
#define jpeg12_alloc_huff_table	jAlcHTable
#define jpeg12_start_compress	jStrtCompress
#define jpeg12_write_scanlines	jWrtScanlines
#define jpeg12_finish_compress	jFinCompress
#define jpeg12_calc_jpeg12_dimensions	jCjpegDimensions
#define jpeg12_write_raw_data	jWrtRawData
#define jpeg12_write_marker	jWrtMarker
#define jpeg12_write_m_header	jWrtMHeader
#define jpeg12_write_m_byte	jWrtMByte
#define jpeg12_write_tables	jWrtTables
#define jpeg12_read_header	jReadHeader
#define jpeg12_start_decompress	jStrtDecompress
#define jpeg12_read_scanlines	jReadScanlines
#define jpeg12_finish_decompress	jFinDecompress
#define jpeg12_read_raw_data	jReadRawData
#define jpeg12_has_multiple_scans	jHasMultScn
#define jpeg12_j12_start_output	jStrtOutput
#define jpeg12_j12_finish_output	jFinOutput
#define jpeg12_input_complete	jInComplete
#define jpeg12_new_colormap	jNewCMap
#define jpeg12_j12_consume_input	jConsumeInput
#define jpeg12_core_output_dimensions	jCoreDimensions
#define jpeg12_calc_output_dimensions	jCalcDimensions
#define jpeg12_save_markers	jSaveMarkers
#define jpeg12_set_marker_processor	jSetMarker
#define jpeg12_read_coefficients	jReadCoefs
#define jpeg12_write_coefficients	jWrtCoefs
#define jpeg12_copy_critical_parameters	jCopyCrit
#define jpeg12_abort_compress	jAbrtCompress
#define jpeg12_abort_decompress	jAbrtDecompress
#define jpeg12_abort		jAbort
#define jpeg12_destroy		jDestroy
#define jpeg12_j12_resync_to_restart	jResyncRestart
#endif /* NEED_SHORT_EXTERNAL_NAMES */


/* Default error-management setup */
EXTERN(struct jpeg12_error_mgr *) jpeg12_std_error
	JPP((struct jpeg12_error_mgr * err));

/* Initialization of JPEG compression objects.
 * jpeg12_create_compress() and jpeg12_create_decompress() are the exported
 * names that applications should call.  These expand to calls on
 * jpeg12_CreateCompress and jpeg12_CreateDecompress with additional information
 * passed for version mismatch checking.
 * NB: you must set up the error-manager BEFORE calling jpeg12_create_xxx.
 */
#define jpeg12_create_compress(cinfo) \
    jpeg12_CreateCompress((cinfo), JPEG12_LIB_VERSION, \
			(size_t) sizeof(struct jpeg12_compress_struct))
#define jpeg12_create_decompress(cinfo) \
    jpeg12_CreateDecompress((cinfo), JPEG12_LIB_VERSION, \
			  (size_t) sizeof(struct jpeg12_decompress_struct))
EXTERN(void) jpeg12_CreateCompress JPP((j12_compress_ptr cinfo,
				      int version, size_t structsize));
EXTERN(void) jpeg12_CreateDecompress JPP((j12_decompress_ptr cinfo,
					int version, size_t structsize));
/* Destruction of JPEG compression objects */
EXTERN(void) jpeg12_destroy_compress JPP((j12_compress_ptr cinfo));
EXTERN(void) jpeg12_destroy_decompress JPP((j12_decompress_ptr cinfo));

/* Standard data source and destination managers: stdio streams. */
/* Caller is responsible for opening the file before and closing after. */
EXTERN(void) jpeg12_stdio_dest JPP((j12_compress_ptr cinfo, FILE * outfile));
EXTERN(void) jpeg12_stdio_src JPP((j12_decompress_ptr cinfo, FILE * infile));

/* Data source and destination managers: memory buffers. */
EXTERN(void) jpeg12_mem_dest JPP((j12_compress_ptr cinfo,
			       unsigned char ** outbuffer,
			       unsigned long * outsize));
EXTERN(void) jpeg12_mem_src JPP((j12_decompress_ptr cinfo,
			      unsigned char * inbuffer,
			      unsigned long insize));

/* Default parameter setup for compression */
EXTERN(void) jpeg12_set_defaults JPP((j12_compress_ptr cinfo));
/* Compression parameter setup aids */
EXTERN(void) jpeg12_set_colorspace JPP((j12_compress_ptr cinfo,
				      J_COLOR_SPACE colorspace));
EXTERN(void) jpeg12_default_colorspace JPP((j12_compress_ptr cinfo));
EXTERN(void) jpeg12_set_quality JPP((j12_compress_ptr cinfo, int quality,
				   boolean force_baseline));
EXTERN(void) jpeg12_set_linear_quality JPP((j12_compress_ptr cinfo,
					  int scale_factor,
					  boolean force_baseline));
EXTERN(void) jpeg12_default_qtables JPP((j12_compress_ptr cinfo,
				       boolean force_baseline));
EXTERN(void) jpeg12_add_quant_table JPP((j12_compress_ptr cinfo, int which_tbl,
				       const unsigned int *basic_table,
				       int scale_factor,
				       boolean force_baseline));
EXTERN(int) jpeg12_quality_scaling JPP((int quality));
EXTERN(void) jpeg12_simple_progression JPP((j12_compress_ptr cinfo));
EXTERN(void) jpeg12_suppress_tables JPP((j12_compress_ptr cinfo,
				       boolean suppress));
EXTERN(JQUANT_TBL *) jpeg12_alloc_quant_table JPP((j12_common_ptr cinfo));
EXTERN(JHUFF_TBL *) jpeg12_alloc_huff_table JPP((j12_common_ptr cinfo));

/* Main entry points for compression */
EXTERN(void) jpeg12_start_compress JPP((j12_compress_ptr cinfo,
				      boolean write_all_tables));
EXTERN(JDIMENSION) jpeg12_write_scanlines JPP((j12_compress_ptr cinfo,
					     JSAMPARRAY scanlines,
					     JDIMENSION num_lines));
EXTERN(void) jpeg12_finish_compress JPP((j12_compress_ptr cinfo));

/* Precalculate JPEG dimensions for current compression parameters. */
EXTERN(void) jpeg12_calc_jpeg12_dimensions JPP((j12_compress_ptr cinfo));

/* Replaces jpeg12_write_scanlines when writing raw j12_downsampled data. */
EXTERN(JDIMENSION) jpeg12_write_raw_data JPP((j12_compress_ptr cinfo,
					    JSAMPIMAGE data,
					    JDIMENSION num_lines));

/* Write a special marker.  See libjpeg.txt concerning safe usage. */
EXTERN(void) jpeg12_write_marker
	JPP((j12_compress_ptr cinfo, int marker,
	     const JOCTET * dataptr, unsigned int datalen));
/* Same, but piecemeal. */
EXTERN(void) jpeg12_write_m_header
	JPP((j12_compress_ptr cinfo, int marker, unsigned int datalen));
EXTERN(void) jpeg12_write_m_byte
	JPP((j12_compress_ptr cinfo, int val));

/* Alternate compression function: just write an abbreviated table file */
EXTERN(void) jpeg12_write_tables JPP((j12_compress_ptr cinfo));

/* Decompression startup: read start of JPEG datastream to see what's there */
EXTERN(int) jpeg12_read_header JPP((j12_decompress_ptr cinfo,
				  boolean require_image));
/* Return value is one of: */
#define JPEG12_SUSPENDED		0 /* Suspended due to lack of input data */
#define JPEG12_HEADER_OK		1 /* Found valid image datastream */
#define JPEG12_HEADER_TABLES_ONLY	2 /* Found valid table-specs-only datastream */
/* If you pass require_image = TRUE (normal case), you need not check for
 * a TABLES_ONLY return code; an abbreviated file will cause an error exit.
 * JPEG12_SUSPENDED is only possible if you use a data source module that can
 * give a suspension return (the stdio source module doesn't).
 */

/* Main entry points for decompression */
EXTERN(boolean) jpeg12_start_decompress JPP((j12_decompress_ptr cinfo));
EXTERN(JDIMENSION) jpeg12_read_scanlines JPP((j12_decompress_ptr cinfo,
					    JSAMPARRAY scanlines,
					    JDIMENSION max_lines));
EXTERN(boolean) jpeg12_finish_decompress JPP((j12_decompress_ptr cinfo));

/* Replaces jpeg12_read_scanlines when reading raw j12_downsampled data. */
EXTERN(JDIMENSION) jpeg12_read_raw_data JPP((j12_decompress_ptr cinfo,
					   JSAMPIMAGE data,
					   JDIMENSION max_lines));

/* Additional entry points for buffered-image mode. */
EXTERN(boolean) jpeg12_has_multiple_scans JPP((j12_decompress_ptr cinfo));
EXTERN(boolean) jpeg12_j12_start_output JPP((j12_decompress_ptr cinfo,
				       int scan_number));
EXTERN(boolean) jpeg12_j12_finish_output JPP((j12_decompress_ptr cinfo));
EXTERN(boolean) jpeg12_input_complete JPP((j12_decompress_ptr cinfo));
EXTERN(void) jpeg12_new_colormap JPP((j12_decompress_ptr cinfo));
EXTERN(int) jpeg12_j12_consume_input JPP((j12_decompress_ptr cinfo));
/* Return value is one of: */
/* #define JPEG12_SUSPENDED	0    Suspended due to lack of input data */
#define JPEG12_REACHED_SOS	1 /* Reached start of new scan */
#define JPEG12_REACHED_EOI	2 /* Reached end of image */
#define JPEG12_ROW_COMPLETED	3 /* Completed one iMCU row */
#define JPEG12_SCAN_COMPLETED	4 /* Completed last iMCU row of a scan */

/* Precalculate output dimensions for current decompression parameters. */
EXTERN(void) jpeg12_core_output_dimensions JPP((j12_decompress_ptr cinfo));
EXTERN(void) jpeg12_calc_output_dimensions JPP((j12_decompress_ptr cinfo));

/* Control saving of COM and APPn markers into marker_list. */
EXTERN(void) jpeg12_save_markers
	JPP((j12_decompress_ptr cinfo, int marker_code,
	     unsigned int length_limit));

/* Install a special processing method for COM or APPn markers. */
EXTERN(void) jpeg12_set_marker_processor
	JPP((j12_decompress_ptr cinfo, int marker_code,
	     jpeg12_marker_parser_method routine));

/* Read or write raw DCT coefficients --- useful for lossless transcoding. */
EXTERN(jvirt_barray_ptr *) jpeg12_read_coefficients JPP((j12_decompress_ptr cinfo));
EXTERN(void) jpeg12_write_coefficients JPP((j12_compress_ptr cinfo,
					  jvirt_barray_ptr * coef_arrays));
EXTERN(void) jpeg12_copy_critical_parameters JPP((j12_decompress_ptr srcinfo,
						j12_compress_ptr dstinfo));

/* If you choose to abort compression or decompression before completing
 * jpeg12_finish_(de)compress, then you need to clean up to release memory,
 * temporary files, etc.  You can just call jpeg12_destroy_(de)compress
 * if you're done with the JPEG object, but if you want to clean it up and
 * reuse it, call this:
 */
EXTERN(void) jpeg12_abort_compress JPP((j12_compress_ptr cinfo));
EXTERN(void) jpeg12_abort_decompress JPP((j12_decompress_ptr cinfo));

/* Generic versions of jpeg12_abort and jpeg12_destroy that work on either
 * flavor of JPEG object.  These may be more convenient in some places.
 */
EXTERN(void) jpeg12_abort JPP((j12_common_ptr cinfo));
EXTERN(void) jpeg12_destroy JPP((j12_common_ptr cinfo));

/* Default restart-marker-resync procedure for use by data source modules */
EXTERN(boolean) jpeg12_j12_resync_to_restart JPP((j12_decompress_ptr cinfo,
					    int desired));


/* These marker codes are exported since applications and data source modules
 * are likely to want to use them.
 */

#define JPEG12_RST0	0xD0	/* RST0 marker code */
#define JPEG12_EOI	0xD9	/* EOI marker code */
#define JPEG12_APP0	0xE0	/* APP0 marker code */
#define JPEG12_COM	0xFE	/* COM marker code */


/* If we have a brain-damaged compiler that emits warnings (or worse, errors)
 * for structure definitions that are never filled in, keep it quiet by
 * supplying dummy definitions for the various substructures.
 */

#ifdef INCOMPLETE_TYPES_BROKEN
#ifndef JPEG12_INTERNALS		/* will be defined in jpegint.h */
struct jvirt_sarray_control { long dummy; };
struct jvirt_barray_control { long dummy; };
struct jpeg12_comp_master { long dummy; };
struct jpeg12_c_main_controller { long dummy; };
struct jpeg12_c_prep_controller { long dummy; };
struct jpeg12_c_coef_controller { long dummy; };
struct jpeg12_marker_writer { long dummy; };
struct jpeg12_j12_color_converter { long dummy; };
struct jpeg12_j12_downsampler { long dummy; };
struct jpeg12_forward_dct { long dummy; };
struct jpeg12_entropy_encoder { long dummy; };
struct jpeg12_decomp_master { long dummy; };
struct jpeg12_d_main_controller { long dummy; };
struct jpeg12_d_coef_controller { long dummy; };
struct jpeg12_d_post_controller { long dummy; };
struct jpeg12_input_controller { long dummy; };
struct jpeg12_marker_reader { long dummy; };
struct jpeg12_entropy_decoder { long dummy; };
struct jpeg12_inverse_dct { long dummy; };
struct jpeg12_j12_upsampler { long dummy; };
struct jpeg12_color_deconverter { long dummy; };
struct jpeg12_j12_color_quantizer { long dummy; };
#endif /* JPEG12_INTERNALS */
#endif /* INCOMPLETE_TYPES_BROKEN */


/*
 * The JPEG library modules define JPEG12_INTERNALS before including this file.
 * The internal structure declarations are read only when that is true.
 * Applications using the library should not include jpegint.h, but may wish
 * to include jerror.h.
 */

#ifdef JPEG12_INTERNALS
#include "jpegint.h"		/* fetch private declarations */
#include "jerror.h"		/* fetch error codes too */
#endif

#ifdef __cplusplus
#ifndef DONT_USE_EXTERN_C
}
#endif
#endif

#endif /* JPEGLIB_H */
