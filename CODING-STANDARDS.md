# C++ coding standards

Apply these rules to project-owned C++ headers, implementations and tests.
Keep supplied fixtures unchanged. These style rules do not apply to third-party
repositories. When explicitly authorized to patch a dependency or its fork,
follow that repository's contribution guide, formatting and existing conventions,
including member access and source suffixes; keep the patch focused.
Vellum-specific architectural requirements are in `PROJECT-ARCHITECTURE.md`.

- All project C++ targets and tests compile with `-Wall -Werror` on GCC/Clang.
  Fix warning causes; do not suppress warnings to get a passing build.
- Use C++23, `.cc` for C++ implementation files and `.hh` for C++ headers
  in `include/vellum/` (including the generated `version.hh`).
- Do not append `_` to member names.
- Arrange class and struct sections in this order: `public`, `protected`,
  `private`. Omit unused sections. Within each section, put methods before
  data members. Put necessary type declarations before their first use.
- Preserve dependencies between data members when ordering declarations:
  initialization follows declaration order, and destruction reverses it.
- Always use explicit `this->` when accessing a non-static data member or
  calling a non-static method of the current object, including inside lambdas
  that capture `this`. Constructor member-initializer names use normal C++
  syntax. Access another object's members through that object.
- Always brace the bodies of `if`, `else`, loops and other control statements,
  even when the body contains only one statement. An `else if` chain is fine.
- Never use an anonymous lambda spanning more than one line. Give multiline
  lambdas descriptive names with `auto`, then pass or assign those names at
  their use sites. Small anonymous lambdas are allowed when the entire lambda
  fits on one line after formatting. A named method or function is also
  appropriate when it makes the operation clearer.
- Choose identifiers that explain their purpose to someone fluent in C++.
  Prefer clearer names or a small refactoring over comments explaining what
  ordinary code does. Apply this to types, methods, members, parameters,
  variables and aliases.
- Use judgment rather than expanding every name. Familiar conventions such
  as Cartesian `x`, `y`, `z`, loop indices `i`, `j`, `k`, and an ordinary
  `impl` pointer are appropriate when their meaning is clear in context.
- Functions and methods should be short enough to fit on one screen. If a
  function or method is too long to fit on one screen, break it into smaller
  functions or methods with descriptive names. Avoid long parameter lists;
  prefer a small struct or class to group related parameters.
- Do not use a weak `lock()` within a subexpression such as an `if`. Store
  the lock result locally in a shared pointer and check it before use. Passing
  a lock result to a function is acceptable because the temporary shared pointer
  stays alive through the call. This is an explicit ownership/readability rule:
  C++ also keeps a temporary shared pointer alive through its full expression;
  another thread cannot invalidate a successfully acquired strong reference.
- Make callback ownership explicit. Break shared-pointer cycles when a dialog
  owner is destroyed; disconnect its field callbacks before destroying the view.
  Use weak or proven non-owning references for callbacks without such an owner.
  Verify control release with weak-reference lifetime tests when ownership changes.
- Treat human-readable `std::string` text as UTF-8. Byte counts/offsets are
  not Unicode codepoint counts or user-perceived character (grapheme) positions.
  Preserve multibyte sequences, convert explicitly at UTF-16/API boundaries and
  validate external text where required.
- Prefer standard-library facilities and algorithms to hand-written versions
  of the same operation. Keep necessary domain-specific behavior explicit.
- Keep implementation files focused on one responsibility. Separate document
  operations, rendering, UI construction, background work, serialization and
  test drivers as those areas grow. Split large dispatch functions into named
  operations with explicit inputs; preserve transaction and lifetime boundaries.
  Keep private declarations small and implementations in `.cc` files rather than
  moving a monolith into a header or an included implementation fragment.
  Judge size by cohesion and readability, without an arbitrary line limit or
  unnecessary fragmentation of otherwise coherent headers.
- Keep comments that explain constraints, ownership, concurrency, units or
  reasons that the code itself cannot convey. Avoid comments that merely
  repeat the names or statements.
- Functions should generally fit on a screen, and god classes are to be avoided.
  Delegate responsibility as appropriate.

Use the repository's `.clang-format` configuration. Build and run the tests
after refactoring, including the Ubuntu Docker build. Run latency benchmarks
only in Release with `-O3`. Stage new source files in Git with the rest of the change.

Keep generated build, test and runtime logs out of Git. Store them in ignored
work/build directories; references in HANDOFF may identify local diagnostic logs.

Testing is mandatory for every implemented behavior: positive, negative, boundary
and adversarial paths. Follow AGENT-CONTRACT.md and IMPLEMENTATION-REVIEW.md.
