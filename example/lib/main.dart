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
  int windowMin = 0;
  int windowMax = 4095;

  void _doLoad() async {
    final bytes =
        await rootBundle.load('MR-MONO2-12-shoulder_reference_small.jpg');
    this.setState(() {
      this.img = bytes.buffer.asUint8List();
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
            body: Column(
              children: [
                if (img != null)
                  Jpeg12BitWidget(
                    input: img!,
                    windowMin: windowMin,
                    windowMax: windowMax,
                  ),
                RangeSlider(
                  values:
                      RangeValues(windowMin.toDouble(), windowMax.toDouble()),
                  onChanged: (values) {
                    setState(() {
                      windowMin = values.start.floor();
                      windowMax = values.end.ceil();
                    });
                  },
                  min: 0,
                  max: 4095,
                ),
              ],
            )));
  }
}
