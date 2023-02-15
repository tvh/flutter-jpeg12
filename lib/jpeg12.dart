library libjpeg12;

import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:math';
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:ffi/ffi.dart';
import 'package:flutter/material.dart';

import 'package:jpeg12/generated_bindings.dart';

const _NUM_DECODE_ROWS = 16;
const _NUM_BITS = 12;

final Jpeg12Native _lib = Jpeg12Native(Platform.isAndroid
    ? DynamicLibrary.open('liblibjpeg.so')
    : DynamicLibrary.process());

class Jpeg12BitImage {
  final int height;
  final int width;

  final Uint16List _pixelData;

  final int minVal;
  final int maxVal;

  Jpeg12BitImage._({
    required this.height,
    required this.width,
    required Uint16List data,
    required this.minVal,
    required this.maxVal,
  }) : _pixelData = data;

  static Jpeg12BitImage decode(Uint8List input) {
    Pointer<jpeg12_decompress_struct> cinfo = nullptr;
    Pointer<jpeg12_error_mgr> jerr = nullptr;
    Pointer<JSAMPROW> row_pointer = nullptr;
    JSAMPROW rowptr = nullptr;
    Pointer<UnsignedChar> inbuffer = nullptr;

    try {
      cinfo = calloc();
      jerr = calloc();

      _lib.jpeg12_CreateDecompress(
          cinfo, JPEG12_LIB_VERSION, sizeOf<jpeg12_decompress_struct>());
      cinfo.ref.err = _lib.jpeg12_std_error(jerr);

      row_pointer = calloc.allocate(_NUM_DECODE_ROWS * sizeOf<JSAMPROW>());
      inbuffer = calloc.allocate(input.length);
      inbuffer.cast<Uint8>().asTypedList(input.length).setAll(0, input);

      // TODO: Error function

      _lib.jpeg12_mem_src(cinfo, inbuffer, input.length);
      if (_lib.jpeg12_read_header(cinfo, 1) != JPEG12_HEADER_OK) {
        throw Exception("Error reading JPEG header");
      }

      if (cinfo.ref.data_precision != _NUM_BITS) {
        throw Exception("JPEG not using 12 bit precision!");
      }

      if (cinfo.ref.num_components != 1) {
        throw Exception("Not a grayscale jpeg picture!");
      }

      _lib.jpeg12_start_decompress(cinfo);

      int numPixels = cinfo.ref.output_width * cinfo.ref.output_height;
      final res = Uint16List(numPixels);
      final rowsize = cinfo.ref.output_width * sizeOf<JSAMPLE>();
      rowptr = calloc.allocate(rowsize * _NUM_DECODE_ROWS);
      for (int i = 0; i < _NUM_DECODE_ROWS; i++) {
        row_pointer[i] = Pointer.fromAddress(rowptr.address + (i * rowsize));
      }
      int minVal = 4095;
      int maxVal = 0;
      while (cinfo.ref.output_scanline < cinfo.ref.image_height) {
        int currentRow = cinfo.ref.output_scanline;
        int read =
            _lib.jpeg12_read_scanlines(cinfo, row_pointer, _NUM_DECODE_ROWS);
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
      return Jpeg12BitImage._(
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
}

class _Jpeg12Painter extends CustomPainter {
  /// The buffer as as [ui.Image]. This image needs to be combined with
  /// the [ui.ColorFilter] from [_filterForWindow].
  final ui.Image? imageData;
  final double windowMin;
  final double windowMax;
  final ui.FilterQuality filterQuality;

  _Jpeg12Painter(
    this.imageData,
    this.windowMin,
    this.windowMax,
    this.filterQuality,
  );

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

  /// Use this function to extract the image data needed for this widget.
  static Future<ui.Image> _imageDataFromJpeg12(Jpeg12BitImage image) {
    final imageCompleter = Completer<ui.Image>();
    final inputBuffer = Uint32List(image._pixelData.length);
    for (int i = 0; i < image._pixelData.length; i++) {
      final x = image._pixelData[i];
      inputBuffer[i] = Color.fromRGBO(0, x >> 8, x, 1).value;
    }
    ui.decodeImageFromPixels(
      inputBuffer.buffer.asUint8List(),
      image.width,
      image.height,
      ui.PixelFormat.bgra8888, // RGBA in Big-endian
      (ui.Image img) {
        imageCompleter.complete(img);
      },
    );
    return imageCompleter.future;
  }

  @override
  void paint(ui.Canvas canvas, ui.Size size) {
    if (imageData != null) {
      final paint = Paint()
        ..colorFilter = _filterForWindow(windowMin, windowMax)
        ..filterQuality = filterQuality;
      canvas.scale(
        size.width / imageData!.width,
        size.height / imageData!.height,
      );
      canvas.drawImage(imageData!, Offset.zero, paint);
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) {
    return oldDelegate is! _Jpeg12Painter ||
        oldDelegate.imageData != imageData ||
        oldDelegate.windowMin != windowMin ||
        oldDelegate.windowMax != windowMax ||
        oldDelegate.filterQuality != filterQuality;
  }
}

class Jpeg12BitWidget extends StatefulWidget {
  final Uint8List input;
  final double? windowMin;
  final double? windowMax;
  final ui.FilterQuality filterQuality;

  const Jpeg12BitWidget({
    Key? key,
    required this.input,
    this.windowMin,
    this.windowMax,
    this.filterQuality = ui.FilterQuality.medium,
  }) : super(key: key);

  @override
  State<Jpeg12BitWidget> createState() => _Jpeg12BitWidgetState();
}

class _Jpeg12BitWidgetState extends State<Jpeg12BitWidget> {
  late Jpeg12BitImage _decoded;
  ui.Image? _currentImage;

  void _replaceCurrentImage() {
    _decoded = Jpeg12BitImage.decode(widget.input);
    _currentImage = null;
    _Jpeg12Painter._imageDataFromJpeg12(_decoded).then((imageData) {
      setState(() {
        _currentImage = imageData;
      });
    });
  }

  @override
  void initState() {
    _replaceCurrentImage();
    super.initState();
  }

  @override
  void didUpdateWidget(covariant Jpeg12BitWidget oldWidget) {
    if (oldWidget.input != widget.input) {
      setState(() {
        _replaceCurrentImage();
      });
    }
    super.didUpdateWidget(oldWidget);
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (context, constraints) {
      final baseSize = ui.Size(
        _decoded.width.toDouble(),
        _decoded.height.toDouble(),
      );
      final size = constraints.constrainSizeAndAttemptToPreserveAspectRatio(
        baseSize,
      );
      return CustomPaint(
        size: size,
        painter: _Jpeg12Painter(
          _currentImage,
          widget.windowMin ?? _decoded.minVal.toDouble(),
          widget.windowMax ?? _decoded.maxVal.toDouble(),
          widget.filterQuality,
        ),
      );
    });
  }
}
