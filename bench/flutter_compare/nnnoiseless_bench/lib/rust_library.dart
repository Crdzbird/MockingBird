import 'dart:io';

import 'package:flutter_nnnoiseless/src/rust/frb_generated.dart';
import 'package:flutter_rust_bridge/flutter_rust_bridge_for_generated.dart';

class RustLibrary {
  const RustLibrary();

  Future<void> load() async {
    if (RustLib.instance.initialized) return;
    final contents = File(Platform.resolvedExecutable).parent.parent.path;
    final path =
        '$contents/Frameworks/flutter_nnnoiseless.framework/flutter_nnnoiseless';
    await RustLib.init(externalLibrary: ExternalLibrary.open(path));
  }
}
