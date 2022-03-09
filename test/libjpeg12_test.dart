import 'dart:ffi';
import 'dart:io';

import 'package:libjpeg12/generated_bindings.dart';
import 'package:libjpeg12/libjpeg12.dart';
import 'package:test/test.dart';

void main() {
  final Directory current = Directory.current;
  Jpeg12BitImage.initialize(NativeLibrary(
      DynamicLibrary.open(current.path + '/jpeg-9/liblibjpeg.dylib')));

  test('smoke', () {
    final img = Jpeg12BitImage.decodeImage(
        File('test/MR-MONO2-12-shoulder_reference_small.jpg')
            .readAsBytesSync());
  });
}
