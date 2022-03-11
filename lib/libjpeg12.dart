library libjpeg12;

import 'dart:async';
import 'dart:ffi';
import 'dart:math';
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/painting.dart';

import 'package:libjpeg12/generated_bindings.dart';

const NUM_DECODE_ROWS = 16;
const NUM_BITS = 12;

class Jpeg12BitImage {
  final int height;
  final int width;
  final Uint16List data;
  final int minVal;
  final int maxVal;

  static late final NativeLibrary _lib;

  static initialize(NativeLibrary lib) {
    _lib = lib;
  }

  Jpeg12BitImage({
    required this.height,
    required this.width,
    required this.data,
    required this.minVal,
    required this.maxVal,
  });

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
        minVal: minVal,
        maxVal: maxVal,
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

  Future<ui.Image> toImage({required int windowMin, required int windowMax}) {
    assert(windowMax > windowMin);
    assert(windowMin >= 0 && windowMin < 4095);
    assert(windowMax >= 0 && windowMax < 4095);
    final windowWidth = windowMax - windowMin;

    Int32List pixels = Int32List(width * height);
    for (var y = 0; y < height; y++) {
      final rowBegin = y * width;
      for (var x = 0; x < width; x++) {
        final int index = rowBegin + x;
        final rawValue = data[index];
        final shiftedVal = rawValue - windowMin;
        final int val = ((shiftedVal << 8) / windowWidth).floor();
        pixels[index] = Color.fromRGBO(val, val, val, 1).value;
      }
    }

    final completer = Completer<ui.Image>();
    ui.decodeImageFromPixels(
      pixels.buffer.asUint8List(),
      width,
      height,
      ui.PixelFormat.bgra8888,
      (ui.Image img) {
        completer.complete(img);
      },
    );

    return completer.future;
  }
}

class _Jpeg12ImageProvider extends ImageProvider<_Jpeg12ImageProvider> {
  final Jpeg12BitImage image;
  final int windowMin;
  final int windowMax;

  _Jpeg12ImageProvider(this.image, {int? windowMin, int? windowMax})
      : windowMin = windowMin ?? image.minVal,
        windowMax = windowMax ?? image.maxVal;

  @override
  Future<_Jpeg12ImageProvider> obtainKey(ImageConfiguration configuration) {
    return SynchronousFuture<_Jpeg12ImageProvider>(this);
  }

  @override
  ImageStreamCompleter load(_Jpeg12ImageProvider key, DecoderCallback decode) {
    return OneFrameImageStreamCompleter(
      image
          .toImage(windowMin: windowMin, windowMax: windowMax)
          .then((img) => ImageInfo(image: img)),
    );
  }

  @override
  bool operator ==(Object other) {
    if (other.runtimeType != runtimeType) return false;
    return other is _Jpeg12ImageProvider &&
        other.image == image &&
        other.windowMin == windowMin &&
        other.windowMax == windowMax;
  }

  @override
  int get hashCode => ui.hashValues(image, windowMin, windowMax);
}

class Jpeg12BitWidget extends StatefulWidget {
  final Uint8List input;
  final int? windowMin;
  final int? windowMax;

  const Jpeg12BitWidget({
    Key? key,
    required this.input,
    this.windowMin,
    this.windowMax,
  }) : super(key: key);

  @override
  State<Jpeg12BitWidget> createState() => _Jpeg12BitWidgetState();
}

class _Jpeg12BitWidgetState extends State<Jpeg12BitWidget> {
  late Jpeg12BitImage decoded;

  @override
  void initState() {
    decoded = Jpeg12BitImage.decodeImage(widget.input);
    super.initState();
  }

  @override
  void didUpdateWidget(covariant Jpeg12BitWidget oldWidget) {
    if (oldWidget.input != widget.input) {
      setState(() {
        decoded = Jpeg12BitImage.decodeImage(widget.input);
      });
    }
    super.didUpdateWidget(oldWidget);
  }

  @override
  Widget build(BuildContext context) {
    return Image(
      image: _Jpeg12ImageProvider(
        decoded,
        windowMin: widget.windowMin,
        windowMax: widget.windowMax,
      ),
      filterQuality: ui.FilterQuality.high,
    );
  }
}
