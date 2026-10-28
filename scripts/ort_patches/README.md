# ONNX Runtime patches

Applied by the build scripts to the ONNX Runtime 1.30.0 checkout before
building.

- `minimal_interop_release.patch`: in a minimal build, `interop_api.cc` leaves
  out the three `Release*` functions of the interop API although its function
  table still points at them. Linkers that load the file fail with undefined
  symbols; the patch defines them as no-ops for minimal builds, as the other
  interop functions already are.
