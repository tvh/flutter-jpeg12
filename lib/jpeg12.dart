library libjpeg12;

import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:math';
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

import 'package:jpeg12/generated_bindings.dart';

const NUM_DECODE_ROWS = 16;
const NUM_BITS = 12;

final Jpeg12Native _lib = Jpeg12Native(Platform.isAndroid
    ? DynamicLibrary.open('liblibjpeg.so')
    : DynamicLibrary.process());

class Jpeg12BitImage {
  final int height;
  final int width;

  /// The buffer as as [ui.Image]. This image needs to be combined with
  /// the [ui.ColorFilter] from [_filterForWindow] _without_ scaling (or with
  /// [FilterQuality.none]).
  final Future<ui.Image> _data;

  final int minVal;
  final int maxVal;

  Jpeg12BitImage._({
    required this.height,
    required this.width,
    required Future<ui.Image> data,
    required this.minVal,
    required this.maxVal,
  }) : _data = data;

  static Jpeg12BitImage decode(Uint8List input) {
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
      final res = Uint32List(numPixels);
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
            res[p] = Color.fromRGBO(0, x >> 8, x, 1).value;
          }
        }
      }

      _lib.jpeg12_finish_decompress(cinfo);
      final imageCompleter = Completer<ui.Image>();
      ui.decodeImageFromPixels(
        res.buffer.asUint8List(),
        cinfo.ref.image_width,
        cinfo.ref.image_height,
        ui.PixelFormat.bgra8888, // RGBA in Big-endian
        (ui.Image img) {
          imageCompleter.complete(img);
        },
      );
      return Jpeg12BitImage._(
        height: cinfo.ref.image_height,
        width: cinfo.ref.image_width,
        data: imageCompleter.future,
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

  /// Use this filter to get the final image.
  static ui.ColorFilter _filterForWindow(double windowMin, double windowMax) {
    final windowWidth = windowMax - windowMin;
    final scaleFactor = 255 / windowWidth;
    final List<double> selector = [
      0,
      scaleFactor * 256,
      scaleFactor,
      0,
      -(scaleFactor * windowMin)
    ];
    return ColorFilter.matrix([
      ...selector,
      ...selector,
      ...selector,
      ...[0, 0, 0, 0, 255],
    ]);
  }

  Future<void> drawToCanvas(
      ui.Canvas canvas, double windowMin, double windowMax) async {
    final paint = Paint()
      ..colorFilter = Jpeg12BitImage._filterForWindow(windowMin, windowMax)
      ..filterQuality = ui.FilterQuality.none;
    canvas.drawImage(await _data, Offset.zero, paint);
  }
}

class Jpeg12ImageProvider extends ImageProvider<Jpeg12ImageProvider> {
  final Jpeg12BitImage image;
  final double windowMin;
  final double windowMax;

  Jpeg12ImageProvider({
    required this.image,
    required this.windowMin,
    required this.windowMax,
  });

  @override
  Future<Jpeg12ImageProvider> obtainKey(ImageConfiguration configuration) {
    return SynchronousFuture<Jpeg12ImageProvider>(this);
  }

  @override
  ImageStreamCompleter load(Jpeg12ImageProvider key, DecoderCallback decode) {
    return OneFrameImageStreamCompleter(_processWindow());
  }

  Future<ImageInfo> _processWindow() async {
    final recorder = ui.PictureRecorder();
    final canvas = ui.Canvas(recorder);
    await image.drawToCanvas(canvas, windowMin, windowMax);
    final picture = recorder.endRecording();
    final resImage = await picture.toImage(image.width, image.height);
    return ImageInfo(image: resImage);
  }

  @override
  operator ==(Object other) {
    return other is Jpeg12ImageProvider &&
        other.image == image &&
        other.windowMax == windowMax &&
        other.windowMin == windowMin;
  }

  @override
  int get hashCode => image.hashCode;
}

class Jpeg12BitWidget extends StatefulWidget {
  final Uint8List input;
  final double? windowMin;
  final double? windowMax;
  final String? semanticLabel;
  final bool excludeFromSemantics;
  final double? width;
  final double? height;
  final BoxFit? fit;
  final Alignment alignment;
  final ImageRepeat repeat;
  final Rect? centerSlice;
  final bool matchTextDirection;
  final FilterQuality filterQuality;

  const Jpeg12BitWidget({
    Key? key,
    required this.input,
    this.windowMin,
    this.windowMax,
    this.semanticLabel,
    this.excludeFromSemantics = false,
    this.width,
    this.height,
    this.fit,
    this.alignment = Alignment.center,
    this.repeat = ImageRepeat.noRepeat,
    this.centerSlice,
    this.matchTextDirection = false,
    this.filterQuality = ui.FilterQuality.high,
  }) : super(key: key);

  @override
  State<Jpeg12BitWidget> createState() => _Jpeg12BitWidgetState();
}

class _Jpeg12BitWidgetState extends State<Jpeg12BitWidget> {
  late Jpeg12BitImage decoded;

  @override
  void initState() {
    decoded = Jpeg12BitImage.decode(widget.input);
    super.initState();
  }

  @override
  void didUpdateWidget(covariant Jpeg12BitWidget oldWidget) {
    if (oldWidget.input != widget.input) {
      setState(() {
        decoded = Jpeg12BitImage.decode(widget.input);
      });
    }
    super.didUpdateWidget(oldWidget);
  }

  @override
  Widget build(BuildContext context) {
    final double windowMax = widget.windowMax ?? decoded.maxVal.toDouble();
    final double windowMin = widget.windowMin ?? decoded.minVal.toDouble();
    return Image(
      image: Jpeg12ImageProvider(
        image: decoded,
        windowMin: windowMin,
        windowMax: windowMax,
      ),
      // Set this here so that scrolling through different windowing values
      // doesn't cut out the display.
      gaplessPlayback: true,
      filterQuality: widget.filterQuality,
      semanticLabel: widget.semanticLabel,
      excludeFromSemantics: widget.excludeFromSemantics,
      width: widget.width,
      height: widget.height,
      fit: widget.fit,
      alignment: widget.alignment,
      repeat: widget.repeat,
      centerSlice: widget.centerSlice,
      matchTextDirection: widget.matchTextDirection,
    );
  }
}
