import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:jpeg12/jpeg12.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  Uint8List? img;
  double windowMin = 0;
  double windowMax = 4095;

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
              values: RangeValues(windowMin.toDouble(), windowMax.toDouble()),
              onChanged: (values) {
                setState(() {
                  windowMin = values.start;
                  windowMax = values.end;
                });
              },
              min: 0,
              max: 4095,
            ),
          ],
        ),
      ),
    );
  }
}
