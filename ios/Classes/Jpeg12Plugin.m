#import "Jpeg12Plugin.h"
#if __has_include(<jpeg12/jpeg12-Swift.h>)
#import <jpeg12/jpeg12-Swift.h>
#else
// Support project import fallback if the generated compatibility header
// is not copied when this plugin is created as a library.
// https://forums.swift.org/t/swift-static-libraries-dont-copy-generated-objective-c-header/19816
#import "jpeg12-Swift.h"
#endif

@implementation Jpeg12Plugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  [SwiftJpeg12Plugin registerWithRegistrar:registrar];
}
@end
