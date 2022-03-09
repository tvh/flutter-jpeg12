import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:libjpeg12/generated_bindings.dart';
import 'package:libjpeg12/libjpeg12.dart';
import 'dart:ffi';
import 'dart:io';

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
  Jpeg12BitImage? img;

  void _doLoad() async {
    final bytes =
        await rootBundle.load('MR-MONO2-12-shoulder_reference_small.jpg');
    this.setState(() {
      img = Jpeg12BitImage.decodeImage(bytes.buffer.asUint8List());
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
        body: Text(
          img?.data.toString() ?? '',
        ),
      ),
    );
  }
}
