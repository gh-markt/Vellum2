# Dependency patches

`v8-icu79-unicode18.patch` adapts V8 upstream commit
https://github.com/v8/v8/commit/f607706154eabc4a949628a88b38dbe95cd2274c
to the pinned V8 13.1 source. Unicode 18 adds a case-folding relationship
between BMP and supplementary characters. The pinned generator otherwise aborts
with ICU 79, and runtime Unicode and non-Unicode regular expressions require
different treatment. The embedded-V8 integration test exercises both modes
when the installed ICU supplies the new Unicode mapping.
It also supplies a missing standard integer header needed by newer toolchains.

This is a local backport, not a claim that all upstream V8 Unicode issues are
resolved. In particular, the upstream change notes that case-insensitive
backreferences spanning different UTF-16 widths remain unsupported.
