library libjpeg12;

import 'dart:ffi';
import 'dart:math';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';

import 'package:libjpeg12/generated_bindings.dart';

const NUM_DECODE_ROWS = 16;
const NUM_BITS = 12;

class Jpeg12BitImage {
  final int height;
  final int width;
  final Uint16List data;

  static late final NativeLibrary _lib;

  static initialize(NativeLibrary lib) {
    _lib = lib;
  }

  Jpeg12BitImage(
      {required this.height, required this.width, required this.data});

  static Jpeg12BitImage decodeImage(Uint8List input) {
    Pointer<jpeg12_decompress_struct> cinfo = nullptr;
    Pointer<jpeg12_error_mgr> jerr = nullptr;
    Pointer<JSAMPROW> row_pointer = nullptr;
    JSAMPROW rowptr = nullptr;
    Pointer<Uint8> inbuffer = nullptr;

    try {
      cinfo = calloc();
      jerr = calloc();

      _lib.jpeg12_CreateDecompress(
          cinfo, JPEG12_LIB_VERSION, sizeOf<jpeg12_decompress_struct>());
      cinfo.ref.err = _lib.jpeg12_std_error(jerr);

      row_pointer = calloc.allocate(NUM_DECODE_ROWS * sizeOf<JSAMPROW>());
      inbuffer = calloc.allocate(input.length);
      inbuffer.asTypedList(input.length).setAll(0, input);

      // TODO: Error function

      _lib.jpeg12_mem_src(cinfo, inbuffer, input.length);
      if (_lib.jpeg12_read_header(cinfo, 1) != JPEG12_HEADER_OK) {
        throw Exception("Error reading JPEG header");
      }

      if (cinfo.ref.data_precision != NUM_BITS) {
        throw Exception("JPEG not using 12 bit precision!");
      }

      if (cinfo.ref.num_components != 1) {
        throw Exception("Not a grayscale jpeg picture!");
      }

      _lib.jpeg12_start_decompress(cinfo);

      int numPixels = cinfo.ref.output_width * cinfo.ref.output_height;
      final res = Uint16List(numPixels);
      final rowsize = cinfo.ref.output_width * sizeOf<JSAMPLE>();
      rowptr = calloc.allocate(rowsize * NUM_DECODE_ROWS);
      for (int i = 0; i < NUM_DECODE_ROWS; i++) {
        row_pointer[i] = Pointer.fromAddress(rowptr.address + (i * rowsize));
      }
      int minVal = 4095;
      int maxVal = 0;
      while (cinfo.ref.output_scanline < cinfo.ref.image_height) {
        int currentRow = cinfo.ref.output_scanline;
        int read =
            _lib.jpeg12_read_scanlines(cinfo, row_pointer, NUM_DECODE_ROWS);
        if (read == 0) {
          throw Exception("Error decoding JPEG");
        }
        for (int i = 0; i < read; i++) {
          for (int j = 0; j < cinfo.ref.output_width; j++) {
            int p = (currentRow + i) * cinfo.ref.output_width + j;
            int x = row_pointer[i][j];
            minVal = min(minVal, x);
            maxVal = max(maxVal, x);
            res[p] = x;
          }
        }
      }

      _lib.jpeg12_finish_decompress(cinfo);
      return Jpeg12BitImage(
        height: cinfo.ref.image_height,
        width: cinfo.ref.image_width,
        data: res,
      );
    } finally {
      _lib.jpeg12_destroy_decompress(cinfo);
      calloc.free(cinfo);
      calloc.free(jerr);
      calloc.free(row_pointer);
      calloc.free(inbuffer);
      calloc.free(rowptr);
    }
  }
}
