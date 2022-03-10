import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:libjpeg12/generated_bindings.dart';
import 'package:libjpeg12/libjpeg12.dart';
import 'dart:ffi';
import 'dart:io';
import 'dart:ui' as ui;

void main() {
  final DynamicLibrary nativeAddLib = Platform.isAndroid
      ? DynamicLibrary.open('liblibjpeg.so')
      : DynamicLibrary.process();
  Jpeg12BitImage.initialize(NativeLibrary(nativeAddLib));
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  Uint8List? img;

  void _doLoad() async {
    final bytes =
        await rootBundle.load('MR-MONO2-12-shoulder_reference_small.jpg');
    final imgRaw = Jpeg12BitImage.decodeImage(bytes.buffer.asUint8List());
    final img = await imgRaw.toImage();
    final imgData = await img.toByteData(format: ui.ImageByteFormat.png);
    this.setState(() {
      this.img = Uint8List.view(imgData!.buffer);
    });
  }

  @override
  void initState() {
    super.initState();
    _doLoad();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
        home: Scaffold(
      appBar: AppBar(
        title: const Text('Plugin example app'),
      ),
      body: img != null ? Image(image: MemoryImage(img!)) : null,
    ));
  }
}
