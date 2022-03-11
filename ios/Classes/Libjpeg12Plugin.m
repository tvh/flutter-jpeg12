#import "Libjpeg12Plugin.h"
#if __has_include(<libjpeg12/libjpeg12-Swift.h>)
#import <libjpeg12/libjpeg12-Swift.h>
#else
// Support project import fallback if the generated compatibility header
// is not copied when this plugin is created as a library.
// https://forums.swift.org/t/swift-static-libraries-dont-copy-generated-objective-c-header/19816
#import "libjpeg12-Swift.h"
#endif

@implementation Libjpeg12Plugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  [SwiftLibjpeg12Plugin registerWithRegistrar:registrar];
}
@end
