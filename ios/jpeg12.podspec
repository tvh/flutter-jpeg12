#
Pod::Spec.new do |s|
  s.name             = 'jpeg12'
  s.version          = '0.1.0'
  s.summary          = 'jpeg12'
  s.homepage         = 'https://www.github.com/tvh/flutter-jpeg12'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Timo von Holtz' => 'tvh@tvholtz.de' }
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*', 'jpeg-9/*.{h,c}'
  s.dependency 'Flutter'
  s.platform = :ios, '9.0'

  # Flutter.framework does not contain a i386 slice.
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES', 'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386' }
  s.swift_version = '5.0'
end
